#include "strategy/transformer_model.h"
#include "training/quality_enforced_loss.h"
#include "common/utils.h"
#include "features/unified_feature_engine.h"
#include "common/types.h"
#include <torch/torch.h>
#include <iostream>
#include <vector>
#include <limits>

/**
 * Real Data TFM Trainer
 * 
 * This trainer uses actual QQQ market data instead of fake torch::randn() data.
 * It properly integrates with UnifiedFeatureEngine to generate 126-dimensional features
 * and creates meaningful prediction targets from real market movements.
 */

struct RealMarketDataLoader {
    std::vector<torch::Tensor> feature_sequences;
    std::vector<torch::Tensor> targets;
    int batch_size = 64;
    int current_pos = 0;
    
    RealMarketDataLoader(const std::string& data_path, int sequence_length = 50, double train_split = 0.8) {
        load_and_process_data(data_path, sequence_length, train_split);
    }
    
    void load_and_process_data(const std::string& data_path, int sequence_length, double train_split) {
        // Load QQQ market data
        std::cout << "Loading market data from: " << data_path << std::endl;
        auto bars = sentio::utils::read_csv_data(data_path);
        
        if (bars.empty()) {
            std::cerr << "ERROR: No market data loaded from " << data_path << std::endl;
            return;
        }
        
        std::cout << "Loaded " << bars.size() << " bars from market data" << std::endl;
        
        // Initialize feature engine
        sentio::features::UnifiedFeatureEngine::Config config;
        sentio::features::UnifiedFeatureEngine feature_engine(config);
        
        std::vector<std::vector<double>> all_features;
        std::vector<double> all_returns;
        
        // Process each bar and generate features
        std::cout << "Processing bars and generating features..." << std::endl;
        for (size_t i = 0; i < bars.size(); ++i) {
            feature_engine.update(bars[i]);
            
            if (feature_engine.is_ready()) {
                auto features = feature_engine.get_features();
                all_features.push_back(features);
                
                // Create target: next bar's return (close price change)
                if (i + 1 < bars.size()) {
                    double current_price = bars[i].close;
                    double next_price = bars[i + 1].close;
                    double return_pct = (next_price - current_price) / current_price;
                    all_returns.push_back(return_pct);
                } else {
                    all_returns.push_back(0.0); // Last bar has no next return
                }
            }
        }
        
        std::cout << "Generated " << all_features.size() << " feature vectors with " 
                  << (all_features.empty() ? 0 : all_features[0].size()) << " dimensions each" << std::endl;
        
        // Create sequences for transformer
        create_sequences(all_features, all_returns, sequence_length);
        
        // Split into train/validation
        split_data(train_split);
        
        std::cout << "Created " << feature_sequences.size() << " training sequences" << std::endl;
    }
    
    void create_sequences(const std::vector<std::vector<double>>& features, 
                         const std::vector<double>& returns, 
                         int sequence_length) {
        
        if (features.size() < sequence_length + 1) {
            std::cerr << "ERROR: Not enough data for sequences. Need at least " 
                      << (sequence_length + 1) << " samples, got " << features.size() << std::endl;
            return;
        }
        
        int feature_dim = features.empty() ? 0 : features[0].size();
        
        // Create overlapping sequences
        for (size_t i = 0; i <= features.size() - sequence_length - 1; ++i) {
            // Create feature sequence tensor [sequence_length, feature_dim]
            torch::Tensor sequence = torch::zeros({sequence_length, feature_dim});
            
            for (int j = 0; j < sequence_length; ++j) {
                for (int k = 0; k < feature_dim; ++k) {
                    sequence[j][k] = features[i + j][k];
                }
            }
            
            // Target is the return after the sequence
            torch::Tensor target = torch::tensor(returns[i + sequence_length]);
            
            feature_sequences.push_back(sequence);
            targets.push_back(target);
        }
    }
    
    void split_data(double train_split) {
        size_t total_sequences = feature_sequences.size();
        size_t train_size = static_cast<size_t>(total_sequences * train_split);
        
        // Keep only training data in this loader
        // (In a full implementation, we'd create separate loaders for train/val)
        feature_sequences.resize(train_size);
        targets.resize(train_size);
    }
    
    std::optional<std::pair<torch::Tensor, torch::Tensor>> next_batch() {
        if (current_pos >= feature_sequences.size()) {
            current_pos = 0;
            return std::nullopt; // Epoch end
        }
        
        int actual_batch_size = std::min(batch_size, 
                                        static_cast<int>(feature_sequences.size() - current_pos));
        
        // Stack sequences into batch tensor [batch_size, sequence_length, feature_dim]
        std::vector<torch::Tensor> batch_sequences;
        std::vector<torch::Tensor> batch_targets;
        
        for (int i = 0; i < actual_batch_size; ++i) {
            batch_sequences.push_back(feature_sequences[current_pos + i]);
            batch_targets.push_back(targets[current_pos + i]);
        }
        
        torch::Tensor batch_features = torch::stack(batch_sequences);
        torch::Tensor batch_targets_tensor = torch::stack(batch_targets).unsqueeze(1);
        
        current_pos += actual_batch_size;
        
        return {{batch_features, batch_targets_tensor}};
    }
    
    size_t size() const {
        return feature_sequences.size();
    }
};

int main(int argc, char** argv) {
    std::cout << "🚀 Starting TFM Training with Real Market Data..." << std::endl;
    
    // --- Parse Command-Line Arguments ---
    std::string data_path = "data/equities/QQQ_RTH_NH.csv";
    int num_epochs = 10;
    double learning_rate = 1e-4;
    double dropout_rate = 0.1;
    int sequence_length = 50;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--data" && i + 1 < argc) {
            data_path = argv[++i];
        } else if (arg == "--epochs" && i + 1 < argc) {
            num_epochs = std::stoi(argv[++i]);
        } else if (arg == "--lr" && i + 1 < argc) {
            learning_rate = std::stod(argv[++i]);
        } else if (arg == "--dropout" && i + 1 < argc) {
            dropout_rate = std::stod(argv[++i]);
        } else if (arg == "--sequence" && i + 1 < argc) {
            sequence_length = std::stoi(argv[++i]);
        }
    }
    
    std::cout << "Training Parameters:" << std::endl;
    std::cout << "  Data Path: " << data_path << std::endl;
    std::cout << "  Epochs: " << num_epochs << std::endl;
    std::cout << "  Learning Rate: " << learning_rate << std::endl;
    std::cout << "  Dropout: " << dropout_rate << std::endl;
    std::cout << "  Sequence Length: " << sequence_length << std::endl;
    
    // --- Load Real Market Data ---
    RealMarketDataLoader data_loader(data_path, sequence_length, 0.8);
    
    if (data_loader.size() == 0) {
        std::cerr << "❌ Failed to load market data. Exiting." << std::endl;
        return 1;
    }
    
    // --- Initialize Model ---
    // Use 126 features (from UnifiedFeatureEngine) instead of fake 128
    sentio::TransformerModelConfig model_config;
    model_config.feature_dim = 126;  // ✅ FIXED: Match UnifiedFeatureEngine output
    model_config.sequence_length = sequence_length;
    model_config.d_model = 256;
    model_config.nhead = 8;
    model_config.num_encoder_layers = 4;
    model_config.dim_feedforward = 1024;
    model_config.dropout = dropout_rate;
    
    auto model = sentio::TransformerModel(model_config);
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(learning_rate));
    sentio::training::QualityEnforcedLoss loss_fn;
    
    double best_loss = std::numeric_limits<double>::max();
    
    // --- Training Loop ---
    std::cout << "\n🎯 Starting training on real market data..." << std::endl;
    
    for (int epoch = 0; epoch < num_epochs; ++epoch) {
        model->train();
        double total_loss = 0.0;
        int num_batches = 0;
        
        while (auto batch_opt = data_loader.next_batch()) {
            auto [features, targets] = *batch_opt;
            
            optimizer.zero_grad();
            auto [predictions, log_var] = model->forward(features);
            
            auto loss_components = loss_fn.compute_loss(predictions, log_var, targets);
            loss_components.total.backward();
            optimizer.step();
            
            total_loss += loss_components.total.item<double>();
            num_batches++;
        }
        
        double avg_loss = total_loss / num_batches;
        
        std::cout << "Epoch " << epoch << "/" << num_epochs 
                  << " | Avg Loss: " << avg_loss << std::endl;
        
        // Save best model
        if (avg_loss < best_loss) {
            best_loss = avg_loss;
            std::string model_filename = "tfm_real_data_model.pt";
            torch::save(model, model_filename);
            std::cout << "  -> New best model saved: " << model_filename << std::endl;
        }
    }
    
    std::cout << "\n✅ Training complete!" << std::endl;
    std::cout << "Best loss: " << best_loss << std::endl;
    std::cout << "Model saved as: tfm_real_data_model.pt" << std::endl;
    
    return 0;
}
