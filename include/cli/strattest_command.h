#pragma once

#include "cli/command_interface.h"

namespace sentio {
namespace cli {

/**
 * @brief Command for generating trading signals from market data
 * 
 * The strattest command processes market data through AI trading strategies
 * to generate buy/sell signals. It serves as the first phase of the trading
 * pipeline, analyzing historical price patterns to identify opportunities.
 * 
 * Key Features:
 * - Multiple AI strategies (Sigor, GRU, Momentum Scalper)
 * - Configurable parameters and output formats
 * - Automatic file organization in data/signals/
 * - JSONL format for efficient signal storage
 */
class StrattestCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string get_name() const override { return "strattest"; }
    std::string get_description() const override { 
        return "Generate trading signals from market data"; 
    }
    void show_help() const override;

private:
    /**
     * @brief Execute Sigor strategy
     */
    int execute_sigor_strategy(const std::string& dataset, 
                              const std::string& output,
                              const std::string& config_path,
                              const std::vector<std::string>& args);
    
    /**
     * @brief Execute GRU strategy
     */
    int execute_gru_strategy(const std::string& dataset,
                            const std::string& output,
                            const std::vector<std::string>& args);
    
    /**
     * @brief Execute Momentum Scalper strategy
     */
    int execute_momentum_strategy(const std::string& dataset,
                                 const std::string& output,
                                 const std::vector<std::string>& args);
    
    /**
     * @brief Generate default output filename
     */
    std::string generate_output_filename(const std::string& strategy) const;
    
    /**
     * @brief Validate input parameters
     */
    bool validate_parameters(const std::string& dataset,
                           const std::string& strategy) const;
};

} // namespace cli
} // namespace sentio
