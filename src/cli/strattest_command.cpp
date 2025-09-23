#include "cli/strattest_command.h"
#include "strategy/sigor_strategy.h"
#include "strategy/sigor_config.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <unistd.h>
#include <sstream>
#include <vector>

// Conditional includes for optional strategies
#ifdef TORCH_AVAILABLE
#include "strategy/transformer_strategy.h"
#include "strategy/optimized_gru_strategy.h"
#endif

// Check if momentum scalper exists
#ifdef MOMENTUM_SCALPER_AVAILABLE
#include "strategy/momentum_scalper.h"
#endif

namespace sentio {
namespace cli {

int StrattestCommand::execute(const std::vector<std::string>& args) {
    // Parse command-line arguments with intelligent defaults
    std::string dataset = get_arg(args, "--dataset");
    std::string output = get_arg(args, "--out");
    const std::string strategy = get_arg(args, "--strategy", "sigor");
    const std::string format = get_arg(args, "--format", "jsonl");
    const std::string config_path = get_arg(args, "--config", "");
    
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
    if (strategy == "sigor") {
        return execute_sigor_strategy(dataset, output, config_path, args);
    } else if (strategy == "gru") {
        // Legacy GRU strategy removed - use cpp_gru instead
        std::cerr << "Error: Legacy 'gru' strategy has been removed for performance.\n";
        std::cerr << "Use 'cpp_gru' for the optimized GRU implementation.\n";
        return 1;
    } else if (strategy == "cpp_gru") {
#ifdef TORCH_AVAILABLE
        return execute_cpp_gru_strategy(dataset, output, args);
#else
        std::cerr << "Error: C++ GRU strategy not available (LibTorch not found)\n";
        return 1;
#endif
    } else if (strategy == "transformer") {
#ifdef TORCH_AVAILABLE
        return execute_transformer_strategy(dataset, output, args);
#else
        std::cerr << "Error: Transformer strategy not available (LibTorch not found)\n";
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
        std::cerr << "Available strategies: sigor";
#ifdef TORCH_AVAILABLE
        std::cerr << ", gru, transformer";
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
        
        // Process dataset to generate signals
        std::cout << "Processing dataset: " << dataset << std::endl;
        auto signals = sigor->process_dataset(dataset, cfg.name, {});
        
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
        
        auto signals = cpp_gru->process_dataset(dataset, base_cfg.name, {});
        
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
            std::cout << "🔧 Performance mode: Processing only " << blocks_to_process << " blocks (~" << (blocks_to_process * 480) << " bars)" << std::endl;
            std::cout << "⚠️  Large transformer model (4.78M params) - limiting dataset for reasonable inference time" << std::endl;
            
            // Create limited dataset for testing (correct: 480 bars per block)
            std::string limited_dataset = create_limited_dataset(dataset, blocks_to_process * 480);
            signals = transformer->process_dataset(limited_dataset, base_cfg.name, {});
            
            // Clean up limited dataset
            std::remove(limited_dataset.c_str());
            
            std::cout << "⚡ Processed " << signals.size() << " signals from limited dataset" << std::endl;
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
    if (strategy != "sigor" && strategy != "cpp_gru" && strategy != "transformer" &&
        strategy != "momentum" && strategy != "scalper") {
        std::cerr << "Error: Invalid strategy '" << strategy << "'" << std::endl;
        std::cerr << "Available strategies: sigor, cpp_gru, transformer, momentum" << std::endl;
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

std::string StrattestCommand::create_limited_dataset(const std::string& original_dataset, int test_bars) const {
    std::string limited_dataset = "/tmp/limited_dataset_" + std::to_string(getpid()) + ".csv";
    
    // Read all lines to determine total count
    std::ifstream input(original_dataset);
    std::vector<std::string> all_lines;
    std::string line;
    
    while (std::getline(input, line)) {
        all_lines.push_back(line);
    }
    input.close();
    
    if (all_lines.empty()) {
        std::cerr << "ERROR: Empty dataset file" << std::endl;
        return limited_dataset;
    }
    
    // Calculate proper backtesting range
    const int warmup_bars = 64; // GRU sequence length
    const int total_data_rows = all_lines.size() - 1; // Exclude header
    const int total_bars_needed = test_bars + warmup_bars;
    
    std::cout << "📊 BACKTESTING DATASET CREATION:" << std::endl;
    std::cout << "  Total available bars: " << total_data_rows << std::endl;
    std::cout << "  Test bars requested: " << test_bars << std::endl;
    std::cout << "  Warmup bars needed: " << warmup_bars << std::endl;
    std::cout << "  Total bars needed: " << total_bars_needed << std::endl;
    
    if (total_bars_needed > total_data_rows) {
        std::cout << "  ⚠️  Requested more bars than available, using all data" << std::endl;
        // Use all available data
        std::ofstream output(limited_dataset);
        for (const auto& line : all_lines) {
            output << line << std::endl;
        }
        output.close();
        std::cout << "  📅 Using full dataset: " << total_data_rows << " bars" << std::endl;
        return limited_dataset;
    }
    
    // Calculate start position (from the END, going backwards)
    const int start_row = total_data_rows - total_bars_needed + 1; // +1 because we need to include header
    const int end_row = total_data_rows;
    
    // Extract date range for display
    std::string start_date = "unknown";
    std::string end_date = "unknown";
    
    if (start_row > 0 && start_row < static_cast<int>(all_lines.size())) {
        std::istringstream ss(all_lines[start_row]);
        std::getline(ss, start_date, ','); // First column is ts_utc
    }
    
    if (end_row > 0 && end_row < static_cast<int>(all_lines.size())) {
        std::istringstream ss(all_lines[end_row]);
        std::getline(ss, end_date, ','); // First column is ts_utc
    }
    
    std::cout << "  📅 Test period: " << start_date << " to " << end_date << std::endl;
    std::cout << "  📍 Using rows " << start_row << " to " << end_row << " (most recent data)" << std::endl;
    
    // Create limited dataset with proper backtesting range
    std::ofstream output(limited_dataset);
    
    // Copy header
    output << all_lines[0] << std::endl;
    
    // Copy the selected range (warmup + test period from END of dataset)
    for (int i = start_row; i <= end_row && i < static_cast<int>(all_lines.size()); ++i) {
        output << all_lines[i] << std::endl;
    }
    
    output.close();
    
    const int actual_bars = end_row - start_row + 1;
    std::cout << "  ✅ Created backtesting dataset with " << actual_bars << " bars" << std::endl;
    std::cout << "  🔄 First " << warmup_bars << " bars for warmup, remaining " << (actual_bars - warmup_bars) << " bars for testing" << std::endl;
    
    return limited_dataset;
}

} // namespace cli
} // namespace sentio
