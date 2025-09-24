#include "cli/trade_command.h"
#include "backend/backend_component.h"
#include "strategy/signal_output.h"
#include "common/utils.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace sentio {
namespace cli {

int TradeCommand::execute(const std::vector<std::string>& args) {
    // Show help if requested
    if (has_flag(args, "--help") || has_flag(args, "-h")) {
        show_help();
        return 0;
    }
    
    // Parse configuration
    TradeConfig config = parse_config(args);
    
    // Validate configuration
    if (!validate_config(config)) {
        return 1;
    }
    
    // Resolve signals file
    std::string signals_file = resolve_signals_file(config.signals_file);
    if (signals_file.empty()) {
        std::cerr << "ERROR: No signals found in data/signals" << std::endl;
        return 1;
    }
    
    // Update config with resolved signals file
    TradeConfig resolved_config = config;
    resolved_config.signals_file = signals_file;
    
    // Extract market data path
    std::string market_path = extract_market_data_path(signals_file);
    if (market_path.empty()) {
        std::cerr << "ERROR: Cannot extract market data path from signals" << std::endl;
        return 2;
    }
    
    // Execute trading
    return execute_trading(resolved_config, market_path);
}

void TradeCommand::show_help() const {
    std::cout << "Usage: sentio_cli trade [options]\n\n";
    std::cout << "Execute trades based on AI-generated signals with portfolio management.\n\n";
    std::cout << "Options:\n";
    std::cout << "  --signals PATH     Signals file or 'latest' (default: latest)\n";
    std::cout << "  --capital AMOUNT   Starting capital (default: 100000)\n";
    std::cout << "  --buy THRESHOLD    Buy threshold (default: 0.6)\n";
    std::cout << "  --sell THRESHOLD   Sell threshold (default: 0.4)\n";
    std::cout << "  --blocks N         Number of blocks to process (default: 20)\n";
    std::cout << "  --leverage-enabled Enable leverage trading (default)\n";
    std::cout << "  --no-leverage      Disable leverage, trade QQQ only\n";
    std::cout << "  --adaptive         Enable adaptive threshold learning\n";
    std::cout << "  --adaptive-algorithm ALGO  Learning algorithm: q-learning, bandit, ensemble\n";
    std::cout << "  --scalper          Enable momentum scalper mode\n";
    std::cout << "  --help, -h         Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sentio_cli trade\n";
    std::cout << "  sentio_cli trade --no-leverage --blocks 50\n";
    std::cout << "  sentio_cli trade --adaptive --adaptive-algorithm bandit\n";
    std::cout << "  sentio_cli trade --scalper --capital 50000\n";
}

TradeCommand::TradeConfig TradeCommand::parse_config(const std::vector<std::string>& args) {
    TradeConfig config;
    
    // Parse basic parameters
    config.signals_file = get_arg(args, "--signals", "latest");
    
    std::string capital_str = get_arg(args, "--capital", "100000");
    config.capital = std::stod(capital_str);
    
    std::string buy_str = get_arg(args, "--buy", "0.6");
    config.buy_threshold = std::stod(buy_str);
    
    std::string sell_str = get_arg(args, "--sell", "0.4");
    config.sell_threshold = std::stod(sell_str);
    
    std::string blocks_str = get_arg(args, "--blocks", "20");
    config.blocks = static_cast<size_t>(std::stoul(blocks_str));
    
    // Parse leverage options
    config.leverage_enabled = !has_flag(args, "--no-leverage");
    
    // Parse adaptive options
    config.adaptive_enabled = has_flag(args, "--adaptive");
    config.learning_algorithm = get_arg(args, "--adaptive-algorithm", "q-learning");
    
    // Parse scalper option
    config.scalper_enabled = has_flag(args, "--scalper") || has_flag(args, "--momentum-scalper");
    
    return config;
}

bool TradeCommand::validate_config(const TradeConfig& config) {
    // Validate capital
    if (config.capital <= 0) {
        std::cerr << "Error: Capital must be positive" << std::endl;
        return false;
    }
    
    // Validate thresholds
    if (config.buy_threshold <= config.sell_threshold) {
        std::cerr << "Error: Buy threshold must be greater than sell threshold" << std::endl;
        return false;
    }
    
    if (config.buy_threshold < 0.5 || config.buy_threshold > 1.0) {
        std::cerr << "Error: Buy threshold must be between 0.5 and 1.0" << std::endl;
        return false;
    }
    
    if (config.sell_threshold < 0.0 || config.sell_threshold > 0.5) {
        std::cerr << "Error: Sell threshold must be between 0.0 and 0.5" << std::endl;
        return false;
    }
    
    // Validate learning algorithm
    if (config.adaptive_enabled) {
        if (config.learning_algorithm != "q-learning" && 
            config.learning_algorithm != "bandit" && 
            config.learning_algorithm != "ensemble") {
            std::cerr << "Error: Invalid learning algorithm. Use: q-learning, bandit, ensemble" << std::endl;
            return false;
        }
    }
    
    return true;
}

std::string TradeCommand::resolve_signals_file(const std::string& signals_input) {
    if (signals_input == "latest" || signals_input.empty()) {
        // Find latest file in data/signals
        std::string signals_dir = "data/signals";
        if (!std::filesystem::exists(signals_dir)) {
            return "";
        }
        
        std::string latest_file;
        std::filesystem::file_time_type latest_time;
        
        for (const auto& entry : std::filesystem::directory_iterator(signals_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".jsonl") {
                auto file_time = entry.last_write_time();
                if (latest_file.empty() || file_time > latest_time) {
                    latest_file = entry.path().string();
                    latest_time = file_time;
                }
            }
        }
        
        return latest_file;
    } else {
        // Use provided file path
        if (std::filesystem::exists(signals_input)) {
            return signals_input;
        } else {
            std::cerr << "Error: Signals file not found: " << signals_input << std::endl;
            return "";
        }
    }
}

std::string TradeCommand::extract_market_data_path(const std::string& signals_file) {
    std::ifstream in(signals_file);
    if (!in.is_open()) {
        return "";
    }
    
    std::string line;
    if (std::getline(in, line)) {
        try {
            auto signal = sentio::SignalOutput::from_json(line);
            auto it = signal.metadata.find("market_data_path");
            if (it != signal.metadata.end()) {
                return it->second;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing signal: " << e.what() << std::endl;
        }
    }
    
    return "";
}

int TradeCommand::execute_trading(const TradeConfig& config, const std::string& market_path) {
    try {
        // Ensure trades directory exists
        std::error_code ec;
        std::filesystem::create_directories("data/trades", ec);
        
        // Configure backend
        sentio::BackendComponent::BackendConfig bc;
        bc.starting_capital = config.capital;
        bc.strategy_thresholds["buy_threshold"] = config.buy_threshold;
        bc.strategy_thresholds["sell_threshold"] = config.sell_threshold;
        bc.cost_model = sentio::CostModel::ALPACA;
        bc.leverage_enabled = config.leverage_enabled;
        
        // Configure trading mode
        if (config.leverage_enabled) {
            std::cout << "🚀 Leverage trading ENABLED" << std::endl;
            std::cout << "   Using automatic instrument selection: QQQ, TQQQ, SQQQ, PSQ" << std::endl;
        } else {
            std::cout << "📈 Standard trading mode" << std::endl;
            std::cout << "   Trading QQQ only (no leverage instruments)" << std::endl;
        }
        
        // Configure adaptive thresholds
        bc.enable_adaptive_thresholds = config.adaptive_enabled && !config.scalper_enabled;
        if (config.adaptive_enabled && !config.scalper_enabled) {
            std::cout << "🤖 ADAPTIVE THRESHOLDS ENABLED" << std::endl;
            std::cout << "   Algorithm: " << config.learning_algorithm << std::endl;
        }
        
        // Configure momentum scalper
        bc.enable_momentum_scalping = config.scalper_enabled;
        if (config.scalper_enabled) {
            std::cout << "🚀 MOMENTUM SCALPER ENABLED" << std::endl;
            std::cout << "   Target: 100+ daily trades, ~10% monthly returns" << std::endl;
        }
        
        // Initialize backend
        sentio::BackendComponent backend(bc);
        std::string run_id = sentio::utils::generate_run_id("trade");
        
        // Calculate processing window using standard block size
        const size_t BLOCK_SIZE = sentio::STANDARD_BLOCK_SIZE;
        std::ifstream signal_counter(config.signals_file);
        if (!signal_counter.is_open()) {
            std::cerr << "ERROR: Cannot open signal file: " << config.signals_file << std::endl;
            return 1;
        }
        
        size_t total_lines = 0;
        std::string tmp;
        while (std::getline(signal_counter, tmp)) ++total_lines;
        signal_counter.close();
        
        size_t max_count = (config.blocks == 0) ? static_cast<size_t>(-1) : config.blocks * BLOCK_SIZE;
        size_t start_index = 0;
        if (config.blocks > 0 && total_lines > max_count) {
            start_index = total_lines - max_count;
        }
        
        size_t signals_to_process = (max_count == static_cast<size_t>(-1)) ? total_lines : std::min(max_count, total_lines - start_index);
        size_t end_index = start_index + signals_to_process;
        
        std::cout << "Trading " << signals_to_process << " signals from index " << start_index << " to " << (end_index - 1) << std::endl;
        
        // Execute trading
        std::string trade_book = "data/trades/" + run_id + "_trades.jsonl";
        backend.process_to_jsonl(config.signals_file, market_path, trade_book, run_id, start_index, end_index);
        
        std::cout << "✅ Trading completed successfully" << std::endl;
        std::cout << "📄 Trade book: " << trade_book << std::endl;
        std::cout << "🆔 Run ID: " << run_id << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Trading execution failed: " << e.what() << std::endl;
        return 1;
    }
}

} // namespace cli
} // namespace sentio
