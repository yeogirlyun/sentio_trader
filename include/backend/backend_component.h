#pragma once

// =============================================================================
// Module: backend/backend_component.h
// Purpose: Transform strategy signals and market data into executable orders,
//          update portfolio state, and persist audit trails to SQLite.
//
// Core idea:
// - The Backend is an application/service layer that consumes pure domain types
//   and orchestrates execution without embedding business rules in the UI.
// - It can run headless or be driven by a CLI/UI adapter.
// =============================================================================

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <sqlite3.h>
#include "common/types.h"
#include "strategy/signal_output.h"

namespace sentio {

class BackendComponent {
public:
    struct TradeOrder {
        int64_t timestamp_ms = 0;
        int bar_index = 0;
        std::string symbol;
        TradeAction action = TradeAction::HOLD;
        double quantity = 0.0;
        double price = 0.0;
        double trade_value = 0.0;
        double fees = 0.0;

        PortfolioState before_state;  // snapshot prior to execution
        PortfolioState after_state;   // snapshot after execution

        double signal_probability = 0.0;
        double signal_confidence = 0.0;

        std::string execution_reason;
        std::string rejection_reason;
        bool conflict_check_passed = true;

        std::string to_sql_values() const; // VALUES (...)
    };

    struct BackendConfig {
        double starting_capital = 100000.0;
        double max_position_size = 0.25; // fraction of equity
        bool enable_conflict_prevention = true;
        CostModel cost_model = CostModel::ALPACA;
        std::map<std::string, double> strategy_thresholds; // buy_threshold, sell_threshold
    };

    explicit BackendComponent(const BackendConfig& config);
    ~BackendComponent();

    // Main processing: turn signals + bars into executed trades
    std::vector<TradeOrder> process_signals(
        const std::string& signal_file_path,
        const std::string& market_data_path,
        const BackendConfig& config
    );

    // Persist trades and run metadata to SQLite database
    bool export_trades(
        const std::vector<TradeOrder>& trades,
        const std::string& db_path,
        const std::string& run_id
    );

private:
    BackendConfig config_;
    std::unique_ptr<class PortfolioManager> portfolio_manager_;
    std::unique_ptr<class StaticPositionManager> position_manager_;
    sqlite3* db_;

    bool init_database(const std::string& db_path);
    void close_database();
    bool create_tables();

    TradeOrder evaluate_signal(const SignalOutput& signal, const Bar& bar);
    bool check_conflicts(const TradeOrder& order);
    double calculate_fees(double trade_value);
    double calculate_position_size(double signal_probability, double available_capital);
};

} // namespace sentio


