#include "strategy/transformer_strategy.h"
#include "common/utils.h"
#include "strategy/transformer_model.h" // For the model class definition
#include "common/feature_sequence_manager.h" // NEW: For proper sequence management
#include "features/feature_config_standard.h" // 🔧 CONSOLIDATED: Standard 91-feature config
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <vector>

namespace sentio {

// =============================================================================
// Module: src/strategy/transformer_strategy.cpp (Rewritten)
// Purpose: Implements the high-performance inference logic for the TFM strategy.
// =============================================================================

TransformerStrategy::TransformerStrategy(const StrategyConfig& base_config, const Config& tfm_config)
    : MLStrategyBase(base_config), config_(tfm_config) {}

bool TransformerStrategy::initialize() {
    utils::log_info("Initializing Rewritten Transformer Strategy (v2)...");

    // 1. Load metadata to configure the strategy and normalization parameters.
    if (!load_metadata()) {
        utils::log_error("Failed to load Transformer metadata from: " + config_.metadata_path);
        return false;
    }
    
    // 2. 🔧 CONSOLIDATED: Initialize with standardized 91-feature configuration
    //    This ensures identical features between TFM, PPO, and all future ML strategies
    auto feature_config = features::get_standard_91_feature_config();
    if (!features::validate_91_feature_config(feature_config)) {
        utils::log_error("❌ CRITICAL: Standard feature configuration validation failed!");
        return false;
    }
    feature_engine_ = std::make_unique<features::UnifiedFeatureEngine>(feature_config);
    
    // 🔍 DEBUG: Verify actual feature count matches expected (91)
    std::cout << "🔧 DEBUG: Configured UnifiedFeatureEngine with filtered features" << std::endl;
    std::cout << "   Expected feature count: " << model_config_.feature_dim << " (from metadata)" << std::endl;
    std::cout << "   Configured feature count: " << feature_config.total_features() << std::endl;
    std::cout << "   Statistical features: " << (feature_config.enable_statistical ? "enabled" : "disabled") << std::endl;
    std::cout << "   Price lag features: " << (feature_config.enable_price_lags ? "enabled" : "disabled") << std::endl;
    std::cout << "   Return lag features: " << (feature_config.enable_return_lags ? "enabled" : "disabled") << std::endl;
    
    // 3. Initialize the Feature Sequence Manager to fix the critical temporal bug
    sequence_manager_ = std::make_unique<FeatureSequenceManager>(
        model_config_.sequence_length, 
        model_config_.feature_dim
    );
    
    // 4. Load the complete model (full model format).
    //    Load the complete trained model directly.
    try {
        // Load the complete saved model (match training parameters from real data trainer)
        pytorch_model_ = std::make_unique<TransformerModel>(
            TransformerModelConfig{
                .feature_dim = model_config_.feature_dim,  // Now correctly 126 from metadata
                .sequence_length = model_config_.sequence_length,
                .d_model = 256,
                .nhead = 8,
                .num_encoder_layers = 4,  // ✅ FIXED: Match training script (was 6, should be 4)
                .dim_feedforward = 1024,
                .dropout = 0.1
            }
        );
        
        // Load the complete model (this will load both structure and weights)
        torch::load(*pytorch_model_, config_.model_path);
        
        (*pytorch_model_)->to(torch::kCPU);
        (*pytorch_model_)->eval();
        
        model_loaded_ = true;
        utils::log_info("Model loaded successfully (complete model format)");
    } catch (const std::exception& e) {
        std::cerr << "❌ DETAILED MODEL LOADING ERROR: " << e.what() << std::endl;
        std::cerr << "❌ Model path: " << config_.model_path << std::endl;
        std::cerr << "❌ Model config - feature_dim: " << model_config_.feature_dim << std::endl;
        std::cerr << "❌ Model config - sequence_length: " << model_config_.sequence_length << std::endl;
        utils::log_error("Failed to load model: " + std::string(e.what()));
        return false;
    }
    
    utils::log_info("Transformer Strategy v2 initialized successfully.");
    utils::log_info("Model: " + config_.model_path);
    return true;
}

bool TransformerStrategy::load_metadata() {
    std::cerr << "🔍 Attempting to load metadata from: " << config_.metadata_path << std::endl;
    std::ifstream metadata_file(config_.metadata_path);
    if (!metadata_file.is_open()) {
        std::cerr << "❌ METADATA ERROR: Cannot open metadata file: " << config_.metadata_path << std::endl;
        utils::log_error("Cannot open metadata file: " + config_.metadata_path);
        return false;
    }
    std::cerr << "✅ Metadata file opened successfully" << std::endl;
    
    try {
        nlohmann::json metadata;
        std::cerr << "🔍 Parsing JSON metadata..." << std::endl;
        metadata_file >> metadata;
        std::cerr << "✅ JSON parsed successfully" << std::endl;
        
        // Load model architecture parameters from metadata.
        model_config_.feature_dim = metadata["architecture"]["feature_dim"];
        model_config_.sequence_length = metadata["architecture"]["sequence_length"];
        
        // Load normalization statistics.
        auto means = metadata["normalization"]["means"].get<std::vector<float>>();
        auto stds = metadata["normalization"]["stds"].get<std::vector<float>>();
        
        // Store as Tensors for efficient computation during inference.
        feature_means_ = torch::tensor(means, torch::kFloat);
        feature_stds_ = torch::tensor(stds, torch::kFloat);

    } catch (const std::exception& e) {
        utils::log_error("Failed to parse metadata: " + std::string(e.what()));
        return false;
    }
    
    return true;
}

void TransformerStrategy::update_indicators(const Bar& bar) {
    // Update the feature engine with the latest market data.
    if (feature_engine_) {
        feature_engine_->update(bar);
        
        // NEW: Feed features into sequence manager for proper temporal handling
        if (feature_engine_->is_ready() && sequence_manager_) {
            auto current_features = feature_engine_->get_features();
            sequence_manager_->add_features(current_features);
        }
    }
}

SignalOutput TransformerStrategy::generate_signal(const Bar& bar, int bar_index) {
    static int call_count = 0;
    static int warmup_logged = 0;
    call_count++;
    
    SignalOutput signal;
    signal.timestamp_ms = bar.timestamp_ms;
    signal.bar_index = bar_index;
    signal.strategy_name = get_name();
    
    // Default to a neutral signal.
    signal.probability = 0.5;
    signal.confidence = 0.0;

    // 🔍 DIAGNOSTIC: Check each component's readiness state
    bool model_ready = model_loaded_;
    bool feature_ready = feature_engine_ && feature_engine_->is_ready();
    bool sequence_ready = sequence_manager_ && sequence_manager_->is_ready();
    
    // Log diagnostic info periodically during warmup
    if (!sequence_ready && warmup_logged < 70) {
        std::cout << "🔍 TFM Diagnostic [" << bar_index << "]: "
                  << "Model=" << (model_ready ? "✅" : "❌") 
                  << ", Features=" << (feature_ready ? "✅" : "❌")
                  << ", Sequence=" << (sequence_ready ? "✅" : "❌");
        if (sequence_manager_) {
            std::cout << " (History: " << sequence_manager_->get_history_size() 
                      << "/" << model_config_.sequence_length << ")";
        }
        if (feature_engine_) {
            std::cout << ", FeatureBars: " << feature_engine_->get_bar_count() << "/64";
        }
        std::cout << std::endl;
        warmup_logged++;
    }

    // Log when sequence manager becomes ready (first time)
    static bool sequence_readiness_logged = false;
    if (sequence_ready && !sequence_readiness_logged) {
        std::cout << "🎉 BREAKTHROUGH! Sequence Manager is now READY! Starting real signal generation..." << std::endl;
        std::cout << "    Bar Index: " << bar_index << ", Sequence History: " 
                  << sequence_manager_->get_history_size() << "/64" << std::endl;
        sequence_readiness_logged = true;
    }

    // Do not proceed if the model isn't loaded or sequence isn't ready.
    if (!model_ready || !feature_ready || !sequence_ready) {
        return signal;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // 🔍 DEBUG: Log that we're starting inference
    static int inference_attempts = 0;
    if (++inference_attempts <= 5) {
        std::cout << "🧪 Starting inference attempt #" << inference_attempts << " [bar " << bar_index << "]" << std::endl;
    }

    try {
        // ✅ FIXED: Get proper temporal sequence using FeatureSequenceManager
        torch::Tensor input_tensor = sequence_manager_->get_batched_tensor().to(torch::kCPU);

        // 3. Normalize features using stats loaded from metadata.
        //    LibTorch correctly broadcasts the 1D mean/std tensors across the 3D input tensor.
        input_tensor = (input_tensor - feature_means_) / feature_stds_;

        // 4. Run inference using PyTorch model (not TorchScript).
        torch::NoGradGuard no_grad;
        
        // ✅ FIXED: Use PyTorch model forward method directly
        auto [prediction, log_var] = (*pytorch_model_)->forward(input_tensor);

        // 5. Convert model outputs to a trading signal.
        double predicted_return = prediction.item<double>();
        
        // Convert the predicted return into a probability using a scaled sigmoid.
        // A positive return suggests a higher probability of price increase.
        signal.probability = 1.0 / (1.0 + std::exp(-predicted_return * 100));

        // ✅ ENHANCED v2: Statistical sigma-based confidence calculation (improved)
        // Convert log-variance to standard deviation (sigma) for more intuitive confidence
        double log_variance = log_var.item<double>();
        double sigma = std::exp(0.5 * log_variance);  // Convert log_var to standard deviation
        signal.confidence = 1.0 - std::min(1.0, sigma);  // Higher sigma = lower confidence
        
        // Ensure confidence is in reasonable range [0.1, 0.9] for trading
        signal.confidence = std::max(0.1, std::min(0.9, signal.confidence));

        // 🔍 DIAGNOSTIC: Log raw values before filtering (first few times)
        static int inference_logged = 0;
        if (inference_logged < 5) {
            std::cout << "🧪 Inference Debug [" << bar_index << "]: "
                      << "Raw pred=" << predicted_return 
                      << ", Raw conf=" << signal.confidence
                      << ", Threshold=" << config_.confidence_threshold << std::endl;
            inference_logged++;
        }

        // Filter out signals with low confidence.
        if (signal.confidence < config_.confidence_threshold) {
            if (inference_logged < 5) {
                std::cout << "    ⚠️  Signal filtered out due to low confidence!" << std::endl;
            }
            signal.probability = 0.5;
            signal.confidence = 0.0;
        }
        
    } catch (const std::exception& e) {
        std::cout << "🚨 INFERENCE EXCEPTION [" << bar_index << "]: " << e.what() << std::endl;
        utils::log_error("Transformer inference exception: " + std::string(e.what()));
        // Return a neutral signal on error to ensure system stability.
        signal.probability = 0.5;
        signal.confidence = 0.0;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    signal.metadata["inference_time_us"] = std::to_string(duration.count());

    return signal;
}

// MLStrategyBase pure virtual method implementations
void TransformerStrategy::apply_metadata_config(const json_utils::JsonValue& metadata) {
    // This method is called by the base class after loading metadata
    // We can use it to configure any additional parameters if needed
}

std::string TransformerStrategy::get_model_path() const {
    return config_.model_path;
}

std::string TransformerStrategy::get_metadata_path() const {
    return config_.metadata_path;
}

size_t TransformerStrategy::get_sequence_length() const {
    return static_cast<size_t>(model_config_.sequence_length);
}

int TransformerStrategy::get_feature_count() const {
    return model_config_.feature_dim;
}

} // namespace sentio