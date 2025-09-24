#include "cli/strattest_command.h"
#include "strategy/sigor_strategy.h"
#include "strategy/sigor_config.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <unistd.h>
#include <sstream>
#include <vector>

// Conditional includes for optional strategies
#ifdef TORCH_AVAILABLE
#include "strategy/transformer_strategy.h"
#include "strategy/optimized_gru_strategy.h"
#include "strategy/cpp_ppo_strategy.h"
#endif

// Check if momentum scalper exists
#ifdef MOMENTUM_SCALPER_AVAILABLE
#include "strategy/momentum_scalper.h"
#endif

namespace sentio {
namespace cli {

/**
 * @brief Generate KST timestamp string in MM-DD-HH-MM format
 */
std::string generate_kst_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    // Convert to KST (UTC+9)
    auto utc_tm = *std::gmtime(&time_t);
    utc_tm.tm_hour += 9;  // Add 9 hours for KST
    
    // Handle day overflow
    if (utc_tm.tm_hour >= 24) {
        utc_tm.tm_hour -= 24;
        utc_tm.tm_mday += 1;
        // Note: This is a simplified day overflow handling
        // For production, should use proper date arithmetic
    }
    
    std::ostringstream oss;
    oss << std::setfill('0') 
        << std::setw(2) << (utc_tm.tm_mon + 1) << "-"  // Month (1-12)
        << std::setw(2) << utc_tm.tm_mday << "-"       // Day
        << std::setw(2) << utc_tm.tm_hour << "-"       // Hour
        << std::setw(2) << utc_tm.tm_min;              // Minute
    
    return oss.str();
}

/**
 * @brief Generate auto signal filename: <strategy>-<MM-DD-HH-MM-KST>.jsonl
 */
std::string generate_signal_filename(const std::string& strategy) {
    std::string timestamp = generate_kst_timestamp();
    return "data/signals/" + strategy + "-" + timestamp + ".jsonl";
}

int StrattestCommand::execute(const std::vector<std::string>& args) {
    // Parse command-line arguments with intelligent defaults
    std::string dataset = get_arg(args, "--dataset");
    const std::string strategy = get_arg(args, "--strategy", "sgo");  // Default changed to sgo
    
    // Auto-generate output filename based on strategy and timestamp
    std::string output = generate_signal_filename(strategy);
    
    // Ensure signals directory exists
    std::filesystem::create_directories("data/signals");
    
    const std::string format = get_arg(args, "--format", "jsonl");
    const std::string config_path = get_arg(args, "--config", "");
    
    std::cout << "📁 Auto-generated signal file: " << output << std::endl;
    
    // Show help if requested
    if (has_flag(args, "--help") || has_flag(args, "-h")) {
        show_help();
        return 0;
    }
    
    // Apply intelligent defaults for common use cases
    if (dataset.empty()) {
        dataset = "data/equities/QQQ_RTH_NH.csv";
    }
    
    if (output.empty()) {
        output = generate_output_filename(strategy);
    }
    
    // Validate parameters
    if (!validate_parameters(dataset, strategy)) {
        return 1;
    }
    
    // Execute strategy based on type
    if (strategy == "sgo") {
        return execute_sigor_strategy(dataset, output, config_path, args);
    } else if (strategy == "gru") {
#ifdef TORCH_AVAILABLE
        return execute_cpp_gru_strategy(dataset, output, args);
#else
        std::cerr << "Error: GRU strategy not available (LibTorch not found)\n";
        return 1;
#endif
    } else if (strategy == "tfm") {
#ifdef TORCH_AVAILABLE
        return execute_transformer_strategy(dataset, output, args);
#else
        std::cerr << "Error: Transformer strategy not available (LibTorch not found)\n";
        return 1;
#endif
    } else if (strategy == "ppo") {
#ifdef TORCH_AVAILABLE
        return execute_cpp_ppo_strategy(dataset, output, args);
#else
        std::cerr << "Error: PPO strategy not available (LibTorch not found)\n";
        return 1;
#endif
    } else if (strategy == "momentum" || strategy == "scalper") {
#ifdef MOMENTUM_SCALPER_AVAILABLE
        return execute_momentum_strategy(dataset, output, args);
#else
        std::cerr << "Error: Momentum strategy not available\n";
        return 1;
#endif
    } else {
        std::cerr << "Error: Unknown strategy '" << strategy << "'\n";
        std::cerr << "Available strategies: sgo";
#ifdef TORCH_AVAILABLE
        std::cerr << ", gru, tfm, ppo";
#endif
#ifdef MOMENTUM_SCALPER_AVAILABLE
        std::cerr << ", momentum";
#endif
        std::cerr << "\n";
        return 1;
    }
}

void StrattestCommand::show_help() const {
    std::cout << "Usage: sentio_cli strattest [options]\n\n";
    std::cout << "Generate trading signals from market data using AI strategies.\n\n";
    std::cout << "Options:\n";
    std::cout << "  --dataset PATH     Market data file (default: data/equities/QQQ_RTH_NH.csv)\n";
    std::cout << "  --out PATH         Output signals file (default: auto-generated)\n";
        std::cout << "  --strategy NAME    Strategy to use: sigor, cpp_gru, transformer, momentum (default: sigor)\n";
    std::cout << "  --format FORMAT    Output format: jsonl (default: jsonl)\n";
    std::cout << "  --config PATH      Strategy configuration file (optional)\n";
    std::cout << "  --blocks N         Number of blocks to process (default: all)\n";
    std::cout << "  --mode MODE        Processing mode: historical, live (default: historical)\n";
    std::cout << "  --help, -h         Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sentio_cli strattest\n";
        std::cout << "  sentio_cli strattest --strategy momentum --blocks 20\n";
    std::cout << "  sentio_cli strattest --strategy cpp_gru --blocks 20\n";
    std::cout << "  sentio_cli strattest --strategy transformer --blocks 10\n";
    std::cout << "  sentio_cli strattest --dataset data/custom.csv --out signals.jsonl\n";
}

int StrattestCommand::execute_sigor_strategy(const std::string& dataset,
                                           const std::string& output,
                                           const std::string& config_path,
                                           const std::vector<std::string>& args) {
    try {
        // Initialize Sigor strategy with configuration
        sentio::StrategyComponent::StrategyConfig cfg;
        cfg.name = "sigor";
        cfg.version = "0.1";
        cfg.warmup_bars = 20;
        
        auto sigor = std::make_unique<sentio::SigorStrategy>(cfg);
        
        // Load custom configuration if provided
        if (!config_path.empty()) {
            auto scfg = sentio::SigorConfig::from_file(config_path);
            sigor->set_config(scfg);
        }
        
        // Get blocks parameter for performance optimization
        int blocks_to_process = get_blocks_parameter(args);
        std::vector<sentio::SignalOutput> signals;
        
        if (blocks_to_process > 0) {
            std::cout << "🔧 Performance mode: Processing only " << blocks_to_process << " blocks (~" << (blocks_to_process * STANDARD_BLOCK_SIZE) << " bars)" << std::endl;
            std::cout << "⚡ Sigor ensemble strategy - fast binary data loading with index-based processing" << std::endl;
            
            // Use index-based processing (no temporary files needed)
            uint64_t total_bars = utils::get_market_data_count(dataset);
            uint64_t bars_to_process = blocks_to_process * STANDARD_BLOCK_SIZE;
            uint64_t start_index = (total_bars > bars_to_process) ? (total_bars - bars_to_process) : 0;
            
            signals = sigor->process_dataset_range(dataset, cfg.name, {}, start_index, bars_to_process);
            
            std::cout << "⚡ Processed " << signals.size() << " signals from range [" << start_index << "-" << (start_index + bars_to_process - 1) << "]" << std::endl;
        } else {
            std::cout << "Processing full dataset: " << dataset << std::endl;
            signals = sigor->process_dataset(dataset, cfg.name, {});
        }
        
        // Add metadata for traceability
        for (auto& signal : signals) {
            signal.metadata["market_data_path"] = dataset;
        }
        
        // Export signals to output file
        bool success = sigor->export_signals(signals, output, "jsonl");
        if (!success) {
            std::cerr << "ERROR: Failed exporting signals to " << output << std::endl;
            return 2;
        }
        
        std::cout << "✅ Exported " << signals.size() << " signals to " << output << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Sigor strategy execution failed: " << e.what() << std::endl;
        return 1;
    }
}

// LEGACY GRU STRATEGY REMOVED FOR PERFORMANCE - USE cpp_gru INSTEAD
int StrattestCommand::execute_gru_strategy(const std::string& dataset,
                                         const std::string& output,
                                         const std::vector<std::string>& args) {
    std::cerr << "ERROR: Legacy GRU strategy has been removed for performance.\n";
    std::cerr << "Use 'cpp_gru' for the optimized GRU implementation.\n";
    return 1;
}

int StrattestCommand::execute_cpp_gru_strategy(const std::string& dataset,
                                              const std::string& output,
                                              const std::vector<std::string>& args) {
#ifdef TORCH_AVAILABLE
    try {
        // Initialize Optimized GRU strategy with 53-feature primary model
        sentio::OptimizedGruConfig config;
        config.model_path = "artifacts/GRU/primary/model.pt";
        config.metadata_path = "artifacts/GRU/primary/metadata.json";
        config.sequence_length = 64;  // Match primary model
        config.feature_dim = 53;      // Match primary model (53 features)
        config.confidence_threshold = 0.6;
        config.enable_debug = false;  // Disable for maximum performance
        
        auto cpp_gru = std::make_unique<sentio::OptimizedGruStrategy>(config);
        
        // Initialize the strategy (loads model)
        if (!cpp_gru->initialize()) {
            std::cerr << "ERROR: Failed to initialize C++ GRU strategy" << std::endl;
            return 1;
        }
        
        std::cout << "⚡ Processing dataset with OptimizedGRU strategy: " << dataset << std::endl;
        std::cout << "   Model: " << config.model_path << std::endl;
        std::cout << "   Target: <500μs inference time" << std::endl;
        std::cout << "   Features: Tensor pooling + O(1) incremental updates" << std::endl;
        
        // Process dataset using the base strategy interface
        sentio::StrategyComponent::StrategyConfig base_cfg;
        base_cfg.name = "cpp_gru";
        base_cfg.version = "1.0";
        base_cfg.warmup_bars = config.sequence_length;
        
        // Get blocks parameter for performance optimization
        int blocks_to_process = get_blocks_parameter(args);
        std::vector<sentio::SignalOutput> signals;
        
        if (blocks_to_process > 0) {
            std::cout << "🔧 Performance mode: Processing only " << blocks_to_process << " blocks (~" << (blocks_to_process * STANDARD_BLOCK_SIZE) << " bars)" << std::endl;
            std::cout << "⚡ OptimizedGRU with tensor pooling - fast binary data loading with index-based processing" << std::endl;
            
            // Use index-based processing (no temporary files needed)
            uint64_t total_bars = utils::get_market_data_count(dataset);
            uint64_t bars_to_process = blocks_to_process * STANDARD_BLOCK_SIZE;
            uint64_t start_index = (total_bars > bars_to_process) ? (total_bars - bars_to_process) : 0;
            
            signals = cpp_gru->process_dataset_range(dataset, base_cfg.name, {}, start_index, bars_to_process);
            
            std::cout << "⚡ Processed " << signals.size() << " signals from range [" << start_index << "-" << (start_index + bars_to_process - 1) << "]" << std::endl;
        } else {
            std::cout << "⚠️  Processing full dataset - this will take a long time!" << std::endl;
            signals = cpp_gru->process_dataset(dataset, base_cfg.name, {});
        }
        
        // Add C++ GRU specific metadata
        for (auto& signal : signals) {
            signal.metadata["market_data_path"] = dataset;
            signal.metadata["model_type"] = "AdvancedGRU_CPP";
            signal.metadata["model_path"] = config.model_path;
            signal.metadata["feature_count"] = std::to_string(config.feature_dim);
            signal.metadata["sequence_length"] = std::to_string(config.sequence_length);
            signal.metadata["optimized"] = "true";
            signal.metadata["tensor_pooling"] = "enabled";
        }
        
        // Export signals
        bool success = cpp_gru->export_signals(signals, output, "jsonl");
        if (!success) {
            std::cerr << "ERROR: Failed exporting C++ GRU signals to " << output << std::endl;
            return 2;
        }
        
        // Get performance statistics
        auto stats = cpp_gru->get_performance_stats();
        std::cout << "✅ Exported " << signals.size() << " OptimizedGRU signals to " << output << std::endl;
        std::cout << "📊 Performance Statistics:" << std::endl;
        std::cout << "   Total inferences: " << stats.total_inferences << std::endl;
        std::cout << "   Average inference time: " << stats.avg_inference_time_us << "μs" << std::endl;
        std::cout << "   Max inference time: " << stats.max_inference_time_us << "μs" << std::endl;
        std::cout << "   Average confidence: " << std::fixed << std::setprecision(3) << stats.avg_confidence << std::endl;
        
        if (stats.avg_inference_time_us < 500) {
            std::cout << "🎯 PERFORMANCE TARGET MET: <500μs ✅" << std::endl;
        } else {
            std::cout << "⚠️  Performance target missed: " << stats.avg_inference_time_us << "μs > 500μs" << std::endl;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: C++ GRU strategy execution failed: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cerr << "ERROR: C++ GRU strategy not available (LibTorch not found)" << std::endl;
    return 1;
#endif
}

int StrattestCommand::execute_transformer_strategy(const std::string& dataset,
                                                  const std::string& output,
                                                  const std::vector<std::string>& args) {
#ifdef TORCH_AVAILABLE
    try {
        // Initialize Transformer strategy
        sentio::StrategyComponent::StrategyConfig base_cfg;
        base_cfg.name = "transformer";
        base_cfg.version = "1.0";
        base_cfg.warmup_bars = 64; // Transformer requires sequence warmup
        
        sentio::TransformerStrategy::TransformerConfig transformer_cfg;
        // Use the trained model artifacts (PyTorch 2.6.0 compatible)
        transformer_cfg.model_path = "artifacts/Transformer/v2_2_6_0_compatible/model.pt";
        transformer_cfg.metadata_path = "artifacts/Transformer/v2_2_6_0_compatible/metadata.json";
        transformer_cfg.sequence_length = 64;
        transformer_cfg.feature_count = 173;
        transformer_cfg.enable_debug_logging = has_flag(args, "--debug");
        
        auto transformer = std::make_unique<sentio::TransformerStrategy>(base_cfg, transformer_cfg);
        
        // Initialize the strategy (load model)
        std::cout << "Initializing Transformer strategy..." << std::endl;
        if (!transformer->initialize()) {
            std::cerr << "ERROR: Failed to initialize Transformer strategy" << std::endl;
            return 1;
        }
        
        // Process dataset with limited blocks for performance
        std::cout << "Processing dataset with Transformer strategy: " << dataset << std::endl;
        
        // Get blocks parameter for performance optimization
        int blocks_to_process = get_blocks_parameter(args);
        std::vector<sentio::SignalOutput> signals;
        
        if (blocks_to_process > 0) {
            std::cout << "🔧 Performance mode: Processing only " << blocks_to_process << " blocks (~" << (blocks_to_process * STANDARD_BLOCK_SIZE) << " bars)" << std::endl;
            std::cout << "⚡ Large transformer model (4.78M params) - fast binary data loading with index-based processing" << std::endl;
            
            // Use index-based processing (no temporary files needed)
            uint64_t total_bars = utils::get_market_data_count(dataset);
            uint64_t bars_to_process = blocks_to_process * STANDARD_BLOCK_SIZE;
            uint64_t start_index = (total_bars > bars_to_process) ? (total_bars - bars_to_process) : 0;
            
            signals = transformer->process_dataset_range(dataset, base_cfg.name, {}, start_index, bars_to_process);
            
            std::cout << "⚡ Processed " << signals.size() << " signals from range [" << start_index << "-" << (start_index + bars_to_process - 1) << "]" << std::endl;
        } else {
            std::cout << "⚠️  Processing full dataset - this will take a VERY long time with transformer!" << std::endl;
            signals = transformer->process_dataset(dataset, base_cfg.name, {});
        }
        
        // Add metadata
        for (auto& signal : signals) {
            signal.metadata["market_data_path"] = dataset;
            signal.metadata["model_type"] = "Transformer";
            signal.metadata["feature_count"] = "173";
            signal.metadata["sequence_length"] = "64";
            signal.metadata["model_version"] = "v2_2_6_0_compatible";
        }
        
        // Export signals
        bool success = transformer->export_signals(signals, output, "jsonl");
        if (!success) {
            std::cerr << "ERROR: Failed exporting transformer signals to " << output << std::endl;
            return 2;
        }
        
        // Show performance stats
        auto stats = transformer->get_inference_stats();
        std::cout << "📊 Transformer Performance:" << std::endl;
        std::cout << "   Total inferences: " << stats.total_inferences << std::endl;
        std::cout << "   Average time: " << stats.avg_inference_time_us << " μs" << std::endl;
        std::cout << "   Latency violations: " << stats.latency_violations << " (" << (stats.violation_rate * 100.0) << "%)" << std::endl;
        
        std::cout << "✅ Exported " << signals.size() << " Transformer signals to " << output << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Transformer strategy execution failed: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cerr << "ERROR: Transformer strategy not available (LibTorch not found)" << std::endl;
    return 1;
#endif
}

int StrattestCommand::execute_momentum_strategy(const std::string& dataset,
                                              const std::string& output,
                                              const std::vector<std::string>& args) {
#ifdef MOMENTUM_SCALPER_AVAILABLE
    try {
        // Initialize Momentum Scalper strategy
        sentio::StrategyComponent::StrategyConfig cfg;
        cfg.name = "momentum";
        cfg.version = "0.1";
        cfg.warmup_bars = 30; // Need warmup for moving averages
        
        auto momentum = std::make_unique<sentio::MomentumScalper>(cfg);
        
        // Process dataset
        std::cout << "Processing dataset with Momentum Scalper: " << dataset << std::endl;
        auto signals = momentum->process_dataset(dataset, cfg.name, {});
        
        // Add metadata
        for (auto& signal : signals) {
            signal.metadata["market_data_path"] = dataset;
            signal.metadata["strategy_type"] = "momentum_scalper";
        }
        
        // Export signals
        bool success = momentum->export_signals(signals, output, "jsonl");
        if (!success) {
            std::cerr << "ERROR: Failed exporting momentum signals to " << output << std::endl;
            return 2;
        }
        
        std::cout << "✅ Exported " << signals.size() << " momentum signals to " << output << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Momentum strategy execution failed: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cerr << "ERROR: Momentum strategy not available" << std::endl;
    return 1;
#endif
}

std::string StrattestCommand::generate_output_filename(const std::string& strategy) const {
    // Ensure proper file organization in data/signals directory
    std::error_code ec;
    std::filesystem::create_directories("data/signals", ec);
    
    return "data/signals/" + strategy + "_signals_AUTO.jsonl";
}

bool StrattestCommand::validate_parameters(const std::string& dataset,
                                         const std::string& strategy) const {
    // Check if dataset file exists
    if (!std::filesystem::exists(dataset)) {
        std::cerr << "Error: Dataset file not found: " << dataset << std::endl;
        return false;
    }
    
    // Validate strategy name
        if (strategy != "sgo" && strategy != "gru" && strategy != "tfm" &&
            strategy != "momentum" && strategy != "scalper" && strategy != "ppo") {
            std::cerr << "Error: Invalid strategy '" << strategy << "'" << std::endl;
            std::cerr << "Available strategies: sgo, gru, tfm, ppo, momentum" << std::endl;
            return false;
        }
    
    return true;
}

int StrattestCommand::get_blocks_parameter(const std::vector<std::string>& args) const {
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--blocks" && i + 1 < args.size()) {
            return std::stoi(args[i + 1]);
        }
    }
    return 0;
}


#ifdef TORCH_AVAILABLE
int StrattestCommand::execute_cpp_ppo_strategy(const std::string& dataset,
                                              const std::string& output,
                                              const std::vector<std::string>& args) {
    std::cout << "🤖 Executing C++ PPO Strategy" << std::endl;
    std::cout << "==============================" << std::endl;
    
    // Parse blocks parameter
    int blocks_to_process = get_blocks_parameter(args);
    
    // TODO: Update PPO strategy to use index-based processing like other strategies
    std::cout << "⚠️  PPO strategy still uses full dataset processing (not optimized)" << std::endl;
    
    try {
        // Initialize C++ PPO strategy with Kochi model
        sentio::CppPpoConfig config;
        config.model_path = "kochi/data/PPO_116/real_kochi_model.pt";
        config.window_size = 30;
        config.feature_dim = 71;   // Real Kochi feature set (30 x 71 = 2130)
        config.confidence_threshold = 0.5;
        config.enable_action_masking = true;
        config.enable_debug = true;   // Enable for testing
        
        auto cpp_ppo = std::make_unique<sentio::CppPpoStrategy>(config);
        
        // Initialize strategy
        if (!cpp_ppo->initialize()) {
            std::cerr << "❌ Failed to initialize C++ PPO strategy" << std::endl;
            return 1;
        }
        
        std::cout << "✅ C++ PPO Strategy initialized successfully" << std::endl;
        std::cout << "   Model: " << config.model_path << std::endl;
        std::cout << "   Window size: " << config.window_size << std::endl;
        std::cout << "   Feature dimensions: " << config.feature_dim << std::endl;
        std::cout << "   Action masking: " << (config.enable_action_masking ? "enabled" : "disabled") << std::endl;
        
        // Load market data
        std::cout << "\n📊 Loading market data..." << std::endl;
        std::ifstream file(dataset);
        if (!file.is_open()) {
            std::cerr << "❌ Failed to open dataset: " << dataset << std::endl;
            return 1;
        }
        
        std::vector<sentio::Bar> bars;
        std::string line;
        std::getline(file, line); // Skip header
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            std::istringstream ss(line);
            std::string token;
            
            sentio::Bar bar;
            
            // Parse CSV: ts_utc,open,high,low,close,volume
            std::getline(ss, token, ',');
            bar.timestamp_ms = std::stoull(token) * 1000; // Convert to milliseconds
            
            std::getline(ss, token, ',');
            bar.open = std::stod(token);
            
            std::getline(ss, token, ',');
            bar.high = std::stod(token);
            
            std::getline(ss, token, ',');
            bar.low = std::stod(token);
            
            std::getline(ss, token, ',');
            bar.close = std::stod(token);
            
            std::getline(ss, token, ',');
            bar.volume = std::stod(token);
            
            bars.push_back(bar);
        }
        file.close();
        
        std::cout << "✅ Loaded " << bars.size() << " bars from dataset" << std::endl;
        
        // Generate signals
        std::cout << "\n🔄 Generating PPO signals..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<sentio::SignalOutput> signals;
        signals.reserve(bars.size());
        
        int processed_bars = 0;
        int progress_interval = std::max(1, static_cast<int>(bars.size()) / 20); // 5% intervals
        
        for (size_t i = 0; i < bars.size(); ++i) {
            sentio::SignalOutput signal = cpp_ppo->generate_signal(bars[i], static_cast<int>(i));
            signals.push_back(signal);
            
            processed_bars++;
            if (processed_bars % progress_interval == 0) {
                double progress = (static_cast<double>(processed_bars) / bars.size()) * 100.0;
                std::cout << "  📈 Progress: " << std::fixed << std::setprecision(1) 
                         << progress << "% (" << processed_bars << "/" << bars.size() << " bars)" << std::endl;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto inference_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        std::cout << "✅ Generated " << signals.size() << " PPO signals" << std::endl;
        std::cout << "⏱️  Total inference time: " << inference_time << "ms" << std::endl;
        std::cout << "🚀 Average time per bar: " << std::fixed << std::setprecision(2) 
                 << (static_cast<double>(inference_time) / bars.size()) << "ms" << std::endl;
        
        // Save signals to JSONL file
        std::cout << "\n💾 Saving signals to: " << output << std::endl;
        std::ofstream output_file(output);
        if (!output_file.is_open()) {
            std::cerr << "❌ Failed to create output file: " << output << std::endl;
            return 1;
        }
        
        for (const auto& signal : signals) {
            // Create JSON object
            std::ostringstream json_line;
            json_line << "{";
            json_line << "\"timestamp_ms\":" << signal.timestamp_ms << ",";
            json_line << "\"bar_index\":" << signal.bar_index << ",";
            json_line << "\"probability\":" << std::fixed << std::setprecision(6) << signal.probability << ",";
            json_line << "\"confidence\":" << std::fixed << std::setprecision(6) << signal.confidence;
            
            // Add metadata
            for (const auto& [key, value] : signal.metadata) {
                json_line << ",\"" << key << "\":\"" << value << "\"";
            }
            
            json_line << "}";
            output_file << json_line.str() << std::endl;
        }
        output_file.close();
        
        std::cout << "✅ Signals saved successfully" << std::endl;
        
        // Performance statistics
        std::cout << "\n📊 PPO Strategy Performance Statistics:" << std::endl;
        std::cout << "  🎯 Total signals generated: " << signals.size() << std::endl;
        std::cout << "  ⚡ Average inference time: " << std::fixed << std::setprecision(2) 
                 << (static_cast<double>(inference_time) / signals.size()) << "ms per signal" << std::endl;
        
        // Signal distribution analysis
        int buy_signals = 0, sell_signals = 0, hold_signals = 0;
        double avg_confidence = 0.0;
        
        for (const auto& signal : signals) {
            avg_confidence += signal.confidence;
            
            // Analyze action from metadata
            auto action_it = signal.metadata.find("ppo_action");
            if (action_it != signal.metadata.end()) {
                if (action_it->second == "BUY") buy_signals++;
                else if (action_it->second == "SELL") sell_signals++;
                else if (action_it->second == "HOLD") hold_signals++;
            }
        }
        
        avg_confidence /= signals.size();
        
        std::cout << "  📈 Signal distribution:" << std::endl;
        std::cout << "    • BUY signals: " << buy_signals << " (" 
                 << std::fixed << std::setprecision(1) << (100.0 * buy_signals / signals.size()) << "%)" << std::endl;
        std::cout << "    • SELL signals: " << sell_signals << " (" 
                 << std::fixed << std::setprecision(1) << (100.0 * sell_signals / signals.size()) << "%)" << std::endl;
        std::cout << "    • HOLD signals: " << hold_signals << " (" 
                 << std::fixed << std::setprecision(1) << (100.0 * hold_signals / signals.size()) << "%)" << std::endl;
        std::cout << "  🎯 Average confidence: " << std::fixed << std::setprecision(3) << avg_confidence << std::endl;
        
        std::cout << "\n🎉 C++ PPO strategy execution completed successfully!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ C++ PPO strategy execution failed: " << e.what() << std::endl;
        return 1;
    }
}
#endif

} // namespace cli
} // namespace sentio
