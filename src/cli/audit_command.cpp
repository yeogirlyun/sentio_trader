#include "cli/audit_command.h"
#include "backend/audit_component.h"
#include "common/utils.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace sentio {
namespace cli {

// ANSI color codes for professional terminal output formatting
static constexpr const char* C_RESET = "\033[0m";   // Reset to default color
static constexpr const char* C_BOLD  = "\033[1m";   // Bold text for headers and emphasis
static constexpr const char* C_DIM   = "\033[2m";   // Dimmed text for secondary information
static constexpr const char* C_CYAN  = "\033[36m";  // Cyan for run IDs and identifiers
static constexpr const char* C_GREEN = "\033[32m";  // Green for success messages and buy actions
static constexpr const char* C_YELL  = "\033[33m";  // Yellow for warnings and neutral info
static constexpr const char* C_RED   = "\033[31m";  // Red for errors and sell actions

// Helper function to format Unix timestamp to readable date/time
std::string fmt_time(long long timestamp_ms) {
    time_t time_sec = timestamp_ms / 1000;
    struct tm* tm_info = gmtime(&time_sec);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M UTC", tm_info);
    return std::string(buffer);
}

int AuditCommand::execute(const std::vector<std::string>& args) {
    // Show help if requested
    if (has_flag(args, "--help") || has_flag(args, "-h")) {
        show_help();
        return 0;
    }
    
    // Parse subcommand (default to "summarize")
    std::string subcommand = "summarize";
    if (!args.empty() && args[0][0] != '-') {
        subcommand = args[0];
    }
    
    // Parse run ID (not needed for signal analysis)
    std::string run_id = get_arg(args, "--run", "");
    
    // Signal analysis doesn't need run ID
    if (subcommand != "signal") {
        run_id = resolve_run_id(run_id);
        
        if (run_id.empty()) {
            std::cerr << "ERROR: No trade books found in data/trades" << std::endl;
            return 3;
        }
    }
    
    // Validate run ID for non-signal commands
    if (subcommand != "signal" && !validate_run_id(run_id)) {
        std::cerr << "ERROR: Trade book not found for run: " << run_id << std::endl;
        return 1;
    }
    
    // Execute subcommand
    if (subcommand == "summarize") {
        return execute_summarize(run_id);
    } else if (subcommand == "report" || subcommand == "position-history") {
        std::string max_str = get_arg(args, "--max", "20");
        size_t max_trades = static_cast<size_t>(std::stoul(max_str));
        return execute_report_comprehensive(run_id, max_trades);
    } else if (subcommand == "trade-list") {
        return execute_trade_list(run_id);
    } else if (subcommand == "signal") {
        return execute_signal_analysis(args);
    } else {
        std::cerr << "Error: Unknown audit subcommand '" << subcommand << "'" << std::endl;
        show_help();
        return 1;
    }
}

void AuditCommand::show_help() const {
    std::cout << "Usage: sentio_cli audit [subcommand] [options]\n\n";
    std::cout << "Analyze trading performance with professional reports.\n\n";
    std::cout << "Subcommands:\n";
    std::cout << "  summarize          Show performance summary (default)\n";
    std::cout << "  report             Multi-section performance report with trade history\n";
    std::cout << "  trade-list         Complete list of all trades\n";
    std::cout << "  signal             Analyze signal accuracy and statistics\n\n";
    std::cout << "Options:\n";
    std::cout << "  --run RUN_ID       Specific run ID to analyze (default: latest)\n";
    std::cout << "  --max N            Maximum trades to show in report (default: 20)\n";
    std::cout << "  --help, -h         Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sentio_cli audit\n";
    std::cout << "  sentio_cli audit report --max 50\n";
    std::cout << "  sentio_cli audit trade-list --run trade_20250922_143022\n";
}

int AuditCommand::execute_summarize(const std::string& run_id) {
    try {
        std::string trade_book = "data/trades/" + run_id + "_trades.jsonl";
        
        // Extract dataset and period information from trade records
        std::string dataset_path;
        std::string dataset_period;
        std::string dataset_size_info;
        std::string testset_period;
        std::string blocks_info;
        double test_coverage_pct = 0.0;
        std::vector<double> equity;
        long long first_trade_ts = 0, last_trade_ts = 0;
        int first_bar_index = -1, last_bar_index = -1;
        int total_trades = 0;
        int buy_count = 0, sell_count = 0, hold_count = 0;
        double total_volume = 0.0;
        double total_realized_pnl = 0.0;
        
        // Parse trade data
        {
            std::ifstream in(trade_book);
            std::string line;
            bool first_record = true;
            
            while (std::getline(in, line)) {
                auto m = utils::from_json(line);
                total_trades++;
                
                if (m.count("equity_after")) equity.push_back(std::stod(m["equity_after"]));
                
                // Count trade types
                if (m.count("action")) {
                    std::string action = m["action"];
                    if (action == "BUY") buy_count++;
                    else if (action == "SELL") sell_count++;
                    else hold_count++;
                }
                
                // Accumulate volume and P&L
                if (m.count("trade_value")) total_volume += std::stod(m["trade_value"]);
                if (m.count("realized_pnl_delta")) total_realized_pnl += std::stod(m["realized_pnl_delta"]);
                
                // Extract timing and dataset info from first and last records
                if (first_record) {
                    if (m.count("timestamp_ms")) first_trade_ts = std::stoll(m["timestamp_ms"]);
                    if (m.count("bar_index")) first_bar_index = std::stoi(m["bar_index"]);
                    first_record = false;
                }
                if (m.count("timestamp_ms")) last_trade_ts = std::stoll(m["timestamp_ms"]);
                if (m.count("bar_index")) last_bar_index = std::stoi(m["bar_index"]);
            }
        }
        
        // Format test period
        if (first_trade_ts > 0 && last_trade_ts > 0) {
            testset_period = fmt_time(first_trade_ts) + " to " + fmt_time(last_trade_ts);
        }
        
        if (first_bar_index >= 0 && last_bar_index >= 0) {
            int bars_used = last_bar_index - first_bar_index + 1;
            int blocks_used = (bars_used + STANDARD_BLOCK_SIZE - 1) / STANDARD_BLOCK_SIZE; // Round up to nearest block
            blocks_info = std::to_string(blocks_used) + " blocks (" + std::to_string(bars_used) + " bars)";
        }
        
        // Calculate performance metrics
        AuditComponent auditor;
        auto summary = auditor.analyze_equity_curve(equity);
        
        // Professional Dataset & Test Information Table
        std::cout << "\n" << C_BOLD << "📊 DATASET & TEST SUMMARY" << C_RESET << "\n";
        std::cout << "┌─────────────────────┬─────────────────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ " << C_BOLD << "Run ID" << C_RESET << std::left << std::setw(13) << ""
                  << " │ " << C_CYAN << std::left << std::setw(75) << run_id << C_RESET << " │\n";
        std::cout << "├─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤\n";
        
        if (!testset_period.empty()) {
            std::cout << "│ " << std::left << std::setw(19) << "Test Period"
                      << " │ " << std::left << std::setw(75) << testset_period << " │\n";
        }
        if (!blocks_info.empty()) {
            std::cout << "│ " << std::left << std::setw(19) << "Test Size"
                      << " │ " << std::left << std::setw(75) << blocks_info << " │\n";
        }
        
        std::cout << "└─────────────────────┴─────────────────────────────────────────────────────────────────────────────┘\n";
        
        // Professional Performance Metrics Table
        std::cout << "\n" << C_BOLD << "📈 PERFORMANCE METRICS" << C_RESET << "\n";
        std::cout << "┌─────────────────────┬──────────────┬─────────────────────┬──────────────┬─────────────────────┬──────────────┐\n";
        
        // Calculate additional metrics
        double total_return = 0.0;
        double total_return_pct = 0.0;
        if (!equity.empty() && equity.front() > 0) {
            total_return = equity.back() - equity.front();
            total_return_pct = (total_return / equity.front()) * 100.0;
        }
        
        std::cout << "│ " << std::left << std::setw(15) << "Starting Capital:"
                  << " │ " << std::right << std::setw(8) << ("$" + std::to_string((int)(equity.empty() ? 0 : equity.front())))
                  << " │ " << std::left << std::setw(15) << "Current Equity:"
                  << " │ " << std::right << std::setw(8) << ("$" + std::to_string((int)(equity.empty() ? 0 : equity.back())))
                  << " │ " << std::left << std::setw(15) << "Total Return:"
                  << " │ " << (total_return >= 0 ? C_GREEN : C_RED) << std::right << std::setw(8)
                  << ("+" + std::to_string(total_return_pct).substr(0, 5) + "%") << C_RESET << " │\n";
        
        std::cout << "│ " << std::left << std::setw(15) << "Total Trades:"
                  << " │ " << std::right << std::setw(8) << total_trades
                  << " │ " << std::left << std::setw(15) << "Sharpe Ratio:"
                  << " │ " << std::right << std::setw(8) << std::to_string(summary.sharpe).substr(0, 6)
                  << " │ " << std::left << std::setw(15) << "Max Drawdown:"
                  << " │ " << C_RED << std::right << std::setw(8) << std::to_string(summary.max_drawdown).substr(0, 6) << C_RESET << " │\n";
        
        std::cout << "└─────────────────────────────────────────────────────────────────────────────────────────────────────────────┘\n";
        
        // Trade Activity Summary
        std::cout << "\n" << C_BOLD << "🎯 TRADING ACTIVITY SUMMARY" << C_RESET << "\n";
        std::cout << "┌─────────────────────────────────────────────────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ " << std::left << std::setw(15) << "BUY Orders:"
                  << " │ " << C_GREEN << std::right << std::setw(8) << buy_count << C_RESET
                  << " │ " << std::left << std::setw(15) << "SELL Orders:"
                  << " │ " << C_RED << std::right << std::setw(8) << sell_count << C_RESET
                  << " │ " << std::left << std::setw(15) << "Trade Volume:"
                  << " │ " << std::right << std::setw(8) << ("$" + std::to_string((int)total_volume))
                  << " │\n";
        std::cout << "│ " << std::left << std::setw(15) << "Realized P&L:"
                  << " │ " << (total_realized_pnl >= 0 ? C_GREEN : C_RED) << std::right << std::setw(8)
                  << ("$" + std::to_string((int)total_realized_pnl)) << C_RESET
                  << " │ " << std::left << std::setw(15) << "HOLD Orders:"
                  << " │ " << std::right << std::setw(8) << hold_count
                  << " │ " << std::left << std::setw(15) << "Active Trades:"
                  << " │ " << std::right << std::setw(8) << (buy_count + sell_count) << " │\n";
        std::cout << "└─────────────────────────────────────────────────────────────────────────────────────────────────────────────┘\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Summarize failed: " << e.what() << std::endl;
        return 1;
    }
}

int AuditCommand::execute_report(const std::string& run_id, size_t max_trades) {
    try {
        std::string trade_book = "data/trades/" + run_id + "_trades.jsonl";
        
        std::cout << "📈 Comprehensive Trading Report for Run: " << C_CYAN << run_id << C_RESET << std::endl;
        
        // Extract dataset and period information from trade records
        std::string dataset_path;
        std::string dataset_period;
        std::string dataset_size_info;
        std::string testset_period;
        std::string blocks_info;
        double test_coverage_pct = 0.0;
        std::vector<double> equity;
        long long first_trade_ts = 0, last_trade_ts = 0;
        int first_bar_index = -1, last_bar_index = -1;
        int total_trades = 0;
        int buy_count = 0, sell_count = 0, hold_count = 0;
        double total_volume = 0.0;
        double total_realized_pnl = 0.0;
        
        // Read and parse all trade records
        std::ifstream in(trade_book);
        std::string line;
        std::vector<std::string> all_lines;
        bool first_record = true;
        
        while (std::getline(in, line)) {
            all_lines.push_back(line);
            auto m = utils::from_json(line);
            total_trades++;
            
            if (m.count("equity_after")) equity.push_back(std::stod(m["equity_after"]));
            
            // Count trade types
            if (m.count("action")) {
                std::string action = m["action"];
                if (action == "BUY") buy_count++;
                else if (action == "SELL") sell_count++;
                else hold_count++;
            }
            
            // Accumulate volume and P&L
            if (m.count("trade_value")) total_volume += std::stod(m["trade_value"]);
            if (m.count("realized_pnl_delta")) total_realized_pnl += std::stod(m["realized_pnl_delta"]);
            
            // Extract timing and dataset info from first and last records
            if (first_record) {
                if (m.count("timestamp_ms")) first_trade_ts = std::stoll(m["timestamp_ms"]);
                if (m.count("bar_index")) first_bar_index = std::stoi(m["bar_index"]);
                first_record = false;
            }
            if (m.count("timestamp_ms")) last_trade_ts = std::stoll(m["timestamp_ms"]);
            if (m.count("bar_index")) last_bar_index = std::stoi(m["bar_index"]);
        }
        
        if (all_lines.empty()) {
            std::cout << "⚠️  No trades found in trade book" << std::endl;
            return 0;
        }
        
        // Extract dataset information from signal files (comprehensive analysis)
        std::string signal_file = "data/signals/sigor_signals_AUTO.jsonl";  // Try default first
        std::ifstream sig_in(signal_file);
        if (sig_in.is_open()) {
            std::string sig_line;
            if (std::getline(sig_in, sig_line)) {
                auto sig_m = utils::from_json(sig_line);
                if (sig_m.count("market_data_path")) {
                    dataset_path = sig_m["market_data_path"];
                    // Extract just the filename for cleaner display
                    size_t last_slash = dataset_path.find_last_of("/\\");
                    if (last_slash != std::string::npos) {
                        dataset_path = dataset_path.substr(last_slash + 1);
                    }
                }
            }
            sig_in.close();
        }
        
        // If no dataset path found, try to extract from latest signal file
        if (dataset_path.empty()) {
            std::string signals_dir = "data/signals";
            if (std::filesystem::exists(signals_dir)) {
                std::string latest_signal_file;
                std::filesystem::file_time_type latest_time;
                
                for (const auto& entry : std::filesystem::directory_iterator(signals_dir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".jsonl") {
                        auto file_time = entry.last_write_time();
                        if (latest_signal_file.empty() || file_time > latest_time) {
                            latest_signal_file = entry.path().string();
                            latest_time = file_time;
                        }
                    }
                }
                
                if (!latest_signal_file.empty()) {
                    std::ifstream latest_sig(latest_signal_file);
                    std::string sig_line;
                    if (std::getline(latest_sig, sig_line)) {
                        auto sig_m = utils::from_json(sig_line);
                        if (sig_m.count("market_data_path")) {
                            dataset_path = sig_m["market_data_path"];
                            size_t last_slash = dataset_path.find_last_of("/\\");
                            if (last_slash != std::string::npos) {
                                dataset_path = dataset_path.substr(last_slash + 1);
                            }
                        }
                    }
                }
            }
        }
        
        // Calculate test coverage if we have dataset info
        if (!dataset_path.empty() && first_bar_index >= 0 && last_bar_index >= 0) {
            // Estimate total dataset size (this is approximate)
            int bars_used = last_bar_index - first_bar_index + 1;
            int estimated_total_bars = last_bar_index + 1; // Rough estimate
            if (estimated_total_bars > 0) {
                test_coverage_pct = (static_cast<double>(bars_used) / estimated_total_bars) * 100.0;
            }
        }
        
        // Format test period
        if (first_trade_ts > 0 && last_trade_ts > 0) {
            testset_period = fmt_time(first_trade_ts) + " to " + fmt_time(last_trade_ts);
        }
        
        if (first_bar_index >= 0 && last_bar_index >= 0) {
            int bars_used = last_bar_index - first_bar_index + 1;
            int blocks_used = (bars_used + STANDARD_BLOCK_SIZE - 1) / STANDARD_BLOCK_SIZE; // Round up to nearest block
            blocks_info = std::to_string(blocks_used) + " blocks (" + std::to_string(bars_used) + " bars)";
        }
        
        // Calculate performance metrics
        AuditComponent auditor;
        auto summary = auditor.analyze_equity_curve(equity);
        
        // Professional Dataset & Test Information Table
        std::cout << "\n" << C_BOLD << "📊 DATASET & TEST SUMMARY" << C_RESET << "\n";
        std::cout << "┌─────────────────────┬─────────────────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ " << C_BOLD << "Run ID" << C_RESET << std::left << std::setw(13) << ""
                  << " │ " << C_CYAN << std::left << std::setw(75) << run_id << C_RESET << " │\n";
        std::cout << "├─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤\n";
        
        if (!testset_period.empty()) {
            std::cout << "│ " << std::left << std::setw(19) << "Test Period"
                      << " │ " << std::left << std::setw(75) << testset_period << " │\n";
        }
        if (!blocks_info.empty()) {
            std::cout << "│ " << std::left << std::setw(19) << "Test Size"
                      << " │ " << std::left << std::setw(75) << blocks_info << " │\n";
        }
        
        std::cout << "└─────────────────────┴─────────────────────────────────────────────────────────────────────────────┘\n";
        
        // Professional Performance Metrics Table
        std::cout << "\n" << C_BOLD << "📈 PERFORMANCE METRICS" << C_RESET << "\n";
        std::cout << "┌─────────────────────┬──────────────┬─────────────────────┬──────────────┬─────────────────────┬──────────────┐\n";
        
        // Calculate additional metrics
        double total_return = 0.0;
        double total_return_pct = 0.0;
        if (!equity.empty() && equity.front() > 0) {
            total_return = equity.back() - equity.front();
            total_return_pct = (total_return / equity.front()) * 100.0;
        }
        
        std::cout << "│ " << std::left << std::setw(15) << "Starting Capital:"
                  << " │ " << std::right << std::setw(8) << ("$" + std::to_string((int)(equity.empty() ? 0 : equity.front())))
                  << " │ " << std::left << std::setw(15) << "Current Equity:"
                  << " │ " << std::right << std::setw(8) << ("$" + std::to_string((int)(equity.empty() ? 0 : equity.back())))
                  << " │ " << std::left << std::setw(15) << "Total Return:"
                  << " │ " << (total_return >= 0 ? C_GREEN : C_RED) << std::right << std::setw(8)
                  << ("+" + std::to_string(total_return_pct).substr(0, 5) + "%") << C_RESET << " │\n";
        
        std::cout << "│ " << std::left << std::setw(15) << "Total Trades:"
                  << " │ " << std::right << std::setw(8) << total_trades
                  << " │ " << std::left << std::setw(15) << "Sharpe Ratio:"
                  << " │ " << std::right << std::setw(8) << std::to_string(summary.sharpe).substr(0, 6)
                  << " │ " << std::left << std::setw(15) << "Max Drawdown:"
                  << " │ " << C_RED << std::right << std::setw(8) << std::to_string(summary.max_drawdown).substr(0, 6) << C_RESET << " │\n";
        
        std::cout << "└─────────────────────────────────────────────────────────────────────────────────────────────────────────────┘\n";
        
        // Trade Activity Summary
        std::cout << "\n" << C_BOLD << "🎯 TRADING ACTIVITY SUMMARY" << C_RESET << "\n";
        std::cout << "┌─────────────────────────────────────────────────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ " << std::left << std::setw(15) << "BUY Orders:"
                  << " │ " << C_GREEN << std::right << std::setw(8) << buy_count << C_RESET
                  << " │ " << std::left << std::setw(15) << "SELL Orders:"
                  << " │ " << C_RED << std::right << std::setw(8) << sell_count << C_RESET
                  << " │ " << std::left << std::setw(15) << "Trade Volume:"
                  << " │ " << std::right << std::setw(8) << ("$" + std::to_string((int)total_volume))
                  << " │\n";
        std::cout << "│ " << std::left << std::setw(15) << "Realized P&L:"
                  << " │ " << (total_realized_pnl >= 0 ? C_GREEN : C_RED) << std::right << std::setw(8)
                  << ("$" + std::to_string((int)total_realized_pnl)) << C_RESET
                  << " │ " << std::left << std::setw(15) << "HOLD Orders:"
                  << " │ " << std::right << std::setw(8) << hold_count
                  << " │ " << std::left << std::setw(15) << "Active Trades:"
                  << " │ " << std::right << std::setw(8) << (buy_count + sell_count) << " │\n";
        std::cout << "└─────────────────────────────────────────────────────────────────────────────────────────────────────────────┘\n";
        
        // Show header
        std::cout << "\n" << C_BOLD << "📋 TRADE HISTORY" << C_RESET << " (showing last " << max_trades << " trades)\n";
        std::cout << "┌──────────┬─────────┬─────────┬─────────────┬─────────────┬─────────────┬─────────────────────────────┐\n";
        std::cout << "│   Index  │  Action │  Price  │    Qty      │    Value    │   P&L       │         Reason              │\n";
        std::cout << "├──────────┼─────────┼─────────┼─────────────┼─────────────┼─────────────┼─────────────────────────────┤\n";
        
        // Show last N trades
        size_t start_idx = all_lines.size() > max_trades ? all_lines.size() - max_trades : 0;
        for (size_t i = start_idx; i < all_lines.size(); ++i) {
            auto m = utils::from_json(all_lines[i]);
            
            std::string bar_idx = m.count("bar_index") ? m["bar_index"] : "N/A";
            std::string action = m.count("action") ? m["action"] : "N/A";
            std::string price = m.count("price") ? ("$" + std::to_string(std::stod(m["price"])).substr(0, 7)) : "N/A";
            std::string qty = m.count("quantity") ? std::to_string(std::stod(m["quantity"])).substr(0, 8) : "0";
            std::string value = m.count("trade_value") ? ("$" + std::to_string(std::stod(m["trade_value"])).substr(0, 8)) : "$0";
            std::string pnl = m.count("realized_pnl_delta") ? ("$" + std::to_string(std::stod(m["realized_pnl_delta"])).substr(0, 8)) : "$0";
            std::string reason = m.count("execution_reason") ? m["execution_reason"] : "N/A";
            
            // Truncate reason if too long
            if (reason.length() > 27) {
                reason = reason.substr(0, 24) + "...";
            }
            
            // Color code actions
            std::string action_colored = action;
            if (action == "BUY") action_colored = C_GREEN + action + C_RESET;
            else if (action == "SELL") action_colored = C_RED + action + C_RESET;
            else if (action == "HOLD") action_colored = C_YELL + action + C_RESET;
            
            std::cout << "│ " << std::right << std::setw(8) << bar_idx
                      << " │ " << std::left << std::setw(7) << action_colored
                      << " │ " << std::right << std::setw(7) << price
                      << " │ " << std::right << std::setw(11) << qty
                      << " │ " << std::right << std::setw(11) << value
                      << " │ " << std::right << std::setw(11) << pnl
                      << " │ " << std::left << std::setw(27) << reason << " │\n";
        }
        
        std::cout << "└──────────┴─────────┴─────────┴─────────────┴─────────────┴─────────────┴─────────────────────────────┘\n";
        std::cout << "\n📄 Trade book: " << trade_book << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Report generation failed: " << e.what() << std::endl;
        return 1;
    }
}

int AuditCommand::execute_trade_list(const std::string& run_id) {
    try {
        std::string trade_book = "data/trades/" + run_id + "_trades.jsonl";
        
        std::cout << "📋 Complete Trade List for Run: " << C_CYAN << run_id << C_RESET << std::endl;
        
        // Read and display all trade records
        std::ifstream in(trade_book);
        std::string line;
        int line_count = 0;
        
        while (std::getline(in, line)) {
            line_count++;
            std::cout << "[" << line_count << "] " << line << std::endl;
        }
        
        if (line_count == 0) {
            std::cout << "⚠️  No trades found in trade book" << std::endl;
        } else {
            std::cout << "\n📊 Total records: " << line_count << std::endl;
        }
        
        std::cout << "📄 Trade book: " << trade_book << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Trade list generation failed: " << e.what() << std::endl;
        return 1;
    }
}

std::string AuditCommand::resolve_run_id(const std::string& run_input) {
    if (!run_input.empty() && run_input != "latest") {
        return run_input;
    }
    
    // Find latest trade file
    std::string trades_dir = "data/trades";
    if (!std::filesystem::exists(trades_dir)) {
        return "";
    }
    
    std::string latest_file;
    std::filesystem::file_time_type latest_time;
    
    for (const auto& entry : std::filesystem::directory_iterator(trades_dir)) {
        if (entry.is_regular_file() && 
            entry.path().filename().string().find("_trades.jsonl") != std::string::npos) {
            auto file_time = entry.last_write_time();
            if (latest_file.empty() || file_time > latest_time) {
                latest_file = entry.path().filename().string();
                latest_time = file_time;
            }
        }
    }
    
    if (!latest_file.empty()) {
        // Extract run ID from filename (format: <runid>_trades.jsonl)
        size_t pos = latest_file.find("_trades.jsonl");
        if (pos != std::string::npos) {
            return latest_file.substr(0, pos);
        }
    }
    
    return "";
}

bool AuditCommand::validate_run_id(const std::string& run_id) {
    std::string trade_book = "data/trades/" + run_id + "_trades.jsonl";
    return std::filesystem::exists(trade_book);
}

int AuditCommand::execute_signal_analysis(const std::vector<std::string>& args) {
    std::cout << C_BOLD << "📊 SIGNAL ANALYSIS REPORT" << C_RESET << std::endl;
    std::cout << "=========================" << std::endl << std::endl;
    
    // Parse signal file argument
    std::string signal_file = get_arg(args, "--file");
    if (signal_file.empty()) {
        // Find the latest signal file in data/signals/
        std::string signals_dir = "data/signals";
        if (!std::filesystem::exists(signals_dir)) {
            std::cerr << "Error: No signals directory found at " << signals_dir << std::endl;
            return 1;
        }
        
        // Find the most recent signal file
        std::filesystem::file_time_type latest_time{};
        for (const auto& entry : std::filesystem::directory_iterator(signals_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".jsonl") {
                auto file_time = entry.last_write_time();
                if (file_time > latest_time) {
                    latest_time = file_time;
                    signal_file = entry.path().string();
                }
            }
        }
        
        if (signal_file.empty()) {
            std::cerr << "Error: No signal files found in " << signals_dir << std::endl;
            return 1;
        }
    }
    
    std::cout << "📁 Analyzing signal file: " << C_CYAN << signal_file << C_RESET << std::endl << std::endl;
    
    // Load signal data and corresponding market data for accuracy analysis
    std::ifstream signal_stream(signal_file);
    if (!signal_stream.is_open()) {
        std::cerr << "Error: Cannot open signal file: " << signal_file << std::endl;
        return 1;
    }
    
    // Parse signals and calculate statistics
    std::vector<double> probabilities;
    std::vector<double> confidences;
    std::vector<long long> timestamps;
    std::vector<int> bar_indices;
    std::string strategy_name;
    
    std::string line;
    while (std::getline(signal_stream, line)) {
        if (line.empty()) continue;
        
        // Simple JSON parsing for signal data
        size_t prob_pos = line.find("\"probability\":");
        size_t conf_pos = line.find("\"confidence\":");
        size_t ts_pos = line.find("\"timestamp_ms\":");
        size_t bar_pos = line.find("\"bar_index\":");
        size_t strat_pos = line.find("\"strategy_name\":\"");
        
        if (prob_pos != std::string::npos && conf_pos != std::string::npos) {
            // Extract probability (handle quoted strings)
            size_t prob_start = prob_pos + 14;  // Length of "probability":
            if (line[prob_start] == '"') prob_start++;  // Skip opening quote
            size_t prob_end = line.find(',', prob_start);
            if (prob_end == std::string::npos) prob_end = line.find('}', prob_start);
            if (line[prob_end - 1] == '"') prob_end--;  // Skip closing quote
            std::string prob_str = line.substr(prob_start, prob_end - prob_start);
            if (prob_str.back() == '"') prob_str.pop_back();  // Remove trailing quote if exists
            double probability = std::stod(prob_str);
            probabilities.push_back(probability);
            
            // Extract confidence (handle quoted strings)
            size_t conf_start = conf_pos + 13;  // Length of "confidence":
            if (line[conf_start] == '"') conf_start++;  // Skip opening quote
            size_t conf_end = line.find(',', conf_start);
            if (conf_end == std::string::npos) conf_end = line.find('}', conf_start);
            if (line[conf_end - 1] == '"') conf_end--;  // Skip closing quote
            std::string conf_str = line.substr(conf_start, conf_end - conf_start);
            if (conf_str.back() == '"') conf_str.pop_back();  // Remove trailing quote if exists
            double confidence = std::stod(conf_str);
            confidences.push_back(confidence);
            
            // Extract timestamp (handle quoted strings)
            if (ts_pos != std::string::npos) {
                size_t ts_start = ts_pos + 15;  // Length of "timestamp_ms":
                if (line[ts_start] == '"') ts_start++;  // Skip opening quote
                size_t ts_end = line.find(',', ts_start);
                if (ts_end == std::string::npos) ts_end = line.find('}', ts_start);
                if (line[ts_end - 1] == '"') ts_end--;  // Skip closing quote
                std::string ts_str = line.substr(ts_start, ts_end - ts_start);
                if (ts_str.back() == '"') ts_str.pop_back();  // Remove trailing quote if exists
                long long timestamp = std::stoll(ts_str);
                timestamps.push_back(timestamp);
            }
            
            // Extract bar index (handle quoted strings)
            if (bar_pos != std::string::npos) {
                size_t bar_start = bar_pos + 12;  // Length of "bar_index":
                if (line[bar_start] == '"') bar_start++;  // Skip opening quote
                size_t bar_end = line.find(',', bar_start);
                if (bar_end == std::string::npos) bar_end = line.find('}', bar_start);
                if (line[bar_end - 1] == '"') bar_end--;  // Skip closing quote
                std::string bar_str = line.substr(bar_start, bar_end - bar_start);
                if (bar_str.back() == '"') bar_str.pop_back();  // Remove trailing quote if exists
                int bar_index = std::stoi(bar_str);
                bar_indices.push_back(bar_index);
            }
            
            // Extract strategy name (first occurrence)
            if (strat_pos != std::string::npos && strategy_name.empty()) {
                size_t strat_start = strat_pos + 17;  // Length of "strategy_name":"
                size_t strat_end = line.find('"', strat_start);
                strategy_name = line.substr(strat_start, strat_end - strat_start);
            }
        }
    }
    signal_stream.close();
    
    if (probabilities.empty()) {
        std::cerr << "Error: No valid signals found in file" << std::endl;
        return 1;
    }
    
    // Calculate basic statistics
    double mean_prob = 0.0, mean_conf = 0.0;
    double min_prob = probabilities[0], max_prob = probabilities[0];
    double min_conf = confidences[0], max_conf = confidences[0];
    
    for (size_t i = 0; i < probabilities.size(); ++i) {
        mean_prob += probabilities[i];
        mean_conf += confidences[i];
        
        min_prob = std::min(min_prob, probabilities[i]);
        max_prob = std::max(max_prob, probabilities[i]);
        min_conf = std::min(min_conf, confidences[i]);
        max_conf = std::max(max_conf, confidences[i]);
    }
    
    mean_prob /= probabilities.size();
    mean_conf /= confidences.size();
    
    // Calculate standard deviation
    double std_prob = 0.0, std_conf = 0.0;
    for (size_t i = 0; i < probabilities.size(); ++i) {
        std_prob += (probabilities[i] - mean_prob) * (probabilities[i] - mean_prob);
        std_conf += (confidences[i] - mean_conf) * (confidences[i] - mean_conf);
    }
    std_prob = std::sqrt(std_prob / probabilities.size());
    std_conf = std::sqrt(std_conf / confidences.size());
    
    // Calculate median
    std::vector<double> sorted_prob = probabilities;
    std::vector<double> sorted_conf = confidences;
    std::sort(sorted_prob.begin(), sorted_prob.end());
    std::sort(sorted_conf.begin(), sorted_conf.end());
    
    double median_prob = sorted_prob[sorted_prob.size() / 2];
    double median_conf = sorted_conf[sorted_conf.size() / 2];
    
    // Signal distribution analysis
    int bullish_signals = 0, bearish_signals = 0, neutral_signals = 0;
    int high_conf_signals = 0, low_conf_signals = 0;
    
    for (size_t i = 0; i < probabilities.size(); ++i) {
        if (probabilities[i] > 0.55) bullish_signals++;
        else if (probabilities[i] < 0.45) bearish_signals++;
        else neutral_signals++;
        
        if (confidences[i] > 0.7) high_conf_signals++;
        else if (confidences[i] < 0.3) low_conf_signals++;
    }
    
    // Display comprehensive signal analysis with perfect alignment
    std::cout << "┌─────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│                    " << C_BOLD << "SIGNAL STATISTICS" << C_RESET << "                        │" << std::endl;
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    
    // Format strategy name with proper padding
    std::ostringstream strategy_line;
    strategy_line << "│ Strategy:           " << C_CYAN << std::setw(39) << std::left << strategy_name << C_RESET << " │";
    std::cout << strategy_line.str() << std::endl;
    
    // Format total signals with proper padding
    std::ostringstream signals_line;
    signals_line << "│ Total Signals:      " << C_YELL << std::setw(39) << std::right << probabilities.size() << C_RESET << " │";
    std::cout << signals_line.str() << std::endl;
    
    // Format time period with proper padding
    std::ostringstream time1_line;
    time1_line << "│ Time Period:        " << C_DIM << std::setw(39) << std::left << fmt_time(timestamps.front()) << C_RESET << " │";
    std::cout << time1_line.str() << std::endl;
    
    std::ostringstream time2_line;
    time2_line << "│                     " << C_DIM << std::setw(39) << std::left << ("to " + fmt_time(timestamps.back())) << C_RESET << " │";
    std::cout << time2_line.str() << std::endl;
    
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    std::cout << "│                   " << C_BOLD << "PROBABILITY ANALYSIS" << C_RESET << "                      │" << std::endl;
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    
    // Format probability analysis with perfect right alignment
    std::ostringstream prob_mean_line;
    prob_mean_line << "│ Mean:               " << std::setw(39) << std::right << std::fixed << std::setprecision(4) << mean_prob << " │";
    std::cout << prob_mean_line.str() << std::endl;
    
    std::ostringstream prob_std_line;
    prob_std_line << "│ Std Dev:            " << std::setw(39) << std::right << std::fixed << std::setprecision(4) << std_prob << " │";
    std::cout << prob_std_line.str() << std::endl;
    
    std::ostringstream prob_median_line;
    prob_median_line << "│ Median:             " << std::setw(39) << std::right << std::fixed << std::setprecision(4) << median_prob << " │";
    std::cout << prob_median_line.str() << std::endl;
    
    std::ostringstream prob_range_line;
    prob_range_line << "│ Range:              " << std::setw(30) << std::right << std::fixed << std::setprecision(4) 
                    << min_prob << " - " << std::setprecision(4) << max_prob << " │";
    std::cout << prob_range_line.str() << std::endl;
    
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    std::cout << "│                   " << C_BOLD << "CONFIDENCE ANALYSIS" << C_RESET << "                       │" << std::endl;
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    
    // Format confidence analysis with perfect right alignment
    std::ostringstream conf_mean_line;
    conf_mean_line << "│ Mean:               " << std::setw(39) << std::right << std::fixed << std::setprecision(4) << mean_conf << " │";
    std::cout << conf_mean_line.str() << std::endl;
    
    std::ostringstream conf_std_line;
    conf_std_line << "│ Std Dev:            " << std::setw(39) << std::right << std::fixed << std::setprecision(4) << std_conf << " │";
    std::cout << conf_std_line.str() << std::endl;
    
    std::ostringstream conf_median_line;
    conf_median_line << "│ Median:             " << std::setw(39) << std::right << std::fixed << std::setprecision(4) << median_conf << " │";
    std::cout << conf_median_line.str() << std::endl;
    
    std::ostringstream conf_range_line;
    conf_range_line << "│ Range:              " << std::setw(30) << std::right << std::fixed << std::setprecision(4) 
                    << min_conf << " - " << std::setprecision(4) << max_conf << " │";
    std::cout << conf_range_line.str() << std::endl;
    
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    std::cout << "│                  " << C_BOLD << "SIGNAL DISTRIBUTION" << C_RESET << "                        │" << std::endl;
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    
    // Format signal distribution with perfect right alignment
    std::ostringstream bull_line;
    bull_line << "│ " << C_GREEN << "Bullish (>0.55):" << C_RESET << "     " << std::setw(6) << std::right << bullish_signals 
              << " (" << std::setw(4) << std::right << std::fixed << std::setprecision(1) 
              << (100.0 * bullish_signals / probabilities.size()) << "%)                         │";
    std::cout << bull_line.str() << std::endl;
    
    std::ostringstream neutral_line;
    neutral_line << "│ " << C_YELL << "Neutral (0.45-0.55):" << C_RESET << " " << std::setw(6) << std::right << neutral_signals 
                 << " (" << std::setw(4) << std::right << std::fixed << std::setprecision(1) 
                 << (100.0 * neutral_signals / probabilities.size()) << "%)                         │";
    std::cout << neutral_line.str() << std::endl;
    
    std::ostringstream bear_line;
    bear_line << "│ " << C_RED << "Bearish (<0.45):" << C_RESET << "     " << std::setw(6) << std::right << bearish_signals 
              << " (" << std::setw(4) << std::right << std::fixed << std::setprecision(1) 
              << (100.0 * bearish_signals / probabilities.size()) << "%)                         │";
    std::cout << bear_line.str() << std::endl;
    
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    
    // Format confidence distribution with perfect right alignment
    std::ostringstream high_conf_line;
    high_conf_line << "│ " << C_BOLD << "High Confidence (>0.7):" << C_RESET << " " << std::setw(6) << std::right << high_conf_signals 
                   << " (" << std::setw(4) << std::right << std::fixed << std::setprecision(1) 
                   << (100.0 * high_conf_signals / probabilities.size()) << "%)                      │";
    std::cout << high_conf_line.str() << std::endl;
    
    std::ostringstream low_conf_line;
    low_conf_line << "│ " << C_DIM << "Low Confidence (<0.3):" << C_RESET << "  " << std::setw(6) << std::right << low_conf_signals 
                  << " (" << std::setw(4) << std::right << std::fixed << std::setprecision(1) 
                  << (100.0 * low_conf_signals / probabilities.size()) << "%)                      │";
    std::cout << low_conf_line.str() << std::endl;
    
    std::cout << "└─────────────────────────────────────────────────────────────┘" << std::endl;
    
    // Load market data for accuracy analysis
    std::cout << std::endl << C_BOLD << "📈 SIGNAL ACCURACY ANALYSIS" << C_RESET << std::endl;
    
    // Try to find market data file from signal metadata
    std::string market_data_file = "data/equities/QQQ_RTH_NH.csv";  // Default
    
    // Load market data
    std::vector<std::pair<long long, double>> market_prices;  // timestamp, close_price
    std::ifstream market_stream(market_data_file);
    if (!market_stream.is_open()) {
        std::cout << "⚠️  Market data not found at " << market_data_file << std::endl;
        std::cout << "   Accuracy analysis requires corresponding market data." << std::endl;
        return 0;
    }
    
    std::cout << "📊 Loading market data from: " << C_DIM << market_data_file << C_RESET << std::endl;
    
    std::string market_line;
    std::getline(market_stream, market_line); // Skip header
    
    while (std::getline(market_stream, market_line)) {
        if (market_line.empty()) continue;
        
        std::istringstream ss(market_line);
        std::string token;
        
        // Parse CSV: ts_utc,open,high,low,close,volume
        std::getline(ss, token, ',');
        
        // Convert ISO datetime to Unix timestamp
        // Format: 2022-09-15T09:30:00-04:00
        long long timestamp = 0;
        if (token.size() >= 19) {
            // Parse year, month, day, hour, minute, second
            int year = std::stoi(token.substr(0, 4));
            int month = std::stoi(token.substr(5, 2));
            int day = std::stoi(token.substr(8, 2));
            int hour = std::stoi(token.substr(11, 2));
            int minute = std::stoi(token.substr(14, 2));
            int second = std::stoi(token.substr(17, 2));
            
            // Simple conversion to Unix timestamp (approximate)
            // This is a simplified conversion - for production use proper datetime library
            struct tm tm_time = {};
            tm_time.tm_year = year - 1900;
            tm_time.tm_mon = month - 1;
            tm_time.tm_mday = day;
            tm_time.tm_hour = hour;
            tm_time.tm_min = minute;
            tm_time.tm_sec = second;
            
            // Convert to UTC timestamp and then to milliseconds
            timestamp = static_cast<long long>(timegm(&tm_time)) * 1000;
            
            // Adjust for timezone offset (EDT is UTC-4)
            if (token.find("-04:00") != std::string::npos) {
                timestamp += 4 * 3600 * 1000; // Add 4 hours in milliseconds
            }
        }
        
        // Skip open, high, low
        std::getline(ss, token, ','); // open
        std::getline(ss, token, ','); // high  
        std::getline(ss, token, ','); // low
        
        // Get close price
        std::getline(ss, token, ',');
        double close_price = std::stod(token);
        
        market_prices.emplace_back(timestamp, close_price);
    }
    market_stream.close();
    
    if (market_prices.empty()) {
        std::cout << "❌ No market data loaded" << std::endl;
        return 0;
    }
    
    std::cout << "✅ Loaded " << market_prices.size() << " market data points" << std::endl << std::endl;
    
    // Calculate accuracy metrics
    int simple_correct = 0, simple_total = 0;
    double advanced_score = 0.0, advanced_total = 0.0;
    int strong_bullish_correct = 0, strong_bullish_total = 0;
    int strong_bearish_correct = 0, strong_bearish_total = 0;
    
    // Match signals with market data
    for (size_t i = 0; i < timestamps.size() && i < probabilities.size(); ++i) {
        long long signal_timestamp = timestamps[i];
        double signal_prob = probabilities[i];
        double signal_conf = confidences[i];
        
        // Find corresponding market data point
        auto market_it = std::lower_bound(market_prices.begin(), market_prices.end(), 
                                        std::make_pair(signal_timestamp, 0.0));
        
        if (market_it != market_prices.end() && market_it + 1 != market_prices.end()) {
            double current_price = market_it->second;
            double next_price = (market_it + 1)->second;
            
            // Calculate actual price change
            double actual_return = (next_price - current_price) / current_price;
            bool actual_up = actual_return > 0.0;
            
            // Simple accuracy: directional correctness
            bool predicted_up = signal_prob > 0.5;
            if (predicted_up == actual_up) {
                simple_correct++;
            }
            simple_total++;
            
            // Advanced accuracy: strength-weighted performance
            // Higher confidence in correct direction should get higher score
            double prediction_strength = std::abs(signal_prob - 0.5) * 2.0; // 0-1 scale
            double actual_strength = std::abs(actual_return) * 100.0; // Convert to percentage
            
            // Score based on alignment of prediction confidence and actual move strength
            double accuracy_score = 0.0;
            if (predicted_up == actual_up) {
                // Correct direction: score based on confidence vs actual strength alignment
                accuracy_score = prediction_strength * signal_conf * (1.0 + actual_strength);
            } else {
                // Wrong direction: penalty proportional to confidence
                accuracy_score = -prediction_strength * signal_conf;
            }
            
            advanced_score += accuracy_score;
            advanced_total += prediction_strength * signal_conf;
            
            // Track strong signal performance
            if (signal_prob > 0.7 && signal_conf > 0.8) { // Strong bullish
                if (actual_up) strong_bullish_correct++;
                strong_bullish_total++;
            } else if (signal_prob < 0.3 && signal_conf > 0.8) { // Strong bearish
                if (!actual_up) strong_bearish_correct++;
                strong_bearish_total++;
            }
        }
    }
    
    // Display accuracy results with perfect alignment
    std::cout << "┌─────────────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│                   " << C_BOLD << "ACCURACY METRICS" << C_RESET << "                          │" << std::endl;
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    
    if (simple_total > 0) {
        double simple_accuracy = 100.0 * simple_correct / simple_total;
        
        // Format simple accuracy with perfect right alignment
        std::ostringstream simple_acc_line;
        simple_acc_line << "│ " << C_BOLD << "Simple Accuracy:" << C_RESET << "     " << std::setw(6) << std::right << simple_correct 
                        << "/" << std::setw(6) << std::left << simple_total 
                        << " (" << std::setw(4) << std::right << std::fixed << std::setprecision(1) << simple_accuracy << "%)                  │";
        std::cout << simple_acc_line.str() << std::endl;
        
        // Color-code accuracy
        std::string accuracy_color = C_RED;
        if (simple_accuracy > 55.0) accuracy_color = C_GREEN;
        else if (simple_accuracy > 50.0) accuracy_color = C_YELL;
        
        // Format direction prediction with perfect right alignment
        std::ostringstream direction_line;
        direction_line << "│ " << accuracy_color << "Direction Prediction:" << C_RESET << " " 
                       << std::setw(20) << std::right << (simple_accuracy > 50.0 ? "Above Random" : "Below Random") 
                       << " (" << std::setw(4) << std::right << std::fixed << std::setprecision(1) << (simple_accuracy - 50.0) 
                       << "% vs 50% )  │";
        std::cout << direction_line.str() << std::endl;
    }
    
    if (advanced_total > 0.0) {
        double advanced_accuracy = advanced_score / advanced_total;
        
        // Format advanced score with perfect right alignment
        std::ostringstream advanced_line;
        advanced_line << "│ " << C_BOLD << "Advanced Score:" << C_RESET << "      " << std::setw(37) << std::right 
                      << std::fixed << std::setprecision(4) << advanced_accuracy << "  │";
        std::cout << advanced_line.str() << std::endl;
        
        std::ostringstream advanced_desc_line;
        advanced_desc_line << "│ " << C_DIM << "(Strength-weighted performance)" << C_RESET << std::setw(26) << " " << "   │";
        std::cout << advanced_desc_line.str() << std::endl;
    }
    
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    std::cout << "│                " << C_BOLD << "HIGH-CONFIDENCE SIGNALS" << C_RESET << "                      │" << std::endl;
    std::cout << "├─────────────────────────────────────────────────────────────┤" << std::endl;
    
    if (strong_bullish_total > 0) {
        double strong_bull_acc = 100.0 * strong_bullish_correct / strong_bullish_total;
        
        // Format strong bullish with perfect right alignment
        std::ostringstream strong_bull_line;
        strong_bull_line << "│ " << C_GREEN << "Strong Bullish (>0.7):" << C_RESET << " " << std::setw(6) << std::right << strong_bullish_correct 
                         << "/" << std::setw(6) << std::left << strong_bullish_total 
                         << " (" << std::setw(4) << std::right << std::fixed << std::setprecision(1) << strong_bull_acc << "%)                │";
        std::cout << strong_bull_line.str() << std::endl;
    }
    
    if (strong_bearish_total > 0) {
        double strong_bear_acc = 100.0 * strong_bearish_correct / strong_bearish_total;
        
        // Format strong bearish with perfect right alignment
        std::ostringstream strong_bear_line;
        strong_bear_line << "│ " << C_RED << "Strong Bearish (<0.3):" << C_RESET << " " << std::setw(6) << std::right << strong_bearish_correct 
                         << "/" << std::setw(6) << std::left << strong_bearish_total 
                         << " (" << std::setw(4) << std::right << std::fixed << std::setprecision(1) << strong_bear_acc << "%)                │";
        std::cout << strong_bear_line.str() << std::endl;
    }
    
    std::cout << "└─────────────────────────────────────────────────────────────┘" << std::endl;
    
    // Additional insights
    std::cout << std::endl << C_BOLD << "📋 ACCURACY INSIGHTS" << C_RESET << std::endl;
    if (simple_total > 0) {
        double simple_accuracy = 100.0 * simple_correct / simple_total;
        if (simple_accuracy > 55.0) {
            std::cout << "✅ " << C_GREEN << "Strong predictive power" << C_RESET << " - Strategy shows significant edge over random" << std::endl;
        } else if (simple_accuracy > 52.0) {
            std::cout << "📈 " << C_YELL << "Moderate predictive power" << C_RESET << " - Strategy shows some edge over random" << std::endl;
        } else if (simple_accuracy > 48.0) {
            std::cout << "⚖️  " << C_DIM << "Near-random performance" << C_RESET << " - Strategy shows little directional edge" << std::endl;
        } else {
            std::cout << "❌ " << C_RED << "Poor predictive power" << C_RESET << " - Strategy may be anti-correlated" << std::endl;
        }
    }
    
    return 0;
}

// COMPREHENSIVE AUDIT REPORT - RESTORED FROM GIT COMMIT 118a670
// This is the complete implementation that matches the previous comprehensive format

int AuditCommand::execute_report_comprehensive(const std::string& run_id, size_t max_trades) {
    try {
        std::string trade_book = "data/trades/" + run_id + "_trades.jsonl";
        
        // Professional multi-section report (ASCII box layout)
        std::ifstream in(trade_book);
        if (!in.is_open()) { 
            std::cerr << "ERROR: Cannot open trade book: " << trade_book << "\n"; 
            return 1; 
        }

        int max_rows = static_cast<int>(max_trades);

        struct TradeEvent { 
            long long ts=0; 
            std::string sym; 
            std::string act; 
            double qty=0, price=0, tval=0, rpnl=0, eq=0, unpnl=0; 
            std::string pos_sum; 
            double cash_after=0, eq_before=0; 
        };
        std::vector<TradeEvent> trades;

        auto fmt_time = [](long long ts_ms) {
            std::time_t t = static_cast<std::time_t>(ts_ms / 1000);
            std::tm* g = std::gmtime(&t);  // Use UTC to match test period calculation
            char buf[20]; 
            std::strftime(buf, sizeof(buf), "%m/%d %H:%M:%S", g); 
            return std::string(buf);
        };
        
        auto money = [](double v, bool show_plus, int width){ 
            std::ostringstream os; 
            os << "$ " << (show_plus?std::showpos:std::noshowpos) << std::fixed << std::setprecision(2) << v << std::noshowpos; 
            std::ostringstream pad; 
            pad << std::right << std::setw(width) << os.str(); 
            return pad.str(); 
        };
        
        auto qty_str = [](double v, int width){ 
            std::ostringstream os; 
            os << std::fixed << std::showpos << std::setprecision(3) << v << std::noshowpos; 
            std::ostringstream pad; 
            pad << std::right << std::setw(width) << os.str(); 
            return pad.str(); 
        };

        auto looks_like_iso = [](const std::string& s){ 
            return s.find('T') != std::string::npos && s.find('-') != std::string::npos && s.find(':') != std::string::npos; 
        };
        
        auto parse_double = [](const std::string& v, double def)->double{ 
            try { return std::stod(v); } catch (...) { return def; } 
        };

        // Extract dataset and period information
        std::string dataset_path;
        std::string testset_period;
        std::string blocks_info;
        long long first_trade_ts = 0, last_trade_ts = 0;
        int first_bar_index = -1, last_bar_index = -1;
        
        // Get dataset info from corresponding signal file
        std::string signal_file = "data/signals/sgo-09-24-14-48.jsonl";  // Use latest signal file
        std::string signals_dir = "data/signals";
        if (std::filesystem::exists(signals_dir)) {
            std::string latest_signal_file;
            std::filesystem::file_time_type latest_time;
            
            for (const auto& entry : std::filesystem::directory_iterator(signals_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".jsonl") {
                    auto file_time = entry.last_write_time();
                    if (latest_signal_file.empty() || file_time > latest_time) {
                        latest_signal_file = entry.path().string();
                        latest_time = file_time;
                    }
                }
            }
            signal_file = latest_signal_file;
        }
        
        {
            std::ifstream sig_in(signal_file);
            std::string sig_line;
            if (std::getline(sig_in, sig_line)) {
                auto m = utils::from_json(sig_line);
                if (m.count("market_data_path")) {
                    dataset_path = m["market_data_path"];
                    // Extract just the filename for cleaner display
                    size_t last_slash = dataset_path.find_last_of("/\\");
                    if (last_slash != std::string::npos) {
                        dataset_path = dataset_path.substr(last_slash + 1);
                    }
                }
            }
        }
        
        // Get complete dataset statistics
        std::string dataset_period;
        std::string dataset_size_info;
        double test_coverage_pct = 0.0;
        
        if (!dataset_path.empty()) {
            std::string full_dataset_path = "data/equities/" + dataset_path;
            auto dataset_bars = utils::read_csv_data(full_dataset_path);
            if (!dataset_bars.empty()) {
                long long dataset_start_ts = dataset_bars.front().timestamp_ms;
                long long dataset_end_ts = dataset_bars.back().timestamp_ms;
                int total_bars = static_cast<int>(dataset_bars.size());
                int total_blocks = (total_bars + 479) / 480; // Use STANDARD_BLOCK_SIZE
                
                auto fmt_time_period = [](long long ts_ms) {
                    std::time_t t = static_cast<std::time_t>(ts_ms / 1000);
                    std::tm* g = std::gmtime(&t);
                    char buf[20]; 
                    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", g); 
                    return std::string(buf);
                };
                
                dataset_period = fmt_time_period(dataset_start_ts) + " to " + fmt_time_period(dataset_end_ts);
                dataset_size_info = std::to_string(total_blocks) + " blocks (" + std::to_string(total_bars) + " bars)";
            }
        }
        
        // Parse all lines, aggregate summary and extract period info
        std::string line;
        double total_realized = 0.0; 
        double final_unrealized = 0.0; 
        double starting_equity = 0.0; 
        double final_equity = 0.0; 
        double final_cash = 0.0;
        bool first_record = true;
        
        while (std::getline(in, line)) {
            auto m = utils::from_json(line);
            TradeEvent e;
            e.ts = m.count("timestamp_ms") ? std::stoll(m["timestamp_ms"]) : 0;
            e.sym = m.count("symbol") ? m["symbol"] : "";
            e.act = m.count("action") ? m["action"] : "";
            e.qty = m.count("quantity") ? parse_double(m["quantity"], 0.0) : 0.0;
            e.price = m.count("price") ? parse_double(m["price"], 0.0) : 0.0;
            e.tval = m.count("trade_value") ? parse_double(m["trade_value"], 0.0) : 0.0;
            e.rpnl = m.count("realized_pnl_delta") ? parse_double(m["realized_pnl_delta"], 0.0) : 0.0;
            e.eq = m.count("equity_after") ? parse_double(m["equity_after"], 0.0) : 0.0;
            e.unpnl = m.count("unrealized_after") ? parse_double(m["unrealized_after"], 0.0) : 0.0;
            
            // Extract timing info for period calculation
            if (first_record) {
                if (m.count("timestamp_ms")) first_trade_ts = std::stoll(m["timestamp_ms"]);
                if (m.count("bar_index")) first_bar_index = std::stoi(m["bar_index"]);
                first_record = false;
            }
            if (m.count("timestamp_ms")) last_trade_ts = std::stoll(m["timestamp_ms"]);
            if (m.count("bar_index")) last_bar_index = std::stoi(m["bar_index"]);
            
            std::string pos_raw = m.count("positions_summary") ? m["positions_summary"] : "";
            // Clean up positions_summary and symbol
            if (looks_like_iso(e.sym)) {
                // Symbol is corrupted, try to recover from positions_summary or use fallback
                if (!pos_raw.empty() && pos_raw.find(':') != std::string::npos) {
                    auto colon = pos_raw.find(':');
                    std::string potential_sym = pos_raw.substr(0, colon);
                    if (!looks_like_iso(potential_sym)) {
                        e.sym = potential_sym;
                    } else {
                        e.sym = "QQQ"; // fallback
                    }
                } else {
                    e.sym = "QQQ"; // fallback
                }
            }
            // Parse actual positions from positions_summary
            if (!pos_raw.empty() && pos_raw.find(':') != std::string::npos) {
                e.pos_sum = pos_raw; // Use the actual positions summary (e.g., "QQQ:103.917687")
            } else {
                e.pos_sum = e.sym + ":0"; // Fallback if no positions
            }
            e.cash_after = m.count("cash_after") ? parse_double(m["cash_after"], 0.0) : 0.0;
            e.eq_before = m.count("equity_before") ? parse_double(m["equity_before"], 0.0) : 0.0;
            trades.push_back(e);
            total_realized += e.rpnl;
        }

        if (trades.empty()) { 
            std::cout << "No trades for run: " << run_id << "\n"; 
            return 0; 
        }
        
        starting_equity = (trades.front().eq_before > 0 ? trades.front().eq_before : trades.front().eq);
        final_equity = trades.back().eq; 
        final_unrealized = trades.back().unpnl; 
        final_cash = trades.back().cash_after;
        
        // Calculate periods and blocks for display
        auto fmt_time_period = [](long long ts_ms) {
            std::time_t t = static_cast<std::time_t>(ts_ms / 1000);
            std::tm* g = std::gmtime(&t);
            char buf[20]; 
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", g); 
            return std::string(buf);
        };
        
        if (first_trade_ts > 0 && last_trade_ts > 0) {
            testset_period = fmt_time_period(first_trade_ts) + " to " + fmt_time_period(last_trade_ts);
        }
        
        if (first_bar_index >= 0 && last_bar_index >= 0) {
            int bars_used = last_bar_index - first_bar_index + 1;
            int blocks_used = (bars_used + 479) / 480; // Use STANDARD_BLOCK_SIZE
            blocks_info = std::to_string(blocks_used) + " blocks (" + std::to_string(bars_used) + " bars)";
            
            // Calculate test coverage percentage
            if (!dataset_path.empty()) {
                std::string full_dataset_path = "data/equities/" + dataset_path;
                auto dataset_bars = utils::read_csv_data(full_dataset_path);
                if (!dataset_bars.empty()) {
                    int total_bars = static_cast<int>(dataset_bars.size());
                    test_coverage_pct = (static_cast<double>(bars_used) / total_bars) * 100.0;
                }
            }
        }
        
        // Professional Dataset & Test Information Table with proper alignment
        std::cout << "\n📈 Comprehensive Trading Report for Run: " << C_CYAN << run_id << C_RESET << "\n\n";
        std::cout << C_BOLD << "📊 DATASET & TEST SUMMARY" << C_RESET << "\n";
        std::cout << "┌─────────────────────┬─────────────────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ " << C_BOLD << "Run ID" << C_RESET << std::left << std::setw(13) << ""
                  << " │ " << C_CYAN << std::left << std::setw(75) << run_id << C_RESET << " │\n";
        std::cout << "├─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤\n";
        
        if (!dataset_path.empty()) {
            std::cout << "│ " << std::left << std::setw(19) << "Dataset File"
                      << " │ " << std::left << std::setw(75) << dataset_path << " │\n";
        }
        if (!dataset_period.empty()) {
            std::cout << "│ " << std::left << std::setw(19) << "Dataset Period"
                      << " │ " << std::left << std::setw(75) << dataset_period << " │\n";
        }
        if (!dataset_size_info.empty()) {
            std::cout << "│ " << std::left << std::setw(19) << "Dataset Size"
                      << " │ " << std::left << std::setw(75) << dataset_size_info << " │\n";
        }
        
        std::cout << "├─────────────────────┼─────────────────────────────────────────────────────────────────────────────┤\n";
        
        if (!testset_period.empty()) {
            std::cout << "│ " << std::left << std::setw(19) << "Test Period"
                      << " │ " << std::left << std::setw(75) << testset_period << " │\n";
        }
        if (!blocks_info.empty()) {
            std::cout << "│ " << std::left << std::setw(19) << "Test Size"
                      << " │ " << std::left << std::setw(75) << blocks_info << " │\n";
        }
        if (test_coverage_pct > 0.0) {
            std::ostringstream coverage_str;
            coverage_str << std::fixed << std::setprecision(1) << test_coverage_pct << "% of dataset";
            std::cout << "│ " << std::left << std::setw(19) << "Test Coverage"
                      << " │ " << C_YELL << std::left << std::setw(75) << coverage_str.str() << C_RESET << " │\n";
        }
        
        std::cout << "└─────────────────────┴─────────────────────────────────────────────────────────────────────────────┘\n";

        // Account Performance Summary with professional formatting
        std::cout << "\n" << C_BOLD << "📊 ACCOUNT PERFORMANCE SUMMARY" << C_RESET << "\n";
        std::cout << "┌───────────────────────────────────────────────────────────────────────────────────────────────────┐\n";
        
        // Row 1: Starting Capital | Current Equity | Total Return
        std::ostringstream pct; 
        pct << std::fixed << std::showpos << std::setprecision(2) << ((final_equity - starting_equity)/std::max(1.0, starting_equity)*100.0) << std::noshowpos << "%";
        std::cout << "│ " << std::left << std::setw(19) << "Starting Capital"
                  << "│ " << std::right << std::setw(11) << money(starting_equity,false,11)
                  << " │ " << std::left << std::setw(19) << "Current Equity"
                  << "│ " << std::right << std::setw(11) << money(final_equity,false,11)
                  << " │ " << std::left << std::setw(12) << "Total Return"
                  << "│ " << std::right << std::setw(12) << pct.str() << "  │\n";
        
        // Row 2: Total Trades | Realized P&L | Unrealized
        std::cout << "│ " << std::left << std::setw(19) << "Total Trades"
                  << "│ " << std::right << std::setw(11) << std::to_string((int)trades.size())
                  << " │ " << std::left << std::setw(19) << "Realized P&L"
                  << "│ " << std::right << std::setw(11) << money(total_realized,true,11)
                  << " │ " << std::left << std::setw(12) << "Unrealized"
                  << "│" << std::right << std::setw(12) << money(final_unrealized,true,12) << "   │\n";
        
        // Row 3: Cash Balance | Position Value | Open Pos.
        double position_value = std::max(0.0, final_equity - final_cash);
        int open_positions = 0;
        if (!trades.back().pos_sum.empty() && trades.back().pos_sum != "CASH") {
            std::string s = trades.back().pos_sum;
            // Parse positions like "QQQ:103.917687" or "QQQ:0,SPY:50.5"
            std::stringstream ss(s);
            std::string pair;
            while (std::getline(ss, pair, ',')) {
                size_t colon_pos = pair.find(':');
                if (colon_pos != std::string::npos) {
                    std::string qty_str = pair.substr(colon_pos + 1);
                    double qty = std::stod(qty_str);
                    if (std::abs(qty) > 1e-6) { // Count non-zero positions
                        open_positions++;
                    }
                }
            }
        }
        std::cout << "│ " << std::left << std::setw(19) << "Cash Balance"
                  << "│ " << std::right << std::setw(11) << money(final_cash,false,11)
                  << " │ " << std::left << std::setw(19) << "Position Value"
                  << "│ " << std::right << std::setw(11) << money(position_value,false,11)
                  << " │ " << std::left << std::setw(12) << "Open Pos."
                  << "│ " << std::right << std::setw(12) << open_positions << "  │\n";
        
        std::cout << "└───────────────────────────────────────────────────────────────────────────────────────────────────┘\n\n";

        // Trade History Table with professional formatting
        std::cout << C_BOLD << "📈 TRADE HISTORY (Last " << std::min(max_rows, (int)trades.size()) << " of " << trades.size() << " trades)" << C_RESET;
        std::cout << C_DIM << " - Run ID: " << C_CYAN << run_id << C_RESET << "\n";
        std::cout << "┌─────────────────┬────────┬────────┬──────────┬──────────┬───────────────┬──────────────┬─────────────────┬──────────────────────────────┬─────────────────┐\n";
        std::cout << "│ Date/Time       │ Symbol │ Action │ Quantity │ Price    │  Trade Value  │  Realized P&L│  Equity After   │ Positions                    │ Unrealized P&L  │\n";
        std::cout << "├─────────────────┼────────┼────────┼──────────┼──────────┼───────────────┼──────────────┼─────────────────┼──────────────────────────────┼─────────────────┤\n";

        size_t start = (trades.size() > (size_t)max_rows) ? (trades.size() - (size_t)max_rows) : 0;
        for (size_t i = start; i < trades.size(); ++i) {
            const auto& e = trades[i];
            
            // Enhanced action formatting with emojis
            std::string action_display;
            const char* action_color = C_RESET;
            if (e.act == "BUY") {
                action_display = "🟢BUY ";
                action_color = C_GREEN;
            } else if (e.act == "SELL") {
                action_display = "🔴SELL";
                action_color = C_RED;
            } else {
                action_display = "—HOLD ";
                action_color = C_DIM;
            }
            
            // Enhanced position display
            std::string pos_display = e.pos_sum;
            if (pos_display.empty()) {
                if (e.qty == 0 || (e.act == "SELL" && std::abs(e.qty) > 0.001)) {
                    pos_display = "CASH";
                } else {
                    pos_display = e.sym + ":1";
                }
            }
            
            // Format quantity with proper sign
            std::ostringstream qty_os;
            if (e.act == "SELL") {
                qty_os << std::fixed << std::setprecision(3) << -std::abs(e.qty);
            } else {
                qty_os << std::fixed << std::setprecision(3) << std::abs(e.qty);
            }
            
            std::cout << "│ " << std::left << std::setw(15) << fmt_time(e.ts)
                      << " │ " << std::left << std::setw(6) << e.sym
                      << " │ " << action_color << std::left << std::setw(6) << action_display << C_RESET
                      << " │ " << std::right << std::setw(8) << qty_os.str()
                      << " │ " << std::right << std::setw(8) << money(e.price,false,8)
                      << " │ " << std::right << std::setw(13) << money(std::abs(e.tval),true,13)
                      << " │ " << std::right << std::setw(12) << money(e.rpnl,true,12)
                      << " │ " << std::right << std::setw(15) << money(e.eq,true,15)
                      << " │ " << std::left << std::setw(28) << pos_display
                      << " │ " << std::right << std::setw(15) << money(e.unpnl,true,15) << " │\n";
        }
        std::cout << "└─────────────────┴────────┴────────┴──────────┴──────────┴───────────────┴──────────────┴─────────────────┴──────────────────────────────┴─────────────────┘\n\n";

        // Current Positions
        std::cout << C_BOLD << "CURRENT POSITIONS" << C_RESET << "\n";
        if (!trades.back().pos_sum.empty()) {
            std::cout << trades.back().pos_sum << "\n\n";
        } else {
            std::cout << "No open positions\n\n";
        }
        
        std::cout << "📄 Trade book: " << trade_book << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Comprehensive report generation failed: " << e.what() << std::endl;
        return 1;
    }
}


} // namespace cli
} // namespace sentio
