#include "strategy/cpp_ppo_strategy.h"
#include "common/utils.h"
#include "features/feature_config_standard.h" // 🔧 CONSOLIDATED: Standard 91-feature config
#include <torch/torch.h>
#include <fstream>
#include <chrono>
#include <cmath>

namespace sentio {

CppPpoStrategy::CppPpoStrategy(const CppPpoConfig& config) 
    : StrategyComponent(StrategyComponent::StrategyConfig{}), config_(config) {
    
    // 🔧 CONSOLIDATED: Use standardized 91-feature configuration
    // This ensures identical features between TFM, PPO, and all future ML strategies
    auto feature_config = features::get_standard_91_feature_config();
    if (!features::validate_91_feature_config(feature_config)) {
        utils::log_error("❌ CRITICAL: Standard feature configuration validation failed in PPO!");
        // Continue execution but log the issue
    }
    
    feature_engine_ = std::make_unique<features::UnifiedFeatureEngine>(feature_config);
    
    // Initialize caches
    last_probabilities_.resize(3, 0.33);  // [p_sell, p_hold, p_buy]
    last_action_mask_.resize(3, true);    // [sell, hold, buy] validity
}

bool CppPpoStrategy::initialize() {
    utils::log_info("🤖 Initializing C++ PPO Strategy");
    
    // Load the trained PPO model
    if (!load_model()) {
        utils::log_error("❌ Failed to load PPO model");
        return false;
    }
    
    utils::log_info("✅ C++ PPO Strategy initialized successfully");
    utils::log_info("   Model: " + config_.model_path);
    utils::log_info("   Window size: " + std::to_string(config_.window_size));
    utils::log_info("   Feature dimensions: " + std::to_string(config_.feature_dim));
    
    return true;
}

bool CppPpoStrategy::load_model() {
    try {
        utils::log_info("🔄 Loading PPO model from: " + config_.model_path);
        
        // Check if model file exists
        std::ifstream file_check(config_.model_path);
        if (!file_check.good()) {
            utils::log_error("❌ Model file does not exist: " + config_.model_path);
            return false;
        }
        file_check.close();
        
        // Set PyTorch to single-threaded for stability
        torch::set_num_threads(1);
        torch::set_num_interop_threads(1);
        
        // Load the TorchScript model
        model_ = torch::jit::load(config_.model_path);
        model_.eval();
        model_.to(torch::kCPU);
        
        model_loaded_ = true;
        
        utils::log_info("✅ PPO model loaded successfully");
        return true;
        
    } catch (const std::exception& e) {
        utils::log_error("❌ Failed to load PPO model: " + std::string(e.what()));
        model_loaded_ = false;
        return false;
    }
}

SignalOutput CppPpoStrategy::generate_signal(const Bar& bar, int bar_index) {
    SignalOutput signal;
    signal.timestamp_ms = bar.timestamp_ms;
    signal.bar_index = bar_index;
    signal.probability = 0.5;  // Default neutral
    signal.confidence = 0.0;
    
    if (!model_loaded_) {
        if (config_.enable_debug) {
            utils::log_warning("PPO model not loaded, returning neutral signal");
        }
        return signal;
    }
    
    // Update feature engine with new bar
    feature_engine_->update(bar);
    bar_history_.push_back(bar);
    
    // Maintain sliding window
    if (bar_history_.size() > config_.window_size) {
        bar_history_.pop_front();
    }
    
    // Need enough history for observation
    if (bar_history_.size() < config_.window_size) {
        if (config_.enable_debug) {
            utils::log_info("Insufficient history: " + std::to_string(bar_history_.size()) + 
                           "/" + std::to_string(config_.window_size));
        }
        return signal;
    }
    
    current_tick_ = bar_index;
    
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Create observation tensor
        torch::Tensor observation = create_observation();
        
        // Get action mask
        std::vector<bool> action_mask = compute_action_mask();
        // Convert action mask to tensor
        std::vector<uint8_t> mask_data;
        for (bool mask_val : action_mask) {
            mask_data.push_back(mask_val ? 1 : 0);
        }
        torch::Tensor mask_tensor = torch::from_blob(mask_data.data(), {3}, torch::kUInt8).to(torch::kBool).clone();
        
        // Run model inference
        std::vector<torch::jit::IValue> inputs;
        inputs.push_back(observation);
        if (config_.enable_action_masking) {
            inputs.push_back(mask_tensor);
        }
        
        // Forward pass through the model
        torch::Tensor logits;
        {
            torch::NoGradGuard no_grad;
            auto output = model_.forward(inputs);
            
            // Extract logits from model output
            if (output.isTuple()) {
                auto tuple_output = output.toTuple();
                logits = tuple_output->elements()[0].toTensor();
            } else {
                logits = output.toTensor();
            }
        }
        
        // Apply softmax to get probabilities
        torch::Tensor probabilities = torch::softmax(logits, -1);
        
        // Apply action mask if enabled
        if (config_.enable_action_masking) {
            probabilities = probabilities * mask_tensor.to(torch::kFloat32);
            probabilities = probabilities / probabilities.sum();  // Renormalize
        }
        
        // Get action (argmax of probabilities)
        torch::Tensor action_tensor = torch::argmax(probabilities, -1);
        int action = action_tensor.item<int>();
        
        // Convert probabilities to vector
        std::vector<double> probs = tensor_to_vector(probabilities);
        
        // Cache results
        last_probabilities_ = probs;
        last_action_mask_ = action_mask;
        
        // Update position state based on action
        update_position_state(action);
        
        // Convert to Sentio signal format
        signal = convert_to_signal(action, probs, bar_index);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto inference_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        
        if (config_.enable_debug) {
            log_inference_details(action, probs, bar_index);
            utils::log_info("PPO inference time: " + std::to_string(inference_time) + "μs");
        }
        
    } catch (const std::exception& e) {
        utils::log_error("PPO inference failed: " + std::string(e.what()));
        // Return neutral signal on error
        signal.probability = 0.5;
        signal.confidence = 0.0;
    }
    
    return signal;
}

torch::Tensor CppPpoStrategy::create_observation() {
    // Create observation tensor matching real Kochi model format
    // Real model expects flattened input: [batch, window_size * feature_dim]
    
    int state_feature_dim = config_.enable_state_features ? 4 : 0;  // 3 position + 1 PnL
    int total_feature_dim = config_.feature_dim + state_feature_dim;
    
    // Create 2D observation first: [window_size, total_feature_dim]
    torch::Tensor observation_2d = torch::zeros({config_.window_size, total_feature_dim}, torch::kFloat32);
    
    // Fill with features for each timestep in the window
    for (int i = 0; i < config_.window_size && i < static_cast<int>(bar_history_.size()); ++i) {
        int bar_idx = bar_history_.size() - config_.window_size + i;
        if (bar_idx < 0) continue;
        
        // Get market features from unified feature engine
        std::vector<double> features = feature_engine_->get_features();
        
        // For now, use a simplified feature set that matches Kochi's 71 features
        // TODO: Implement proper Kochi feature calculation
        const Bar& bar = bar_history_[bar_idx];
        
        // Create simplified features matching Kochi's format
        std::vector<double> kochi_features = create_kochi_compatible_features(bar, bar_idx);
        
        // Fill market features
        for (size_t j = 0; j < kochi_features.size() && j < config_.feature_dim; ++j) {
            observation_2d[i][j] = static_cast<float>(kochi_features[j]);
        }
        
        // Add state features if enabled
        if (config_.enable_state_features) {
            torch::Tensor state_features = get_state_features();
            for (int k = 0; k < state_features.size(0); ++k) {
                observation_2d[i][config_.feature_dim + k] = state_features[k];
            }
        }
    }
    
    // Flatten for real Kochi model: [1, window_size * total_feature_dim]
    torch::Tensor observation_flat = observation_2d.view({1, -1});
    
    return observation_flat;
}

torch::Tensor CppPpoStrategy::get_state_features() const {
    // Create state features matching Kochi2 format
    // [position_long, position_short, position_hold, unrealized_pnl]
    
    torch::Tensor state_features = torch::zeros({4}, torch::kFloat32);
    
    // Position one-hot encoding
    switch (position_state_) {
        case PositionState::LONG:
            state_features[0] = 1.0f;  // position_long
            break;
        case PositionState::SHORT:
            state_features[1] = 1.0f;  // position_short
            break;
        case PositionState::HOLD:
            state_features[2] = 1.0f;  // position_hold
            break;
    }
    
    // Unrealized P&L
    double unrealized_pnl = compute_reward();
    state_features[3] = static_cast<float>(unrealized_pnl);
    
    return state_features;
}

std::vector<bool> CppPpoStrategy::compute_action_mask() const {
    std::vector<bool> mask(3, true);  // [sell, hold, buy]
    
    if (!config_.enable_action_masking) {
        return mask;  // All actions allowed
    }
    
    // Apply action masking rules (matching Kochi logic)
    switch (position_state_) {
        case PositionState::HOLD:
            // Can buy or sell from neutral position, always can hold
            mask[0] = true;   // Can sell (short)
            mask[1] = true;   // Can hold
            mask[2] = true;   // Can buy (long)
            break;
            
        case PositionState::LONG:
            // Can sell to close or hold, cannot buy more
            mask[0] = true;   // Can sell (close long)
            mask[1] = true;   // Can hold
            mask[2] = false;  // Cannot buy more
            break;
            
        case PositionState::SHORT:
            // Can buy to close or hold, cannot sell more
            mask[0] = false;  // Cannot sell more
            mask[1] = true;   // Can hold
            mask[2] = true;   // Can buy (close short)
            break;
    }
    
    return mask;
}

void CppPpoStrategy::update_position_state(int action) {
    PpoAction ppo_action = index_to_ppo_action(action);
    
    switch (ppo_action) {
        case PpoAction::BUY:
            if (position_state_ == PositionState::SHORT) {
                // Close short position
                position_state_ = PositionState::HOLD;
                entry_price_ = 0.0;
                total_trades_++;
            } else if (position_state_ == PositionState::HOLD) {
                // Open long position
                position_state_ = PositionState::LONG;
                entry_price_ = bar_history_.back().close;  // Use close price
                total_trades_++;
            }
            break;
            
        case PpoAction::SELL:
            if (position_state_ == PositionState::LONG) {
                // Close long position
                position_state_ = PositionState::HOLD;
                entry_price_ = 0.0;
                total_trades_++;
            } else if (position_state_ == PositionState::HOLD) {
                // Open short position
                position_state_ = PositionState::SHORT;
                entry_price_ = bar_history_.back().close;  // Use close price
                total_trades_++;
            }
            break;
            
        case PpoAction::HOLD:
            // No position change
            break;
    }
}

double CppPpoStrategy::compute_reward() const {
    if (position_state_ == PositionState::HOLD || entry_price_ <= 0.0 || bar_history_.empty()) {
        return 0.0;
    }
    
    double current_price = bar_history_.back().close;
    double reward = 0.0;
    
    if (position_state_ == PositionState::LONG) {
        reward = (current_price / entry_price_) - 1.0;
    } else if (position_state_ == PositionState::SHORT) {
        reward = (entry_price_ / current_price) - 1.0;
    }
    
    // Apply transaction fee
    reward *= (1.0 - config_.transaction_fee_pct);
    
    return reward;
}

SignalOutput CppPpoStrategy::convert_to_signal(int action, const std::vector<double>& probabilities, int bar_index) {
    SignalOutput signal;
    signal.timestamp_ms = bar_history_.back().timestamp_ms;
    signal.bar_index = bar_index;
    
    // Use BUY probability as "probability next bar up" (matching our analysis)
    signal.probability = probabilities.size() > 2 ? probabilities[2] : 0.5;
    
    // Use selected action probability as confidence
    signal.confidence = probabilities.size() > action ? probabilities[action] : 0.0;
    
    // Add required metadata for trading system compatibility
    PpoAction ppo_action = index_to_ppo_action(action);
    signal.metadata["ppo_action"] = ppo_action_to_string(ppo_action);
    signal.metadata["p_sell"] = std::to_string(probabilities[0]);
    signal.metadata["p_hold"] = std::to_string(probabilities[1]); 
    signal.metadata["p_buy"] = std::to_string(probabilities[2]);
    signal.metadata["position_state"] = std::to_string(static_cast<int>(position_state_));
    signal.metadata["total_trades"] = std::to_string(total_trades_);
    signal.metadata["unrealized_pnl"] = std::to_string(compute_reward());
    
    // Add required fields for trading system
    signal.metadata["market_data_path"] = "data/equities/QQQ_RTH_NH.csv";
    signal.metadata["strategy_name"] = "cpp_ppo";
    signal.metadata["strategy_version"] = "1.0";
    signal.metadata["symbol"] = "QQQ";
    
    return signal;
}

std::vector<double> CppPpoStrategy::get_action_probabilities() const {
    return last_probabilities_;
}

std::vector<bool> CppPpoStrategy::get_action_mask() const {
    return last_action_mask_;
}

std::vector<double> CppPpoStrategy::tensor_to_vector(const torch::Tensor& tensor) {
    std::vector<double> result;
    
    if (tensor.dim() == 1) {
        result.reserve(tensor.size(0));
        for (int i = 0; i < tensor.size(0); ++i) {
            result.push_back(tensor[i].item<double>());
        }
    } else if (tensor.dim() == 2 && tensor.size(0) == 1) {
        // Handle batch dimension
        result.reserve(tensor.size(1));
        for (int i = 0; i < tensor.size(1); ++i) {
            result.push_back(tensor[0][i].item<double>());
        }
    }
    
    return result;
}

void CppPpoStrategy::log_inference_details(int action, const std::vector<double>& probs, int bar_index) const {
    PpoAction ppo_action = index_to_ppo_action(action);
    
    utils::log_info("🤖 PPO Inference [Bar " + std::to_string(bar_index) + "]:");
    utils::log_info("   Action: " + ppo_action_to_string(ppo_action) + " (index: " + std::to_string(action) + ")");
    utils::log_info("   Probabilities: SELL=" + std::to_string(probs[0]) + 
                    ", HOLD=" + std::to_string(probs[1]) + 
                    ", BUY=" + std::to_string(probs[2]));
    utils::log_info("   Position: " + std::to_string(static_cast<int>(position_state_)) + 
                    ", Trades: " + std::to_string(total_trades_));
    utils::log_info("   Unrealized P&L: " + std::to_string(compute_reward()));
}

std::vector<double> CppPpoStrategy::create_kochi_compatible_features(const Bar& bar, int bar_index) const {
    std::vector<double> features(config_.feature_dim, 0.0);
    
    // Create simplified features that approximate Kochi's feature set
    // This is a placeholder implementation - in production, we'd need the exact Kochi features
    
    if (bar_history_.size() < 2) {
        return features;  // Return zeros if insufficient history
    }
    
    // Basic OHLC features (normalized)
    double close = bar.close;
    double open = bar.open;
    double high = bar.high;
    double low = bar.low;
    double volume = bar.volume;
    
    int idx = 0;
    
    // Price ratios (5 features)
    if (idx < config_.feature_dim) features[idx++] = (high - low) / close;  // Range ratio
    if (idx < config_.feature_dim) features[idx++] = (close - open) / close;  // Body ratio
    if (idx < config_.feature_dim) features[idx++] = (high - std::max(open, close)) / close;  // Upper shadow
    if (idx < config_.feature_dim) features[idx++] = (std::min(open, close) - low) / close;  // Lower shadow
    if (idx < config_.feature_dim) features[idx++] = std::log(volume + 1.0) / 20.0;  // Log volume (normalized)
    
    // Price momentum features (10 features)
    if (bar_history_.size() >= 2) {
        const Bar& prev = bar_history_[bar_history_.size() - 2];
        if (idx < config_.feature_dim) features[idx++] = (close - prev.close) / prev.close;  // Return
        if (idx < config_.feature_dim) features[idx++] = (high - prev.high) / prev.high;  // High change
        if (idx < config_.feature_dim) features[idx++] = (low - prev.low) / prev.low;  // Low change
        if (idx < config_.feature_dim) features[idx++] = (volume - prev.volume) / (prev.volume + 1.0);  // Volume change
        if (idx < config_.feature_dim) features[idx++] = (open - prev.close) / prev.close;  // Gap
    }
    
    // Moving averages (simplified - 20 features)
    if (bar_history_.size() >= 5) {
        // 5-period MA
        double ma5 = 0.0;
        for (int i = 0; i < 5 && i < static_cast<int>(bar_history_.size()); ++i) {
            ma5 += bar_history_[bar_history_.size() - 1 - i].close;
        }
        ma5 /= std::min(5, static_cast<int>(bar_history_.size()));
        if (idx < config_.feature_dim) features[idx++] = (close - ma5) / ma5;
    }
    
    if (bar_history_.size() >= 10) {
        // 10-period MA
        double ma10 = 0.0;
        for (int i = 0; i < 10 && i < static_cast<int>(bar_history_.size()); ++i) {
            ma10 += bar_history_[bar_history_.size() - 1 - i].close;
        }
        ma10 /= std::min(10, static_cast<int>(bar_history_.size()));
        if (idx < config_.feature_dim) features[idx++] = (close - ma10) / ma10;
    }
    
    if (bar_history_.size() >= 20) {
        // 20-period MA
        double ma20 = 0.0;
        for (int i = 0; i < 20 && i < static_cast<int>(bar_history_.size()); ++i) {
            ma20 += bar_history_[bar_history_.size() - 1 - i].close;
        }
        ma20 /= std::min(20, static_cast<int>(bar_history_.size()));
        if (idx < config_.feature_dim) features[idx++] = (close - ma20) / ma20;
    }
    
    // Volatility features (10 features)
    if (bar_history_.size() >= 10) {
        // Calculate recent volatility
        std::vector<double> returns;
        for (int i = 1; i < 10 && i < static_cast<int>(bar_history_.size()); ++i) {
            double ret = (bar_history_[bar_history_.size() - i].close - 
                         bar_history_[bar_history_.size() - i - 1].close) / 
                         bar_history_[bar_history_.size() - i - 1].close;
            returns.push_back(ret);
        }
        
        if (!returns.empty()) {
            double mean_return = 0.0;
            for (double ret : returns) mean_return += ret;
            mean_return /= returns.size();
            
            double variance = 0.0;
            for (double ret : returns) {
                variance += (ret - mean_return) * (ret - mean_return);
            }
            variance /= returns.size();
            double volatility = std::sqrt(variance);
            
            if (idx < config_.feature_dim) features[idx++] = volatility * 100.0;  // Volatility (%)
        }
    }
    
    // Technical indicators (simplified RSI - 5 features)
    if (bar_history_.size() >= 14) {
        // Simplified RSI calculation
        double gains = 0.0, losses = 0.0;
        for (int i = 1; i < 14 && i < static_cast<int>(bar_history_.size()); ++i) {
            double change = bar_history_[bar_history_.size() - i].close - 
                           bar_history_[bar_history_.size() - i - 1].close;
            if (change > 0) gains += change;
            else losses -= change;
        }
        
        if (losses > 0) {
            double rs = gains / losses;
            double rsi = 100.0 - (100.0 / (1.0 + rs));
            if (idx < config_.feature_dim) features[idx++] = rsi / 100.0;  // Normalize to [0,1]
        }
    }
    
    // Time features (10 features)
    time_t timestamp = bar.timestamp_ms / 1000;
    struct tm* time_info = gmtime(&timestamp);
    if (time_info && idx < config_.feature_dim) {
        features[idx++] = std::sin(2 * M_PI * time_info->tm_hour / 24.0);  // Hour cyclical
        if (idx < config_.feature_dim) features[idx++] = std::cos(2 * M_PI * time_info->tm_hour / 24.0);
        if (idx < config_.feature_dim) features[idx++] = std::sin(2 * M_PI * time_info->tm_min / 60.0);  // Minute cyclical
        if (idx < config_.feature_dim) features[idx++] = std::cos(2 * M_PI * time_info->tm_min / 60.0);
        if (idx < config_.feature_dim) features[idx++] = std::sin(2 * M_PI * time_info->tm_wday / 7.0);  // Day of week
    }
    
    // Fill remaining features with normalized price lags
    for (int lag = 1; lag <= 10 && idx < config_.feature_dim; ++lag) {
        if (static_cast<int>(bar_history_.size()) > lag) {
            double lagged_price = bar_history_[bar_history_.size() - 1 - lag].close;
            features[idx++] = (close - lagged_price) / lagged_price;
        } else {
            features[idx++] = 0.0;
        }
    }
    
    // Ensure all features are bounded
    for (double& feature : features) {
        if (std::isnan(feature) || std::isinf(feature)) {
            feature = 0.0;
        }
        // Clamp to reasonable range
        feature = std::max(-5.0, std::min(5.0, feature));
    }
    
    return features;
}

} // namespace sentio
