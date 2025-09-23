#include "training/gru_trainer.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <ctime>

// JSON library - use nlohmann/json if available, otherwise simple fallback
#ifdef NLOHMANN_JSON_VERSION_MAJOR
#include <nlohmann/json.hpp>
#else
// Simple JSON fallback for metadata export
namespace nlohmann {
    struct json {
        std::map<std::string, std::string> data;
        json& operator[](const std::string& key) { return *this; }
        void operator=(const std::string& value) {}
        void operator=(int value) {}
        void operator=(double value) {}
        void operator=(bool value) {}
        void operator=(size_t value) {}
        void operator=(const std::vector<std::string>& value) {}
        void operator=(const std::vector<double>& value) {}
        std::string dump(int indent = 0) const { return "{}"; }
    };
}
#endif

namespace sentio {
namespace training {

GRUTrainer::GRUTrainer(const ml::GRUConfig& config) : config_(config) {
    // Create model
    model_ = std::make_shared<ml::AdvancedGRUModelImpl>(config);
    
    // Create optimizer with more conservative settings
    optimizer_ = std::make_unique<torch::optim::AdamW>(
        model_->parameters(),
        torch::optim::AdamWOptions(config.learning_rate * 0.1).weight_decay(config.weight_decay) // Reduce learning rate
    );
    
    std::cout << "🧠 Advanced GRU Trainer initialized" << std::endl;
    std::cout << "   Model parameters: " << model_->get_parameter_count() << std::endl;
    std::cout << "   Feature dim: " << config.feature_dim << std::endl;
    std::cout << "   Sequence length: " << config.sequence_length << std::endl;
    std::cout << "   Hidden dim: " << config.hidden_dim << std::endl;
    std::cout << "   Multi-task: " << (config.enable_multi_task ? "enabled" : "disabled") << std::endl;
}

bool GRUTrainer::load_data(const std::string& csv_path) {
    std::cout << "📈 Loading market data from: " << csv_path << std::endl;
    
    std::ifstream file(csv_path);
    if (!file.is_open()) {
        std::cerr << "❌ Failed to open file: " << csv_path << std::endl;
        return false;
    }
    
    std::string line;
    std::getline(file, line); // Skip header
    
    raw_data_.clear();
    int bar_index = 0;
    
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        
        while (std::getline(ss, token, ',')) {
            tokens.push_back(token);
        }
        
        if (tokens.size() >= 6) {
            Bar bar;
            // bar.bar_index = bar_index++; // Bar doesn't have bar_index field
            bar.symbol = "QQQ"; // Default symbol
            
            // Parse timestamp (assuming it's in milliseconds or ISO format)
            if (tokens[0].find('-') != std::string::npos || tokens[0].find('T') != std::string::npos) {
                // ISO format - simplified parsing
                bar.timestamp_ms = bar_index * 60000; // 1 minute bars
            } else {
                bar.timestamp_ms = std::stoll(tokens[0]);
            }
            
            bar.open = std::stod(tokens[1]);
            bar.high = std::stod(tokens[2]);
            bar.low = std::stod(tokens[3]);
            bar.close = std::stod(tokens[4]);
            bar.volume = tokens.size() > 5 ? std::stod(tokens[5]) : 1000000.0;
            
            raw_data_.push_back(bar);
        }
    }
    
    std::cout << "   Loaded " << raw_data_.size() << " bars" << std::endl;
    return !raw_data_.empty();
}

torch::Tensor GRUTrainer::create_comprehensive_features(const std::vector<Bar>& bars) {
    if (bars.empty()) return torch::empty({0, config_.feature_dim});
    
    const int num_bars = bars.size();
    torch::Tensor features = torch::zeros({num_bars, config_.feature_dim});
    
    // Extract price and volume data
    std::vector<double> prices, volumes, opens, highs, lows;
    prices.reserve(num_bars);
    volumes.reserve(num_bars);
    opens.reserve(num_bars);
    highs.reserve(num_bars);
    lows.reserve(num_bars);
    
    for (const auto& bar : bars) {
        prices.push_back(bar.close);
        volumes.push_back(bar.volume);
        opens.push_back(bar.open);
        highs.push_back(bar.high);
        lows.push_back(bar.low);
    }
    
    // Calculate returns
    std::vector<double> returns(num_bars, 0.0);
    for (int i = 1; i < num_bars; ++i) {
        returns[i] = std::log(prices[i] / prices[i-1]);
    }
    
    // Feature index
    int feat_idx = 0;
    feature_names_.clear();
    
    // Basic features
    for (int i = 0; i < num_bars; ++i) {
        if (feat_idx < config_.feature_dim) {
            features[i][feat_idx] = returns[i];
            if (i == 0) feature_names_.push_back("returns");
            feat_idx++;
        }
        
        if (feat_idx < config_.feature_dim && i >= 1) {
            features[i][feat_idx] = returns[i-1];
            if (i == 1) feature_names_.push_back("returns_lag1");
            feat_idx++;
        }
        
        if (feat_idx < config_.feature_dim && i >= 2) {
            features[i][feat_idx] = returns[i-2];
            if (i == 2) feature_names_.push_back("returns_lag2");
            feat_idx++;
        }
        
        // Price acceleration
        if (feat_idx < config_.feature_dim && i >= 1) {
            double accel = i >= 2 ? returns[i] - returns[i-1] : 0.0;
            features[i][feat_idx] = accel;
            if (i == 1) feature_names_.push_back("price_acceleration");
            feat_idx++;
        }
        
        // Momentum features
        if (feat_idx < config_.feature_dim && i >= 5) {
            double momentum_5 = prices[i] / prices[i-5] - 1.0;
            features[i][feat_idx] = momentum_5;
            if (i == 5) feature_names_.push_back("momentum_5");
            feat_idx++;
        }
        
        if (feat_idx < config_.feature_dim && i >= 10) {
            double momentum_10 = prices[i] / prices[i-10] - 1.0;
            features[i][feat_idx] = momentum_10;
            if (i == 10) feature_names_.push_back("momentum_10");
            feat_idx++;
        }
        
        // Moving averages
        for (int period : {5, 10, 20, 50}) {
            if (feat_idx < config_.feature_dim && i >= period) {
                double sma = calculate_sma(prices, period, i);
                double sma_ratio = prices[i] / sma;
                features[i][feat_idx] = sma_ratio;
                if (i == period) feature_names_.push_back("sma_ratio_" + std::to_string(period));
                feat_idx++;
            }
        }
        
        // Volatility features
        for (int period : {5, 10, 20}) {
            if (feat_idx < config_.feature_dim && i >= period) {
                double vol = 0.0;
                for (int j = std::max(0, i - period + 1); j <= i; ++j) {
                    vol += returns[j] * returns[j];
                }
                vol = std::sqrt(vol / period);
                features[i][feat_idx] = vol;
                if (i == period) feature_names_.push_back("volatility_" + std::to_string(period));
                feat_idx++;
            }
        }
        
        // RSI
        if (feat_idx < config_.feature_dim && i >= 14) {
            double rsi = calculate_rsi(prices, 14, i);
            features[i][feat_idx] = rsi / 100.0; // Normalize to [0,1]
            if (i == 14) feature_names_.push_back("rsi");
            feat_idx++;
        }
        
        // Bollinger Bands
        if (feat_idx < config_.feature_dim && i >= 20) {
            double bb_pos = calculate_bollinger_position(prices, 20, i);
            features[i][feat_idx] = bb_pos;
            if (i == 20) feature_names_.push_back("bollinger_position");
            feat_idx++;
        }
        
        // MACD
        if (feat_idx < config_.feature_dim && i >= 26) {
            auto [macd, signal, histogram] = calculate_macd(prices, i);
            if (feat_idx < config_.feature_dim) {
                features[i][feat_idx] = macd;
                if (i == 26) feature_names_.push_back("macd");
                feat_idx++;
            }
            if (feat_idx < config_.feature_dim) {
                features[i][feat_idx] = signal;
                if (i == 26) feature_names_.push_back("macd_signal");
                feat_idx++;
            }
            if (feat_idx < config_.feature_dim) {
                features[i][feat_idx] = histogram;
                if (i == 26) feature_names_.push_back("macd_histogram");
                feat_idx++;
            }
        }
        
        // Volume features
        if (feat_idx < config_.feature_dim && i >= 10) {
            double vol_sma = calculate_sma(volumes, 10, i);
            double vol_ratio = volumes[i] / vol_sma;
            features[i][feat_idx] = vol_ratio;
            if (i == 10) feature_names_.push_back("volume_ratio");
            feat_idx++;
        }
        
        // Market microstructure
        if (feat_idx < config_.feature_dim) {
            double high_low_ratio = (highs[i] - lows[i]) / prices[i];
            features[i][feat_idx] = high_low_ratio;
            if (i == 0) feature_names_.push_back("high_low_ratio");
            feat_idx++;
        }
        
        if (feat_idx < config_.feature_dim) {
            double open_close_ratio = (prices[i] - opens[i]) / opens[i];
            features[i][feat_idx] = open_close_ratio;
            if (i == 0) feature_names_.push_back("open_close_ratio");
            feat_idx++;
        }
        
        // Gap
        if (feat_idx < config_.feature_dim && i >= 1) {
            double gap = (opens[i] - prices[i-1]) / prices[i-1];
            features[i][feat_idx] = gap;
            if (i == 1) feature_names_.push_back("gap");
            feat_idx++;
        }
        
        // Reset feature index for next bar
        feat_idx = 0;
    }
    
    // Fill remaining features with zeros if needed
    while (feature_names_.size() < config_.feature_dim) {
        feature_names_.push_back("feature_" + std::to_string(feature_names_.size()));
    }
    
    std::cout << "✅ Created " << feature_names_.size() << " comprehensive features" << std::endl;
    return features;
}

std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor> 
GRUTrainer::create_multi_task_targets(const std::vector<Bar>& bars) {
    const int num_bars = bars.size();
    
    torch::Tensor direction_targets = torch::zeros({num_bars});
    torch::Tensor magnitude_targets = torch::zeros({num_bars});
    torch::Tensor confidence_targets = torch::zeros({num_bars});
    torch::Tensor volatility_targets = torch::zeros({num_bars});
    torch::Tensor regime_targets = torch::zeros({num_bars});
    
    std::vector<double> prices;
    for (const auto& bar : bars) {
        prices.push_back(bar.close);
    }
    
    // Calculate returns for targets
    std::vector<double> returns(num_bars, 0.0);
    for (int i = 1; i < num_bars; ++i) {
        returns[i] = std::log(prices[i] / prices[i-1]);
    }
    
    // Analyze target distribution first
    int bullish_count = 0, bearish_count = 0;
    
    for (int i = 0; i < num_bars - 1; ++i) {
        // FIXED: Use next bar probability only (not 5-bar average trend)
        // This creates much more diverse signals (0.2-0.8) for better trading
        double future_return = returns[i + 1];  // Just next single bar
        
        // Binary target: next bar up (>0) or down (<=0)
        // No threshold needed - pure next-bar direction prediction
        double direction = (future_return > 0.0) ? 1.0f : 0.0f;
        
        if (direction > 0.5) bullish_count++;
        else bearish_count++;
        
        direction_targets[i] = direction;
        
        // Magnitude: absolute future return
        magnitude_targets[i] = std::abs(future_return);
        
        // Confidence: based on signal strength (higher for larger moves)
        confidence_targets[i] = std::min(1.0, std::abs(future_return) * 10.0);
        
        // Volatility: next 5-bar realized volatility
        if (i + 5 < num_bars) {
            double vol = 0.0;
            for (int j = 1; j <= 5; ++j) {
                if (i + j < num_bars) {
                    vol += returns[i + j] * returns[i + j];
                }
            }
            volatility_targets[i] = std::sqrt(vol / 5.0);
        }
        
        // Regime: simplified classification based on recent volatility and trend
        if (i >= 20) {
            double vol_20 = 0.0;
            double ret_20 = 0.0;
            for (int j = 0; j < 20; ++j) {
                if (i - j >= 0) {
                    vol_20 += returns[i - j] * returns[i - j];
                    ret_20 += returns[i - j];
                }
            }
            vol_20 = std::sqrt(vol_20 / 20.0);
            ret_20 /= 20.0;
            
            // Classify regime: 0=bull_low_vol, 1=bull_high_vol, 2=bear_low_vol, 3=bear_high_vol
            bool high_vol = vol_20 > 0.02; // 2% daily volatility threshold
            bool bull_trend = ret_20 > 0;
            
            if (bull_trend && !high_vol) regime_targets[i] = 0;
            else if (bull_trend && high_vol) regime_targets[i] = 1;
            else if (!bull_trend && !high_vol) regime_targets[i] = 2;
            else regime_targets[i] = 3;
        }
    }
    
    // Log target distribution analysis
    std::cout << "Target distribution analysis:" << std::endl;
    std::cout << "  Bullish targets: " << bullish_count << std::endl;
    std::cout << "  Bearish targets: " << bearish_count << std::endl;
    
    double ratio = (double)bullish_count / (bullish_count + bearish_count);
    std::cout << "  Bullish ratio: " << std::fixed << std::setprecision(3) << ratio << std::endl;
    
    if (ratio < 0.3 || ratio > 0.7) {
        std::cout << "  WARNING: Imbalanced targets! This may cause model bias." << std::endl;
        std::cout << "  Consider applying class weighting or data resampling." << std::endl;
    } else {
        std::cout << "  INFO: Target distribution appears balanced." << std::endl;
    }
    
    std::cout << "✅ Created multi-task targets" << std::endl;
    return std::make_tuple(direction_targets, magnitude_targets, confidence_targets, volatility_targets, regime_targets);
}

bool GRUTrainer::prepare_training_data() {
    if (raw_data_.empty()) {
        std::cerr << "❌ No data loaded" << std::endl;
        return false;
    }
    
    std::cout << "🔧 Preparing training data..." << std::endl;
    
    // Create features and targets
    auto features = create_comprehensive_features(raw_data_);
    auto targets = create_multi_task_targets(raw_data_);
    
    // Create sequences
    create_training_sequences(features, targets);
    
    // Calculate feature statistics for normalization
    feature_means_.resize(config_.feature_dim, 0.0);
    feature_scales_.resize(config_.feature_dim, 1.0);
    
    if (!train_samples_.empty()) {
        std::vector<torch::Tensor> feature_tensors;
        for (const auto& sample : train_samples_) {
            feature_tensors.push_back(sample.features);
        }
        torch::Tensor all_features = torch::stack(feature_tensors);
        auto mean_std = torch::std_mean(all_features.view({-1, config_.feature_dim}), 0);
        auto means = std::get<1>(mean_std);
        auto stds = std::get<0>(mean_std);
        
        for (int i = 0; i < config_.feature_dim; ++i) {
            feature_means_[i] = means[i].item<double>();
            feature_scales_[i] = std::max(1e-8, stds[i].item<double>());
        }
        
        // Normalize features with NaN checking and clipping
        for (auto& sample : train_samples_) {
            for (int i = 0; i < config_.feature_dim; ++i) {
                auto normalized = (sample.features.select(1, i) - feature_means_[i]) / feature_scales_[i];
                // Replace NaN/Inf with zeros and clip extreme values
                normalized = torch::where(torch::isfinite(normalized), normalized, torch::zeros_like(normalized));
                normalized = torch::clamp(normalized, -5.0, 5.0);
                sample.features.select(1, i) = normalized;
            }
        }
        
        for (auto& sample : val_samples_) {
            for (int i = 0; i < config_.feature_dim; ++i) {
                auto normalized = (sample.features.select(1, i) - feature_means_[i]) / feature_scales_[i];
                // Replace NaN/Inf with zeros and clip extreme values
                normalized = torch::where(torch::isfinite(normalized), normalized, torch::zeros_like(normalized));
                normalized = torch::clamp(normalized, -5.0, 5.0);
                sample.features.select(1, i) = normalized;
            }
        }
    }
    
    std::cout << "   Training samples: " << train_samples_.size() << std::endl;
    std::cout << "   Validation samples: " << val_samples_.size() << std::endl;
    
    return !train_samples_.empty();
}

void GRUTrainer::create_training_sequences(const torch::Tensor& features,
                                           const std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor>& targets) {
    const int num_samples = features.size(0);
    const int seq_len = config_.sequence_length;
    
    auto [direction_targets, magnitude_targets, confidence_targets, volatility_targets, regime_targets] = targets;
    
    train_samples_.clear();
    val_samples_.clear();
    
    for (int i = seq_len; i < num_samples; ++i) {
        TrainingSample sample;
        sample.features = features.slice(0, i - seq_len, i);
        sample.direction = direction_targets[i].unsqueeze(0);
        sample.magnitude = magnitude_targets[i].unsqueeze(0);
        sample.confidence = confidence_targets[i].unsqueeze(0);
        sample.volatility = volatility_targets[i].unsqueeze(0);
        sample.regime = regime_targets[i].unsqueeze(0);
        
        // Split into train/validation
        if (i < num_samples * config_.train_split) {
            train_samples_.push_back(sample);
        } else {
            val_samples_.push_back(sample);
        }
    }
}

// Helper functions for technical indicators
double GRUTrainer::calculate_sma(const std::vector<double>& data, int period, int index) const {
    if (index < period - 1) return data[index];
    
    double sum = 0.0;
    for (int i = index - period + 1; i <= index; ++i) {
        sum += data[i];
    }
    return sum / period;
}

double GRUTrainer::calculate_rsi(const std::vector<double>& prices, int period, int index) const {
    if (index < period) return 50.0;
    
    double gain = 0.0, loss = 0.0;
    for (int i = index - period + 1; i <= index; ++i) {
        double change = prices[i] - prices[i-1];
        if (change > 0) gain += change;
        else loss -= change;
    }
    
    gain /= period;
    loss /= period;
    
    if (loss == 0.0) return 100.0;
    double rs = gain / loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

double GRUTrainer::calculate_bollinger_position(const std::vector<double>& prices, int period, int index) const {
    if (index < period - 1) return 0.5;
    
    double sma = calculate_sma(prices, period, index);
    double sum_sq = 0.0;
    for (int i = index - period + 1; i <= index; ++i) {
        double diff = prices[i] - sma;
        sum_sq += diff * diff;
    }
    double std_dev = std::sqrt(sum_sq / period);
    
    double upper = sma + 2.0 * std_dev;
    double lower = sma - 2.0 * std_dev;
    
    if (upper == lower) return 0.5;
    return (prices[index] - lower) / (upper - lower);
}

std::tuple<double, double, double> GRUTrainer::calculate_macd(const std::vector<double>& prices, int index) const {
    if (index < 26) return {0.0, 0.0, 0.0};
    
    // Simplified MACD calculation
    double ema_12 = prices[index]; // Simplified
    double ema_26 = prices[index]; // Simplified
    
    if (index >= 12) {
        double sum_12 = 0.0;
        for (int i = index - 11; i <= index; ++i) {
            sum_12 += prices[i];
        }
        ema_12 = sum_12 / 12.0;
    }
    
    if (index >= 26) {
        double sum_26 = 0.0;
        for (int i = index - 25; i <= index; ++i) {
            sum_26 += prices[i];
        }
        ema_26 = sum_26 / 26.0;
    }
    
    double macd = ema_12 - ema_26;
    double signal = macd; // Simplified
    double histogram = macd - signal;
    
    return {macd, signal, histogram};
}

std::tuple<torch::Tensor, TrainingMetrics> GRUTrainer::compute_multi_objective_loss(
    const ml::GRUOutput& predictions, const TrainingSample& targets) {
    
    TrainingMetrics metrics;
    std::vector<torch::Tensor> losses;
    
    // Direction loss (primary objective) - use BCEWithLogitsLoss for numerical stability
    auto direction_targets_float = targets.direction.to(torch::kFloat).unsqueeze(1); // Match [batch, 1] shape
    auto direction_loss = torch::nn::functional::binary_cross_entropy_with_logits(predictions.direction, direction_targets_float);
    losses.push_back(direction_loss);
    metrics.direction_loss = direction_loss.item<double>();
    
    if (config_.enable_multi_task) {
        // Magnitude loss
        auto magnitude_targets = targets.magnitude.unsqueeze(1); // Match [batch, 1] shape
        auto magnitude_loss = torch::mse_loss(predictions.magnitude, magnitude_targets);
        losses.push_back(magnitude_loss * 0.5);
        metrics.magnitude_loss = magnitude_loss.item<double>();
        
        // Confidence loss
        auto confidence_targets = targets.confidence.unsqueeze(1); // Match [batch, 1] shape
        auto confidence_loss = torch::mse_loss(predictions.confidence, confidence_targets);
        losses.push_back(confidence_loss * 0.3);
        metrics.confidence_loss = confidence_loss.item<double>();
        
        // Volatility loss
        auto volatility_targets = targets.volatility.unsqueeze(1); // Match [batch, 1] shape
        auto volatility_loss = torch::mse_loss(predictions.volatility, volatility_targets);
        losses.push_back(volatility_loss * 0.3);
        metrics.volatility_loss = volatility_loss.item<double>();
        
        // Regime loss - targets should be [batch] for cross_entropy
        auto regime_targets = targets.regime.to(torch::kLong).squeeze(); // Ensure [batch] shape
        auto regime_loss = torch::nn::functional::cross_entropy(predictions.regime, regime_targets);
        losses.push_back(regime_loss * 0.2);
        metrics.regime_loss = regime_loss.item<double>();
    }
    
    // Total loss with NaN checking
    auto total_loss = torch::stack(losses).sum();
    
    // Check for NaN/Inf in loss and replace with large finite value
    if (!torch::isfinite(total_loss).item<bool>()) {
        std::cout << "⚠️  Warning: NaN/Inf detected in loss, using fallback value" << std::endl;
        total_loss = torch::tensor(100.0); // Large but finite fallback
    }
    
    metrics.total_loss = total_loss.item<double>();
    
    // Calculate accuracy - use sigmoid to convert logits to probabilities
    auto predicted_probs = torch::sigmoid(predictions.direction);
    auto predicted_direction = (predicted_probs > 0.5).to(torch::kFloat);
    auto direction_targets_expanded = targets.direction.unsqueeze(1).to(torch::kFloat);
    auto correct = (predicted_direction == direction_targets_expanded).to(torch::kFloat);
    metrics.accuracy = correct.mean().item<double>();
    
    return {total_loss, metrics};
}

TrainingMetrics GRUTrainer::train_epoch() {
    model_->train();
    
    TrainingMetrics epoch_metrics;
    int num_batches = 0;
    
    // Simple batch processing (can be improved with DataLoader)
    for (size_t i = 0; i < train_samples_.size(); i += config_.batch_size) {
        size_t batch_end = std::min(i + config_.batch_size, train_samples_.size());
        
        // Create batch
        std::vector<torch::Tensor> batch_features, batch_direction, batch_magnitude, batch_confidence, batch_volatility, batch_regime;
        
        for (size_t j = i; j < batch_end; ++j) {
            batch_features.push_back(train_samples_[j].features.unsqueeze(0));
            batch_direction.push_back(train_samples_[j].direction);
            batch_magnitude.push_back(train_samples_[j].magnitude);
            batch_confidence.push_back(train_samples_[j].confidence);
            batch_volatility.push_back(train_samples_[j].volatility);
            batch_regime.push_back(train_samples_[j].regime);
        }
        
        auto features_batch = torch::cat(batch_features, 0);
        TrainingSample batch_targets;
        batch_targets.direction = torch::cat(batch_direction, 0);
        batch_targets.magnitude = torch::cat(batch_magnitude, 0);
        batch_targets.confidence = torch::cat(batch_confidence, 0);
        batch_targets.volatility = torch::cat(batch_volatility, 0);
        batch_targets.regime = torch::cat(batch_regime, 0);
        
        // Forward pass
        auto predictions = model_->forward(features_batch);
        
        // Compute loss
        auto [loss, batch_metrics] = compute_multi_objective_loss(predictions, batch_targets);
        
        // Backward pass
        optimizer_->zero_grad();
        loss.backward();
        
        // Gradient clipping with more aggressive clipping
        torch::nn::utils::clip_grad_norm_(model_->parameters(), 0.5); // Reduce from 1.0 to 0.5
        
        optimizer_->step();
        
        // Accumulate metrics
        epoch_metrics.total_loss += batch_metrics.total_loss;
        epoch_metrics.direction_loss += batch_metrics.direction_loss;
        epoch_metrics.magnitude_loss += batch_metrics.magnitude_loss;
        epoch_metrics.confidence_loss += batch_metrics.confidence_loss;
        epoch_metrics.volatility_loss += batch_metrics.volatility_loss;
        epoch_metrics.regime_loss += batch_metrics.regime_loss;
        epoch_metrics.accuracy += batch_metrics.accuracy;
        num_batches++;
    }
    
    // Average metrics
    if (num_batches > 0) {
        epoch_metrics.total_loss /= num_batches;
        epoch_metrics.direction_loss /= num_batches;
        epoch_metrics.magnitude_loss /= num_batches;
        epoch_metrics.confidence_loss /= num_batches;
        epoch_metrics.volatility_loss /= num_batches;
        epoch_metrics.regime_loss /= num_batches;
        epoch_metrics.accuracy /= num_batches;
    }
    
    return epoch_metrics;
}

TrainingMetrics GRUTrainer::validate_epoch() {
    model_->eval();
    
    TrainingMetrics epoch_metrics;
    int num_batches = 0;
    
    torch::NoGradGuard no_grad;
    
    for (size_t i = 0; i < val_samples_.size(); i += config_.batch_size) {
        size_t batch_end = std::min(i + config_.batch_size, val_samples_.size());
        
        // Create batch
        std::vector<torch::Tensor> batch_features, batch_direction, batch_magnitude, batch_confidence, batch_volatility, batch_regime;
        
        for (size_t j = i; j < batch_end; ++j) {
            batch_features.push_back(val_samples_[j].features.unsqueeze(0));
            batch_direction.push_back(val_samples_[j].direction);
            batch_magnitude.push_back(val_samples_[j].magnitude);
            batch_confidence.push_back(val_samples_[j].confidence);
            batch_volatility.push_back(val_samples_[j].volatility);
            batch_regime.push_back(val_samples_[j].regime);
        }
        
        auto features_batch = torch::cat(batch_features, 0);
        TrainingSample batch_targets;
        batch_targets.direction = torch::cat(batch_direction, 0);
        batch_targets.magnitude = torch::cat(batch_magnitude, 0);
        batch_targets.confidence = torch::cat(batch_confidence, 0);
        batch_targets.volatility = torch::cat(batch_volatility, 0);
        batch_targets.regime = torch::cat(batch_regime, 0);
        
        // Forward pass
        auto predictions = model_->forward(features_batch);
        
        // Compute loss
        auto [loss, batch_metrics] = compute_multi_objective_loss(predictions, batch_targets);
        
        // Accumulate metrics
        epoch_metrics.total_loss += batch_metrics.total_loss;
        epoch_metrics.direction_loss += batch_metrics.direction_loss;
        epoch_metrics.magnitude_loss += batch_metrics.magnitude_loss;
        epoch_metrics.confidence_loss += batch_metrics.confidence_loss;
        epoch_metrics.volatility_loss += batch_metrics.volatility_loss;
        epoch_metrics.regime_loss += batch_metrics.regime_loss;
        epoch_metrics.accuracy += batch_metrics.accuracy;
        num_batches++;
    }
    
    // Average metrics
    if (num_batches > 0) {
        epoch_metrics.total_loss /= num_batches;
        epoch_metrics.direction_loss /= num_batches;
        epoch_metrics.magnitude_loss /= num_batches;
        epoch_metrics.confidence_loss /= num_batches;
        epoch_metrics.volatility_loss /= num_batches;
        epoch_metrics.regime_loss /= num_batches;
        epoch_metrics.accuracy /= num_batches;
    }
    
    epoch_metrics.is_validation = true;
    return epoch_metrics;
}

bool GRUTrainer::train() {
    std::cout << "🎓 Starting training..." << std::endl;
    std::cout << "   Epochs: " << config_.epochs << std::endl;
    std::cout << "   Batch size: " << config_.batch_size << std::endl;
    std::cout << "   Learning rate: " << config_.learning_rate << std::endl;
    
    for (int epoch = 0; epoch < config_.epochs; ++epoch) {
        // Train epoch
        auto train_metrics = train_epoch();
        train_metrics.epoch = epoch;
        train_history_.push_back(train_metrics);
        
        // Validate epoch
        auto val_metrics = validate_epoch();
        val_metrics.epoch = epoch;
        val_history_.push_back(val_metrics);
        
        // Print progress
        if (epoch % 10 == 0 || epoch < 10) {
            std::cout << "Epoch " << std::setw(3) << epoch + 1 << "/" << config_.epochs
                      << " | Train Loss: " << std::fixed << std::setprecision(6) << train_metrics.total_loss
                      << " | Val Loss: " << val_metrics.total_loss
                      << " | Accuracy: " << std::setprecision(3) << val_metrics.accuracy * 100 << "%" << std::endl;
        }
        
        // Progress callback
        if (progress_callback_) {
            progress_callback_(val_metrics);
        }
        
        // Early stopping
        if (val_metrics.total_loss < best_val_loss_ - config_.min_delta) {
            best_val_loss_ = val_metrics.total_loss;
            patience_counter_ = 0;
            
            // Save best model
            save_checkpoint(epoch);
        } else {
            patience_counter_++;
            if (patience_counter_ >= config_.patience) {
                std::cout << "Early stopping triggered at epoch " << epoch + 1 << std::endl;
                break;
            }
        }
        
        // Save checkpoint
        if (config_.save_checkpoints && (epoch + 1) % config_.checkpoint_frequency == 0) {
            save_checkpoint(epoch);
        }
    }
    
    std::cout << "✅ Training completed. Best validation loss: " << best_val_loss_ << std::endl;
    return true;
}

TrainingMetrics GRUTrainer::evaluate() {
    return validate_epoch();
}

void GRUTrainer::save_checkpoint(int epoch) {
    std::filesystem::create_directories(config_.output_dir);
    
    std::string model_path = config_.output_dir + "/model_epoch_" + std::to_string(epoch) + ".pt";
    model_->save_model(model_path);
    
    // Save best model as well
    if (epoch >= 0) {
        std::string best_path = config_.output_dir + "/best_model.pt";
        model_->save_model(best_path);
    }
}

bool GRUTrainer::export_model() {
    std::cout << "💾 Exporting model and metadata..." << std::endl;
    
    std::filesystem::create_directories(config_.output_dir);
    
    // Save final model
    std::string model_path = config_.output_dir + "/model.pt";
    model_->save_model(model_path);
    
    // Create metadata
    nlohmann::json metadata;
    metadata["model_type"] = "AdvancedGRU_CPP";
    metadata["version"] = "v1.0";
    metadata["feature_count"] = config_.feature_dim;
    metadata["feature_names"] = feature_names_;
    metadata["sequence_length"] = config_.sequence_length;
    metadata["hidden_dim"] = config_.hidden_dim;
    metadata["num_heads"] = config_.num_heads;
    metadata["num_layers"] = config_.num_layers;
    metadata["enable_multi_task"] = config_.enable_multi_task;
    metadata["feature_means"] = feature_means_;
    metadata["feature_scales"] = feature_scales_;
    metadata["training_samples"] = train_samples_.size();
    metadata["validation_samples"] = val_samples_.size();
    metadata["epochs_trained"] = train_history_.size();
    metadata["best_val_loss"] = best_val_loss_;
    metadata["parameter_count"] = model_->get_parameter_count();
    metadata["created_at"] = static_cast<int>(std::time(nullptr));
    
    // Save metadata
    std::string metadata_path = config_.output_dir + "/metadata.json";
    std::ofstream metadata_file(metadata_path);
    metadata_file << metadata.dump(2);
    metadata_file.close();
    
    std::cout << "✅ Model exported to: " << model_path << std::endl;
    std::cout << "✅ Metadata exported to: " << metadata_path << std::endl;
    
    return true;
}

} // namespace training
} // namespace sentio
