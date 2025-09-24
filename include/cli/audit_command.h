#pragma once

#include "cli/command_interface.h"

namespace sentio {
namespace cli {

/**
 * @brief Command for analyzing trading performance with professional reports
 * 
 * The audit command provides comprehensive analysis of trading performance
 * with professional-grade reports including metrics, position history,
 * and trade analysis.
 * 
 * Key Features:
 * - Professional brokerage-style reports
 * - Multiple report types (summary, position history, trade list)
 * - Performance metrics and analysis
 * - Dataset and test period information
 * - Instrument distribution analysis
 */
class AuditCommand : public Command {
public:
    int execute(const std::vector<std::string>& args) override;
    std::string get_name() const override { return "audit"; }
    std::string get_description() const override { 
        return "Analyze performance with professional reports"; 
    }
    void show_help() const override;

private:
    /**
     * @brief Execute summarize subcommand
     */
    int execute_summarize(const std::string& run_id);
    
    /**
     * @brief Execute report (position-history) subcommand
     */
    int execute_report(const std::string& run_id, size_t max_trades);
    
    /**
     * @brief Execute comprehensive report (restored from previous version)
     */
    int execute_report_comprehensive(const std::string& run_id, size_t max_trades);
    
    /**
     * @brief Execute trade-list subcommand
     */
    int execute_trade_list(const std::string& run_id);
    
    /**
     * @brief Execute signal analysis subcommand
     */
    int execute_signal_analysis(const std::vector<std::string>& args);
    
    /**
     * @brief Resolve latest run ID if not provided
     */
    std::string resolve_run_id(const std::string& run_input);
    
    /**
     * @brief Validate that trade book exists for run ID
     */
    bool validate_run_id(const std::string& run_id);
};

} // namespace cli
} // namespace sentio
