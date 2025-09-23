#include "training/gru_trainer.h"
#include "common/utils.h"
#include <iostream>
#include <chrono>
#include <iomanip>

using namespace sentio;
using namespace sentio::training;

void print_banner() {
    std::cout << "🚀 SENTIO ADVANCED GRU TRAINER (C++)" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "📊 Target: 10% return on 20 blocks (vs current 2.87%)" << std::endl;
    std::cout << "🎯 Features: Multi-task learning, attention, 53+ features" << std::endl;
    std::cout << "⚡ Pure C++ implementation - no Python/TorchScript issues" << std::endl;
    std::cout << std::endl;
}

void print_config(const ml::GRUConfig& config) {
    std::cout << "🧠 Model Configuration:" << std::endl;
    std::cout << "   Feature dim: " << config.feature_dim << std::endl;
    std::cout << "   Sequence length: " << config.sequence_length << std::endl;
    std::cout << "   Hidden dim: " << config.hidden_dim << std::endl;
    std::cout << "   Num layers: " << config.num_layers << std::endl;
    std::cout << "   Attention heads: " << config.num_heads << std::endl;
    std::cout << "   Multi-task: " << (config.enable_multi_task ? "enabled" : "disabled") << std::endl;
    std::cout << "   Dropout: " << config.dropout << std::endl;
    std::cout << std::endl;
    
    std::cout << "🎓 Training Configuration:" << std::endl;
    std::cout << "   Epochs: " << config.epochs << std::endl;
    std::cout << "   Batch size: " << config.batch_size << std::endl;
    std::cout << "   Learning rate: " << config.learning_rate << std::endl;
    std::cout << "   Weight decay: " << config.weight_decay << std::endl;
    std::cout << "   Patience: " << config.patience << std::endl;
    std::cout << "   Output dir: " << config.output_dir << std::endl;
    std::cout << std::endl;
}

struct TrainingArgs {
    std::string data_file = "data/equities/QQQ_RTH_NH.csv";
    std::string output_dir = "artifacts/GRU/cpp_trained";
    int epochs = 50;
    int batch_size = 64;
    int hidden_dim = 128;
    int num_heads = 8;
    bool enable_multi_task = true;
};

void print_help(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --data-file <path>     Path to market data CSV (default: data/equities/QQQ_RTH_NH.csv)" << std::endl;
    std::cout << "  --output-dir <path>    Output directory (default: artifacts/GRU/cpp_trained)" << std::endl;
    std::cout << "  --epochs <n>           Number of training epochs (default: 50)" << std::endl;
    std::cout << "  --batch-size <n>       Batch size (default: 64)" << std::endl;
    std::cout << "  --hidden-dim <n>       Hidden dimension (default: 128)" << std::endl;
    std::cout << "  --num-heads <n>        Number of attention heads (default: 8)" << std::endl;
    std::cout << "  --no-multi-task        Disable multi-task learning" << std::endl;
    std::cout << "  --help                 Show this help message" << std::endl;
}

TrainingArgs parse_arguments(int argc, char* argv[]) {
    TrainingArgs args;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--data-file" && i + 1 < argc) {
            args.data_file = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            args.output_dir = argv[++i];
        } else if (arg == "--epochs" && i + 1 < argc) {
            args.epochs = std::stoi(argv[++i]);
        } else if (arg == "--batch-size" && i + 1 < argc) {
            args.batch_size = std::stoi(argv[++i]);
        } else if (arg == "--hidden-dim" && i + 1 < argc) {
            args.hidden_dim = std::stoi(argv[++i]);
        } else if (arg == "--num-heads" && i + 1 < argc) {
            args.num_heads = std::stoi(argv[++i]);
        } else if (arg == "--no-multi-task") {
            args.enable_multi_task = false;
        } else if (arg == "--help") {
            print_help(argv[0]);
            exit(0);
        }
    }
    
    return args;
}

ml::GRUConfig create_config(const TrainingArgs& args) {
    ml::GRUConfig config;
    config.output_dir = args.output_dir;
    config.epochs = args.epochs;
    config.batch_size = args.batch_size;
    config.hidden_dim = args.hidden_dim;
    config.num_heads = args.num_heads;
    config.enable_multi_task = args.enable_multi_task;
    return config;
}

bool run_training_pipeline(GRUTrainer& trainer, const TrainingArgs& args) {
    // Load data
    std::cout << "📈 Loading market data..." << std::endl;
    if (!trainer.load_data(args.data_file)) {
        std::cerr << "❌ Failed to load data from: " << args.data_file << std::endl;
        return false;
    }
    
    // Prepare training data
    std::cout << "🔧 Preparing training data..." << std::endl;
    if (!trainer.prepare_training_data()) {
        std::cerr << "❌ Failed to prepare training data" << std::endl;
        return false;
    }
    
    // Train model
    std::cout << "🎓 Training model..." << std::endl;
    if (!trainer.train()) {
        std::cerr << "❌ Training failed" << std::endl;
        return false;
    }
    
    // Final evaluation
    std::cout << "📊 Final evaluation..." << std::endl;
    auto final_metrics = trainer.evaluate();
    std::cout << "   Final validation loss: " << final_metrics.total_loss << std::endl;
    std::cout << "   Final accuracy: " << final_metrics.accuracy * 100 << "%" << std::endl;
    
    // Export model
    std::cout << "💾 Exporting model..." << std::endl;
    if (!trainer.export_model()) {
        std::cerr << "❌ Failed to export model" << std::endl;
        return false;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    print_banner();
    
    // Parse command line arguments
    TrainingArgs args = parse_arguments(argc, argv);
    
    // Create configuration
    ml::GRUConfig config = create_config(args);
    print_config(config);
    
    // Create trainer
    GRUTrainer trainer(config);
    
    // Set progress callback
    trainer.set_progress_callback([](const TrainingMetrics& metrics) {
        if (metrics.epoch % 5 == 0) {
            std::cout << "   📊 Epoch " << metrics.epoch + 1 
                      << " - Val Loss: " << std::fixed << std::setprecision(6) << metrics.total_loss
                      << " - Accuracy: " << std::setprecision(1) << metrics.accuracy * 100 << "%" << std::endl;
        }
    });
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        if (!run_training_pipeline(trainer, args)) {
            return 1;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        
        std::cout << std::endl;
        std::cout << "🎉 TRAINING COMPLETED SUCCESSFULLY!" << std::endl;
        std::cout << "=================================" << std::endl;
        std::cout << "⏱️  Total time: " << duration.count() << " seconds" << std::endl;
        std::cout << "📁 Model saved to: " << config.output_dir << std::endl;
        std::cout << "🎯 Ready for C++ strategy integration!" << std::endl;
        std::cout << std::endl;
        std::cout << "🚀 Next steps:" << std::endl;
        std::cout << "   1. Test with: ./build/sentio_cli strattest gru --blocks 20" << std::endl;
        std::cout << "   2. Compare performance against other algorithms" << std::endl;
        std::cout << "   3. Run full trading pipeline" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Training failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
