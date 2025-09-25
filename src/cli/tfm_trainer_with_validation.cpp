#include "strategy/transformer_model.h"
#include "training/quality_enforced_loss.h"
#include "training/training_data_validator.h"
#include <torch/torch.h>
#include <iostream>
#include <limits>

// --- MOCK DATA LOADER (Now with a constructor that takes data) ---
struct MockDataLoader {
    torch::Tensor features, targets;
    int batch_size = 64;
    int current_pos = 0;

    MockDataLoader(torch::Tensor f, torch::Tensor t) : features(f), targets(t) {}

    std::optional<std::pair<torch::Tensor, torch::Tensor>> next_batch() {
        if (current_pos >= features.size(0)) {
            current_pos = 0;
            return std::nullopt; // Epoch end
        }
        auto f_batch = features.slice(0, current_pos, std::min(static_cast<int64_t>(current_pos + batch_size), features.size(0)));
        auto t_batch = targets.slice(0, current_pos, std::min(static_cast<int64_t>(current_pos + batch_size), targets.size(0)));
        current_pos += batch_size;
        return {{f_batch, t_batch}};
    }
};

// ✅ NEW FUNCTION: To run evaluation on the validation set
double run_validation(sentio::TransformerModel& model, MockDataLoader& val_loader) {
    model->eval(); // Set model to evaluation mode (disables dropout, etc.)
    torch::NoGradGuard no_grad; // Disables gradient calculation

    double total_val_loss = 0.0;
    int num_val_batches = 0;
    
    while (auto batch_opt = val_loader.next_batch()) {
        auto [features, targets] = *batch_opt;
        auto [predictions, log_var] = model->forward(features);
        
        // Use a simple MSE for validation loss; it's stable and fast.
        auto loss = torch::mse_loss(predictions.squeeze(), targets.squeeze());
        total_val_loss += loss.item<double>();
        num_val_batches++;
    }
    
    return total_val_loss / num_val_batches;
}

// --- MAIN TRAINER (ENHANCED WITH VALIDATION) ---
int main(int argc, char** argv) {
    std::cout << "🚀 Starting TFM Training with Validation..." << std::endl;

    // --- Default Hyperparameters ---
    double learning_rate = 1e-4;
    double dropout_rate = 0.1;
    int num_epochs = 2;  // Default epochs

    // --- Parse Command-Line Arguments ---
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--lr" && i + 1 < argc) {
            learning_rate = std::stod(argv[++i]);
        } else if (arg == "--dropout" && i + 1 < argc) {
            dropout_rate = std::stod(argv[++i]);
        } else if (arg == "--epochs" && i + 1 < argc) {
            num_epochs = std::stoi(argv[++i]);
        }
    }
    std::cout << "🧪 Training with LR: " << learning_rate << " | Dropout: " << dropout_rate << " | Epochs: " << num_epochs << std::endl;

    // --- 1. Load and Split Data ---
    // In a real app, you'd load from files. We'll create mock data here.
    auto all_features = torch::randn({10000, 50, 128});
    auto all_targets = torch::randn({10000}).unsqueeze(1);  // Ensure shape is [10000, 1]

    // Split: 70% train, 15% validation, 15% test
    long train_size = all_features.size(0) * 0.7;
    long val_size = all_features.size(0) * 0.15;
    
    auto train_features = all_features.slice(0, 0, train_size);
    auto train_targets = all_targets.slice(0, 0, train_size);
    
    auto val_features = all_features.slice(0, train_size, train_size + val_size);
    auto val_targets = all_targets.slice(0, train_size, train_size + val_size);

    // Test set would be the remainder (not used during training)

    // --- 2. Initialization ---
    sentio::TransformerModelConfig model_config;
    model_config.feature_dim = 128;
    model_config.d_model = 256;
    model_config.nhead = 8;
    model_config.num_encoder_layers = 4;
    model_config.dropout = dropout_rate;
    
    auto model = sentio::TransformerModel(model_config);
    torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(learning_rate));
    sentio::training::QualityEnforcedLoss loss_fn;
    
    MockDataLoader train_loader(train_features, train_targets);
    MockDataLoader val_loader(val_features, val_targets);
    
    const int NUM_EPOCHS = num_epochs;
    double best_validation_loss = std::numeric_limits<double>::max();

    // --- 3. Training Loop ---
    for (int epoch = 0; epoch < NUM_EPOCHS; ++epoch) {
        model->train(); // Set model to training mode
        double total_train_loss = 0;
        int num_train_batches = 0;

        while (auto batch_opt = train_loader.next_batch()) {
            auto [features, targets] = *batch_opt;
            optimizer.zero_grad();
            auto [predictions, log_var] = model->forward(features);
            auto components = loss_fn.compute_loss(predictions, log_var, targets);
            components.total.backward();
            optimizer.step();
            total_train_loss += components.total.item<double>();
            num_train_batches++;
        }

        // --- ✅ 4. VALIDATION STEP ---
        double avg_train_loss = total_train_loss / num_train_batches;
        double validation_loss = run_validation(model, val_loader);

        std::cout << "--- Epoch " << epoch << "/" << NUM_EPOCHS << " ---" 
                  << " | Avg Train Loss: " << avg_train_loss
                  << " | Validation Loss: " << validation_loss << std::endl;

        // ✅ Save the best model based on validation performance
        if (validation_loss < best_validation_loss) {
            best_validation_loss = validation_loss;
            
            // ✅ Save with unique name for this hyperparameter combination
            std::string model_filename = "model_lr" + std::to_string(learning_rate) + "_drop" + std::to_string(dropout_rate) + ".pt";
            torch::save(model, model_filename);
            std::cout << "  -> New best model found! Saved to " << model_filename << std::endl;
        }
    }

    std::cout << "\n✅ Training complete. Best model saved with validation loss: " << best_validation_loss << std::endl;
    return 0;
}
