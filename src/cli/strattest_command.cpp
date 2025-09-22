#include "cli/strattest_command.h"
#include "strategy/sigor_strategy.h"
#include "strategy/sigor_config.h"
#include <iostream>
#include <filesystem>

// Conditional includes for optional strategies
#ifdef TORCH_AVAILABLE
#include "strategy/gru_strategy.h"
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
#ifdef TORCH_AVAILABLE
        return execute_gru_strategy(dataset, output, args);
#else
        std::cerr << "Error: GRU strategy not available (LibTorch not found)\n";
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
        std::cerr << ", gru";
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
    std::cout << "  --strategy NAME    Strategy to use: sigor, gru, momentum (default: sigor)\n";
    std::cout << "  --format FORMAT    Output format: jsonl (default: jsonl)\n";
    std::cout << "  --config PATH      Strategy configuration file (optional)\n";
    std::cout << "  --blocks N         Number of blocks to process (default: all)\n";
    std::cout << "  --mode MODE        Processing mode: historical, live (default: historical)\n";
    std::cout << "  --help, -h         Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sentio_cli strattest\n";
    std::cout << "  sentio_cli strattest --strategy gru --blocks 20\n";
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

int StrattestCommand::execute_gru_strategy(const std::string& dataset,
                                         const std::string& output,
                                         const std::vector<std::string>& args) {
#ifdef TORCH_AVAILABLE
    try {
        // Initialize GRU strategy
        sentio::StrategyComponent::StrategyConfig cfg;
        cfg.name = "gru";
        cfg.version = "0.1";
        cfg.warmup_bars = 64; // GRU requires longer warmup for sequence
        
        auto gru = std::make_unique<sentio::GRUStrategy>(cfg);
        
        // Process dataset
        std::cout << "Processing dataset with GRU strategy: " << dataset << std::endl;
        auto signals = gru->process_dataset(dataset, cfg.name, {});
        
        // Add metadata
        for (auto& signal : signals) {
            signal.metadata["market_data_path"] = dataset;
            signal.metadata["model_type"] = "GRU";
        }
        
        // Export signals
        bool success = gru->export_signals(signals, output, "jsonl");
        if (!success) {
            std::cerr << "ERROR: Failed exporting GRU signals to " << output << std::endl;
            return 2;
        }
        
        std::cout << "✅ Exported " << signals.size() << " GRU signals to " << output << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: GRU strategy execution failed: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cerr << "ERROR: GRU strategy not available (LibTorch not found)" << std::endl;
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
    if (strategy != "sigor" && strategy != "gru" && 
        strategy != "momentum" && strategy != "scalper") {
        std::cerr << "Error: Invalid strategy '" << strategy << "'" << std::endl;
        std::cerr << "Available strategies: sigor, gru, momentum" << std::endl;
        return false;
    }
    
    return true;
}

} // namespace cli
} // namespace sentio
