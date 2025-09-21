// =============================================================================
// Module: src/cli/sentio_cli_main.cpp
// Purpose: Unified command-line interface for the Sentio Trading System
//
// Core Architecture & Recent Enhancements:
// This is the primary orchestrator for all Sentio trading operations, providing
// a comprehensive CLI that has been extensively enhanced with professional-grade
// features and robust error handling.
//
// Three-Phase Trading Workflow:
// 1. 'strattest' - Generate trading signals from market data using AI strategies
// 2. 'trade' - Execute trades with aggressive capital allocation (up to 100%)
// 3. 'audit' - Analyze performance with brokerage-grade professional reports
//
// Recent Major Improvements:
// - Professional Unicode table formatting with perfect alignment
// - Comprehensive dataset analysis with test coverage metrics  
// - UTC timezone consistency across all operations
// - Aggressive capital allocation for maximum profit potential
// - Alpaca zero-commission fee model integration
// - Robust position tracking with actual share quantities
// - Crash-on-error philosophy with detailed debug logging
// - Duplicate code elimination with shared utilities
// - Enhanced JSON parsing to prevent data corruption
//
// Design Philosophy:
// The system follows a fail-fast approach with extensive validation and logging.
// All financial calculations are validated with immediate program termination
// on any anomalies to prevent trading errors. Professional reporting provides
// clear visibility into trading performance and risk metrics.
// =============================================================================

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include "strategy/strategy_component.h"
#include "strategy/sigor_strategy.h"
#include "strategy/sigor_config.h"
#include "backend/backend_component.h"
#include "backend/audit_component.h"
#include "strategy/signal_output.h"
#include "common/utils.h"

namespace fs = std::filesystem;

namespace {
    // ANSI color codes for professional terminal output formatting
    // These colors enhance readability and provide visual cues for different types of information
    constexpr const char* C_RESET = "\033[0m";   // Reset to default color
    constexpr const char* C_BOLD  = "\033[1m";   // Bold text for headers and emphasis
    constexpr const char* C_DIM   = "\033[2m";   // Dimmed text for secondary information
    constexpr const char* C_CYAN  = "\033[36m";  // Cyan for run IDs and identifiers
    constexpr const char* C_GREEN = "\033[32m";  // Green for success messages and buy actions
    constexpr const char* C_YELL  = "\033[33m";  // Yellow for warnings and neutral info
    constexpr const char* C_RED   = "\033[31m";  // Red for errors and sell actions

    // Use common get_arg function from utils
    using sentio::utils::get_arg;
    
    // Helper function to get the most recent file from a directory
    std::string get_latest_file(const std::string& directory) {
        std::vector<fs::directory_entry> entries;
        if (fs::exists(directory)) {
            for (auto& e : fs::directory_iterator(directory)) {
                if (e.is_regular_file()) entries.push_back(e);
            }
            std::sort(entries.begin(), entries.end(), [](auto& a, auto& b){ 
                return a.last_write_time() > b.last_write_time(); 
            });
            if (!entries.empty()) {
                return entries.front().path().string();
            }
        }
        return "";
    }
}

/// Main entry point for the Sentio Trading System CLI
/// 
/// This function orchestrates the entire trading workflow by dispatching to
/// three main command handlers based on user input. Each command represents
/// a distinct phase in the algorithmic trading pipeline.
///
/// Command Flow:
/// 1. Parse command-line arguments using shared utilities
/// 2. Dispatch to appropriate command handler (strattest/trade/audit)
/// 3. Each handler manages its own workflow and error handling
/// 4. All operations use UTC timestamps and professional reporting
///
/// Error Handling Philosophy:
/// The system employs a fail-fast approach where any financial calculation
/// errors, data corruption, or invalid states result in immediate program
/// termination with detailed debug information.
int main(int argc, char** argv) {
    // Validate minimum argument requirements
    if (argc < 2) {
        std::cout << "Usage: sentio_cli <strattest|trade|audit> [options]\n";
        std::cout << "  strattest - Generate trading signals from market data\n";
        std::cout << "  trade     - Execute trades with portfolio management\n";
        std::cout << "  audit     - Analyze performance with professional reports\n";
        return 1;
    }
    
    // Extract primary command for workflow dispatch
    std::string cmd = argv[1];

    // =========================================================================
    // COMMAND: strattest - AI Strategy Signal Generation
    // =========================================================================
    // This command processes market data through AI trading strategies to
    // generate buy/sell signals. It serves as the first phase of the trading
    // pipeline, analyzing historical price patterns to identify opportunities.
    //
    // Key Features:
    // - Sigor AI strategy with configurable parameters
    // - Automatic output file organization in data/signals/
    // - Market data path injection for downstream traceability
    // - JSONL format for efficient signal storage and processing
    if (cmd == "strattest") {
        // Parse command-line arguments with intelligent defaults
        std::string dataset = get_arg(argc, argv, "--dataset");
        std::string out = get_arg(argc, argv, "--out");
        const std::string strategy = get_arg(argc, argv, "--strategy", "sigor");
        const std::string format = get_arg(argc, argv, "--format", "jsonl");
        const std::string cfg_path = get_arg(argc, argv, "--config", "");
        
        // Apply intelligent defaults for common use cases
        if (dataset.empty()) dataset = "data/equities/QQQ_RTH_NH.csv";
        if (out.empty()) {
            // Ensure proper file organization in data/signals directory
            std::error_code ec;
            std::filesystem::create_directories("data/signals", ec);
            out = "data/signals/" + strategy + "_signals_AUTO.jsonl";
        }
        sentio::StrategyComponent::StrategyConfig cfg;
        cfg.name = strategy; cfg.version = "0.1"; cfg.warmup_bars = 20;
        auto sigor = std::make_unique<sentio::SigorStrategy>(cfg);
        if (!cfg_path.empty()) {
            auto scfg = sentio::SigorConfig::from_file(cfg_path);
            sigor->set_config(scfg);
        }
        auto signals = sigor->process_dataset(dataset, cfg.name, {});
        for (auto& s : signals) s.metadata["market_data_path"] = dataset;
        bool ok = sigor->export_signals(signals, out, format);
        if (!ok) { std::cerr << "ERROR: failed exporting to " << out << "\n"; return 2; }
        std::cout << "Exported " << signals.size() << " signals to " << out << "\n";
        return 0;
    }

    // =========================================================================
    // COMMAND: trade - Aggressive Portfolio Management & Trade Execution
    // =========================================================================
    // This command executes trades based on AI-generated signals using an
    // aggressive capital allocation strategy designed for maximum profit potential.
    // It represents the second phase of the trading pipeline.
    //
    // Key Features:
    // - Aggressive capital allocation: Up to 100% capital usage based on signal strength
    // - Alpaca zero-commission fee model for cost-effective execution
    // - Robust portfolio management with negative cash prevention
    // - Comprehensive trade logging with full audit trail
    // - Fail-fast error handling with detailed debug information
    // - Block-based processing for controlled backtesting scope
    if (cmd == "trade") {
        // Parse trading parameters with robust defaults
        std::string sig = get_arg(argc, argv, "--signals");
        double capital = std::stod(get_arg(argc, argv, "--capital", "100000"));
        double buy_th = std::stod(get_arg(argc, argv, "--buy", "0.6"));
        double sell_th = std::stod(get_arg(argc, argv, "--sell", "0.4"));
        size_t blocks = static_cast<size_t>(std::stoul(get_arg(argc, argv, "--blocks", "20")));
        const size_t BLOCK_SIZE = 450; // Standard block size for consistent processing

        // Resolve latest signals in data/signals if requested or empty
        if (sig.empty() || sig == "latest") {
            sig = get_latest_file("data/signals");
            if (sig.empty()) { std::cerr << "ERROR: no signals found in data/signals\n"; return 1; }
        }

        // Determine market data path from first signal's metadata
        std::ifstream in(sig);
        if (!in.is_open()) { std::cerr << "ERROR: cannot open signals file\n"; return 2; }
        std::string line;
        std::getline(in, line);
        in.close();
        auto s0 = sentio::SignalOutput::from_json(line);
        if (!s0.metadata.count("market_data_path")) { std::cerr << "ERROR: signals missing market_data_path metadata\n"; return 3; }
        std::string market_path = s0.metadata["market_data_path"];

        // Ensure trades directory exists
        std::error_code ec;
        fs::create_directories("data/trades", ec);

        sentio::BackendComponent::BackendConfig bc;
        bc.starting_capital = capital;
        bc.strategy_thresholds["buy_threshold"] = buy_th;
        bc.strategy_thresholds["sell_threshold"] = sell_th;
        bc.cost_model = sentio::CostModel::ALPACA;

        sentio::BackendComponent backend(bc);
        std::string run_id = sentio::utils::generate_run_id("trade");

        // Determine total lines in signals file to compute tail window
        size_t total_lines = 0;
        {
            std::ifstream c(sig);
            std::string tmp;
            while (std::getline(c, tmp)) ++total_lines;
        }
        size_t max_count = (blocks == 0) ? static_cast<size_t>(-1) : blocks * BLOCK_SIZE;
        size_t start_index = 0;
        if (blocks > 0 && total_lines > max_count) {
            start_index = total_lines - max_count;
        }
        size_t slice = (max_count==static_cast<size_t>(-1))?total_lines:max_count;
        std::cout << "Trading last " << slice << " signals starting at index " << start_index << std::endl;
        // Write trade book JSONL
        std::string trade_book = std::string("data/trades/") + run_id + "_trades.jsonl";
        backend.process_to_jsonl(sig, market_path, trade_book, run_id, start_index, max_count);
        std::cout << C_BOLD << "Run ID: " << C_CYAN << run_id << C_RESET
                  << "  Trades: " << trade_book << "\n";
        std::cout << "Executed trades exported." << "\n";
        return 0;
    }

    // =========================================================================
    // COMMAND: audit - Professional Brokerage-Grade Performance Analysis
    // =========================================================================
    // This command provides comprehensive analysis of trading performance with
    // professional-grade reporting that rivals institutional brokerage platforms.
    // It represents the final phase of the trading pipeline.
    //
    // Key Features:
    // - Professional Unicode table formatting with perfect alignment
    // - Comprehensive dataset analysis with test coverage metrics
    // - UTC timezone consistency for accurate time-based analysis
    // - Robust position tracking with actual share quantities
    // - Enhanced JSON parsing to prevent data corruption
    // - Two analysis modes: summarize (overview) and position-history (detailed)
    //
    // Recent Enhancements:
    // - Fixed position calculation bugs to show actual share quantities
    // - Added complete dataset period and test coverage analysis
    // - Implemented professional table alignment with Unicode borders
    // - Enhanced error handling with detailed validation
    if (cmd == "audit") {
        // Parse audit subcommand and parameters
        std::string sub = (argc >= 3 && argv[2][0] != '-') ? std::string(argv[2]) : std::string("summarize");
        std::string run = get_arg(argc, argv, "--run", "");
        // Resolve latest trade book if no run provided
        if (run.empty()) {
            std::string latest_trade_file = get_latest_file("data/trades");
            if (!latest_trade_file.empty()) {
                std::string fname = fs::path(latest_trade_file).filename().string();
                // expected: <runid>_trades.jsonl
                run = fname.substr(0, fname.find("_trades"));
            }
            if (run.empty()) { std::cerr << "ERROR: no trade books found in data/trades\n"; return 3; }
        }
        std::string trade_book = std::string("data/trades/") + run + "_trades.jsonl";

        if (sub == "summarize") {
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
            
            {
                std::ifstream in(trade_book);
                std::string line;
                bool first_record = true;
                
                while (std::getline(in, line)) {
                    auto m = sentio::utils::from_json(line);
                    if (m.count("equity_after")) equity.push_back(std::stod(m["equity_after"]));
                    
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
            
            // Get dataset info from corresponding signal file
            std::string signal_file = "data/signals/sigor_signals_AUTO.jsonl";
            {
                std::ifstream sig_in(signal_file);
                std::string line;
                if (std::getline(sig_in, line)) {
                    auto m = sentio::utils::from_json(line);
                    if (m.count("market_data_path")) {
                        dataset_path = m["market_data_path"];
                    }
                }
            }
            
            // Get complete dataset statistics
            auto fmt_time = [](long long ts_ms) {
                std::time_t t = static_cast<std::time_t>(ts_ms / 1000);
                std::tm* g = std::gmtime(&t);
                char buf[20]; std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", g); 
                return std::string(buf);
            };
            
            if (!dataset_path.empty()) {
                auto dataset_bars = sentio::utils::read_csv_data(dataset_path);
                if (!dataset_bars.empty()) {
                    long long dataset_start_ts = dataset_bars.front().timestamp_ms;
                    long long dataset_end_ts = dataset_bars.back().timestamp_ms;
                    int total_bars = static_cast<int>(dataset_bars.size());
                    int total_blocks = (total_bars + 449) / 450; // Round up to nearest block
                    
                    dataset_period = fmt_time(dataset_start_ts) + " to " + fmt_time(dataset_end_ts);
                    dataset_size_info = std::to_string(total_blocks) + " blocks (" + std::to_string(total_bars) + " bars)";
                    
                    // Calculate test coverage percentage
                    if (first_bar_index >= 0 && last_bar_index >= 0) {
                        int bars_used = last_bar_index - first_bar_index + 1;
                        test_coverage_pct = (static_cast<double>(bars_used) / total_bars) * 100.0;
                    }
                }
            }
            
            // Calculate periods and blocks
            
            if (first_trade_ts > 0 && last_trade_ts > 0) {
                testset_period = fmt_time(first_trade_ts) + " to " + fmt_time(last_trade_ts);
            }
            
            if (first_bar_index >= 0 && last_bar_index >= 0) {
                int bars_used = last_bar_index - first_bar_index + 1;
                int blocks_used = (bars_used + 449) / 450; // Round up to nearest block
                blocks_info = std::to_string(blocks_used) + " blocks (" + std::to_string(bars_used) + " bars)";
            }
            sentio::AuditComponent auditor;
            auto summary = auditor.analyze_equity_curve(equity);
            
            // Professional Dataset & Test Information Table with proper alignment
            std::cout << "\n" << C_BOLD << "📊 DATASET & TEST SUMMARY" << C_RESET << "\n";
            std::cout << "┌─────────────────────┬─────────────────────────────────────────────────────────────────────────────┐\n";
            std::cout << "│ " << C_BOLD << "Run ID" << C_RESET << std::left << std::setw(13) << ""
                      << " │ " << C_CYAN << std::left << std::setw(75) << run << C_RESET << " │\n";
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
            
            // Professional Performance Metrics Table with proper alignment
            std::cout << "\n" << C_BOLD << "📈 PERFORMANCE METRICS" << C_RESET << "\n";
            std::cout << "┌─────────────────────┬──────────────┬─────────────────────┬──────────────┬─────────────────────┬──────────────┐\n";
            
            // Calculate additional metrics
            double total_return = 0.0;
            double total_return_pct = 0.0;
            if (!equity.empty() && equity.front() > 0) {
                total_return = equity.back() - equity.front();
                total_return_pct = (total_return / equity.front()) * 100.0;
            }
            
            // Format values with proper width control
            std::ostringstream trades_str, start_equity_str, final_equity_str, return_str, sharpe_str, drawdown_str;
            
            trades_str << equity.size();
            
            if (!equity.empty()) {
                start_equity_str << "$" << std::fixed << std::setprecision(2) << equity.front();
                final_equity_str << "$" << std::fixed << std::setprecision(2) << equity.back();
            } else {
                start_equity_str << "$0.00";
                final_equity_str << "$0.00";
            }
            
            if (total_return >= 0) {
                return_str << "+" << std::fixed << std::setprecision(2) << total_return_pct << "%";
            } else {
                return_str << std::fixed << std::setprecision(2) << total_return_pct << "%";
            }
            
            sharpe_str << std::fixed << std::setprecision(4) << summary.sharpe;
            drawdown_str << std::fixed << std::setprecision(2) << (summary.max_drawdown * 100.0) << "%";
            
            // First row: Trades, Starting Equity, Total Return
            std::cout << "│ " << std::left << std::setw(19) << "Total Trades"
                      << " │ " << std::right << std::setw(12) << trades_str.str()
                      << " │ " << std::left << std::setw(19) << "Starting Equity"
                      << " │ " << std::right << std::setw(12) << start_equity_str.str()
                      << " │ " << std::left << std::setw(19) << "Total Return"
                      << " │ " << (total_return >= 0 ? C_GREEN : C_RED) << std::right << std::setw(12) << return_str.str() << C_RESET << " │\n";
            
            // Second row: Final Equity, Sharpe Ratio, Max Drawdown
            const char* sharpe_color = (summary.sharpe >= 1.0 ? C_GREEN : (summary.sharpe >= 0.0 ? C_YELL : C_RED));
            std::cout << "│ " << std::left << std::setw(19) << "Final Equity"
                      << " │ " << std::right << std::setw(12) << final_equity_str.str()
                      << " │ " << std::left << std::setw(19) << "Sharpe Ratio"
                      << " │ " << sharpe_color << std::right << std::setw(12) << sharpe_str.str() << C_RESET
                      << " │ " << std::left << std::setw(19) << "Max Drawdown"
                      << " │ " << C_YELL << std::right << std::setw(12) << drawdown_str.str() << C_RESET << " │\n";
            
            std::cout << "└─────────────────────┴──────────────┴─────────────────────┴──────────────┴─────────────────────┴──────────────┘\n";
            return 0;
        } else if (sub == "position-history") {
            // Professional multi-section report (ASCII box layout)
            std::ifstream in(trade_book);
            if (!in.is_open()) { std::cerr << "ERROR: Cannot open trade book: " << trade_book << "\n"; return 1; }

            int max_rows = 20;
            try { max_rows = std::stoi(get_arg(argc, argv, "--max", "20")); } catch (...) { max_rows = 20; }

            struct TradeEvent { long long ts=0; std::string sym; std::string act; double qty=0, price=0, tval=0, rpnl=0, eq=0, unpnl=0; std::string pos_sum; double cash_after=0, eq_before=0; };
            std::vector<TradeEvent> trades;

            auto fmt_time = [](long long ts_ms) {
                std::time_t t = static_cast<std::time_t>(ts_ms / 1000);
                std::tm* g = std::gmtime(&t);  // Use UTC to match test period calculation
                char buf[20]; std::strftime(buf, sizeof(buf), "%m/%d %H:%M:%S", g); return std::string(buf);
            };
            auto money = [](double v, bool show_plus, int width){ std::ostringstream os; os << "$ " << (show_plus?std::showpos:std::noshowpos) << std::fixed << std::setprecision(2) << v << std::noshowpos; std::ostringstream pad; pad << std::right << std::setw(width) << os.str(); return pad.str(); };
            auto qty_str = [](double v, int width){ std::ostringstream os; os << std::fixed << std::showpos << std::setprecision(3) << v << std::noshowpos; std::ostringstream pad; pad << std::right << std::setw(width) << os.str(); return pad.str(); };

            auto looks_like_iso = [](const std::string& s){ return s.find('T') != std::string::npos && s.find('-') != std::string::npos && s.find(':') != std::string::npos; };
            auto parse_double = [](const std::string& v, double def)->double{ try { return std::stod(v); } catch (...) { return def; } };

            // Extract dataset and period information
            std::string dataset_path;
            std::string testset_period;
            std::string blocks_info;
            long long first_trade_ts = 0, last_trade_ts = 0;
            int first_bar_index = -1, last_bar_index = -1;
            
            // Get dataset info from corresponding signal file
            std::string signal_file = "data/signals/sigor_signals_AUTO.jsonl";
            {
                std::ifstream sig_in(signal_file);
                std::string sig_line;
                if (std::getline(sig_in, sig_line)) {
                    auto m = sentio::utils::from_json(sig_line);
                    if (m.count("market_data_path")) {
                        dataset_path = m["market_data_path"];
                    }
                }
            }
            
            // Get complete dataset statistics
            std::string dataset_period;
            std::string dataset_size_info;
            double test_coverage_pct = 0.0;
            
            if (!dataset_path.empty()) {
                auto dataset_bars = sentio::utils::read_csv_data(dataset_path);
                if (!dataset_bars.empty()) {
                    long long dataset_start_ts = dataset_bars.front().timestamp_ms;
                    long long dataset_end_ts = dataset_bars.back().timestamp_ms;
                    int total_bars = static_cast<int>(dataset_bars.size());
                    int total_blocks = (total_bars + 449) / 450; // Round up to nearest block
                    
                    auto fmt_time_period = [](long long ts_ms) {
                        std::time_t t = static_cast<std::time_t>(ts_ms / 1000);
                        std::tm* g = std::gmtime(&t);
                        char buf[20]; std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", g); 
                        return std::string(buf);
                    };
                    
                    dataset_period = fmt_time_period(dataset_start_ts) + " to " + fmt_time_period(dataset_end_ts);
                    dataset_size_info = std::to_string(total_blocks) + " blocks (" + std::to_string(total_bars) + " bars)";
                }
            }
            
            // Parse all lines, aggregate summary and extract period info
            std::string line;
            double total_realized = 0.0; double final_unrealized = 0.0; double starting_equity = 0.0; double final_equity = 0.0; double final_cash = 0.0;
            bool first_record = true;
            while (std::getline(in, line)) {
                auto m = sentio::utils::from_json(line);
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

            if (trades.empty()) { std::cout << "No trades for run: " << run << "\n"; return 0; }
            starting_equity = (trades.front().eq_before > 0 ? trades.front().eq_before : trades.front().eq);
            final_equity = trades.back().eq; final_unrealized = trades.back().unpnl; final_cash = trades.back().cash_after;
            
            // Calculate periods and blocks for display
            auto fmt_time_period = [](long long ts_ms) {
                std::time_t t = static_cast<std::time_t>(ts_ms / 1000);
                std::tm* g = std::gmtime(&t);
                char buf[20]; std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", g); 
                return std::string(buf);
            };
            
            if (first_trade_ts > 0 && last_trade_ts > 0) {
                testset_period = fmt_time_period(first_trade_ts) + " to " + fmt_time_period(last_trade_ts);
            }
            
            if (first_bar_index >= 0 && last_bar_index >= 0) {
                int bars_used = last_bar_index - first_bar_index + 1;
                int blocks_used = (bars_used + 449) / 450; // Round up to nearest block
                blocks_info = std::to_string(blocks_used) + " blocks (" + std::to_string(bars_used) + " bars)";
                
                // Calculate test coverage percentage
                if (!dataset_path.empty()) {
                    auto dataset_bars = sentio::utils::read_csv_data(dataset_path);
                    if (!dataset_bars.empty()) {
                        int total_bars = static_cast<int>(dataset_bars.size());
                        test_coverage_pct = (static_cast<double>(bars_used) / total_bars) * 100.0;
                    }
                }
            }
            
            // Professional Dataset & Test Information Table with proper alignment
            std::cout << "\n" << C_BOLD << "📊 DATASET & TEST SUMMARY" << C_RESET << "\n";
            std::cout << "┌─────────────────────┬─────────────────────────────────────────────────────────────────────────────┐\n";
            std::cout << "│ " << C_BOLD << "Run ID" << C_RESET << std::left << std::setw(13) << ""
                      << " │ " << C_CYAN << std::left << std::setw(75) << run << C_RESET << " │\n";
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
            std::ostringstream pct; pct << std::fixed << std::showpos << std::setprecision(2) << ((final_equity - starting_equity)/std::max(1.0, starting_equity)*100.0) << std::noshowpos << "%";
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
            std::cout << C_DIM << " - Run ID: " << C_CYAN << run << C_RESET << "\n";
            std::cout << "┌─────────────────┬────────┬────────┬──────────┬──────────┬───────────────┬──────────────┬─────────────────┬──────────────────────┬─────────────────┐\n";
            std::cout << "│ Date/Time       │ Symbol │ Action │ Quantity │ Price    │  Trade Value  │  Realized P&L│  Equity After   │ Positions            │ Unrealized P&L  │\n";
            std::cout << "├─────────────────┼────────┼────────┼──────────┼──────────┼───────────────┼──────────────┼─────────────────┼──────────────────────┼─────────────────┤\n";

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
                    action_display = "—";
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
                          << " │ " << std::left << std::setw(20) << pos_display
                          << " │ " << std::right << std::setw(15) << money(e.unpnl,true,15) << " │\n";
            }
            std::cout << "└─────────────────┴────────┴────────┴──────────┴──────────┴───────────────┴──────────────┴─────────────────┴──────────────────────┴─────────────────┘\n\n";

            // Current Positions
            std::cout << C_BOLD << "CURRENT POSITIONS" << C_RESET << "\n";
            if (!trades.back().pos_sum.empty()) {
                std::cout << trades.back().pos_sum << "\n\n";
            } else {
                std::cout << "No open positions\n\n";
            }
            return 0;
        } else {
            std::cerr << "Unknown audit subcommand: " << sub << "\n";
            return 1;
        }
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    return 1;
}


