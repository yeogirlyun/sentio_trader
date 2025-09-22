#pragma once

#include "cli/command_interface.h"

namespace sentio {
namespace cli {

/**
 * @brief Command for executing trades based on AI-generated signals
 * 
 * The trade command executes trades using aggressive capital allocation
 * strategy designed for maximum profit potential. It represents the second
 * phase of the trading pipeline.
 * 
 * Key Features:
 * - Aggressive capital allocation (up to 100% usage)
 * - Multiple trading algorithms (static, adaptive, momentum scalper)
 * - Leverage support (QQQ, TQQQ, SQQQ, PSQ)
 * - Comprehensive trade logging and audit trail
 * - Block-based processing for controlled backtesting
 */
class TradeCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string get_name() const override { return "trade"; }
    std::string get_description() const override { 
        return "Execute trades with portfolio management"; 
    }
    void show_help() const override;

private:
    struct TradeConfig {
        std::string signals_file;
        double capital = 100000.0;
        double buy_threshold = 0.6;
        double sell_threshold = 0.4;
        size_t blocks = 20;
        bool leverage_enabled = true;
        bool adaptive_enabled = false;
        bool scalper_enabled = false;
        std::string learning_algorithm = "q-learning";
    };
    
    /**
     * @brief Parse command line arguments into configuration
     */
    TradeConfig parse_config(const std::vector<std::string>& args);
    
    /**
     * @brief Validate trading configuration
     */
    bool validate_config(const TradeConfig& config);
    
    /**
     * @brief Resolve signals file path (handle "latest" keyword)
     */
    std::string resolve_signals_file(const std::string& signals_input);
    
    /**
     * @brief Extract market data path from signals metadata
     */
    std::string extract_market_data_path(const std::string& signals_file);
    
    /**
     * @brief Execute the actual trading logic
     */
    int execute_trading(const TradeConfig& config, 
                       const std::string& market_path);
};

} // namespace cli
} // namespace sentio
