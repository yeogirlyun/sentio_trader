#include "backend/backend_component.h"
#include "backend/portfolio_manager.h"
#include "common/utils.h"

#include <fstream>
#include <sstream>

namespace sentio {

BackendComponent::BackendComponent(const BackendConfig& config)
    : config_(config), db_(nullptr) {
    portfolio_manager_ = std::make_unique<PortfolioManager>(config.starting_capital);
    position_manager_ = std::make_unique<StaticPositionManager>();
}

BackendComponent::~BackendComponent() {
    close_database();
}

std::vector<BackendComponent::TradeOrder> BackendComponent::process_signals(
    const std::string& signal_file_path,
    const std::string& market_data_path,
    const BackendConfig& /*config*/) {

    std::vector<TradeOrder> trades;

    // Read signals (JSONL)
    std::vector<SignalOutput> signals;
    std::ifstream signal_file(signal_file_path);
    std::string line;
    while (std::getline(signal_file, line)) {
        signals.push_back(SignalOutput::from_json(line));
    }

    // Read market data
    auto bars = utils::read_csv_data(market_data_path);

    // Process each signal with corresponding bar
    for (size_t i = 0; i < signals.size() && i < bars.size(); ++i) {
        const auto& signal = signals[i];
        const auto& bar = bars[i];

        // Update market prices in portfolio
        portfolio_manager_->update_market_prices({{bar.symbol, bar.close}});

        // Evaluate signal and generate trade order
        auto order = evaluate_signal(signal, bar);

        // Check for conflicts if enabled
        if (config_.enable_conflict_prevention) {
            order.conflict_check_passed = !check_conflicts(order);
            if (!order.conflict_check_passed) {
                order.action = TradeAction::HOLD;
                order.rejection_reason = "Conflict detected";
            }
        }

        // Execute trade if not HOLD
        if (order.action != TradeAction::HOLD) {
            if (order.action == TradeAction::BUY) {
                portfolio_manager_->execute_buy(order.symbol, order.quantity, order.price, order.fees);
                position_manager_->add_position(order.symbol);
                position_manager_->update_direction(StaticPositionManager::Direction::LONG);
            } else if (order.action == TradeAction::SELL) {
                portfolio_manager_->execute_sell(order.symbol, order.quantity, order.price, order.fees);
                position_manager_->remove_position(order.symbol);
            }
        }

        // Record portfolio state after trade
        order.after_state = portfolio_manager_->get_state();
        trades.push_back(order);
    }

    return trades;
}

BackendComponent::TradeOrder BackendComponent::evaluate_signal(
    const SignalOutput& signal, const Bar& bar) {

    TradeOrder order;
    order.timestamp_ms = signal.timestamp_ms;
    order.bar_index = signal.bar_index;
    order.symbol = signal.symbol;
    order.signal_probability = signal.probability;
    order.signal_confidence = signal.confidence;
    order.price = bar.close;
    order.before_state = portfolio_manager_->get_state();

    // Decision logic based on probability thresholds
    double buy_threshold = 0.6;
    double sell_threshold = 0.4;
    if (auto it = config_.strategy_thresholds.find("buy_threshold"); it != config_.strategy_thresholds.end()) {
        buy_threshold = it->second;
    }
    if (auto it = config_.strategy_thresholds.find("sell_threshold"); it != config_.strategy_thresholds.end()) {
        sell_threshold = it->second;
    }

    if (signal.probability > buy_threshold && signal.confidence > 0.5) {
        // Buy signal
        if (!portfolio_manager_->has_position(signal.symbol)) {
            double available_capital = portfolio_manager_->get_cash_balance();
            double position_size = calculate_position_size(signal.probability, available_capital);
            order.quantity = position_size / bar.close;
            order.trade_value = position_size;
            order.fees = calculate_fees(order.trade_value);
            if (portfolio_manager_->can_buy(signal.symbol, order.quantity, bar.close)) {
                order.action = TradeAction::BUY;
                order.execution_reason = "Buy signal above threshold";
            } else {
                order.action = TradeAction::HOLD;
                order.rejection_reason = "Insufficient capital";
            }
        } else {
            order.action = TradeAction::HOLD;
            order.execution_reason = "Already has position";
        }
    } else if (signal.probability < sell_threshold) {
        // Sell signal
        if (portfolio_manager_->has_position(signal.symbol)) {
            auto position = portfolio_manager_->get_position(signal.symbol);
            order.quantity = position.quantity;
            order.trade_value = order.quantity * bar.close;
            order.fees = calculate_fees(order.trade_value);
            order.action = TradeAction::SELL;
            order.execution_reason = "Sell signal below threshold";
        } else {
            order.action = TradeAction::HOLD;
            order.execution_reason = "No position to sell";
        }
    } else {
        order.action = TradeAction::HOLD;
        order.execution_reason = "Signal in neutral zone";
    }

    return order;
}

bool BackendComponent::check_conflicts(const TradeOrder& order) {
    return position_manager_->would_cause_conflict(order.symbol, order.action);
}

double BackendComponent::calculate_fees(double trade_value) {
    switch (config_.cost_model) {
        case CostModel::ZERO:       return 0.0;
        case CostModel::FIXED:      return 1.0;                 // $1 per trade
        case CostModel::PERCENTAGE: return trade_value * 0.001; // 0.1%
        case CostModel::ALPACA:     return 0.0;                 // zero commission
        default:                    return 0.0;
    }
}

double BackendComponent::calculate_position_size(double signal_probability, double available_capital) {
    // Kelly-inspired sizing: scale with edge over 0.5
    double confidence_factor = std::clamp((signal_probability - 0.5) * 2.0, 0.0, 1.0);
    double base_size = available_capital * config_.max_position_size;
    return base_size * confidence_factor;
}

bool BackendComponent::export_trades(
    const std::vector<TradeOrder>& trades,
    const std::string& db_path,
    const std::string& run_id) {

    if (!init_database(db_path)) {
        return false;
    }
    if (!create_tables()) {
        return false;
    }

    // Begin transaction
    sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

    // Insert run information (simplified)
    std::string run_sql = "INSERT INTO runs (run_id, strategy_name, dataset_path, config_hash, started_at, status) VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, run_sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, "backend", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, "signals", -1, SQLITE_STATIC);
    auto cfg_hash = utils::calculate_sha256(run_id);
    sqlite3_bind_text(stmt, 4, cfg_hash.c_str(), -1, SQLITE_TRANSIENT);
    auto ts = utils::current_timestamp_str();
    sqlite3_bind_text(stmt, 5, ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, "completed", -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Insert trades
    for (const auto& trade : trades) {
        std::string trade_sql =
            "INSERT INTO trades (run_id, timestamp_ms, bar_index, symbol, action, "
            "quantity, price, trade_value, fees, cash_before, equity_before, "
            "positions_before, cash_after, equity_after, positions_after, "
            "signal_probability, signal_confidence, execution_reason, "
            "rejection_reason, conflict_check_passed) "
            "VALUES " + trade.to_sql_values();
        sqlite3_exec(db_, trade_sql.c_str(), nullptr, nullptr, nullptr);
    }

    // Commit transaction
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
    return true;
}

bool BackendComponent::init_database(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        return false;
    }
    return true;
}

void BackendComponent::close_database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool BackendComponent::create_tables() {
    const char* create_runs_table = R"(
        CREATE TABLE IF NOT EXISTS runs (
            run_id TEXT PRIMARY KEY,
            strategy_name TEXT NOT NULL,
            dataset_path TEXT NOT NULL,
            config_hash TEXT NOT NULL,
            started_at TIMESTAMP NOT NULL,
            completed_at TIMESTAMP,
            status TEXT NOT NULL
        )
    )";

    const char* create_trades_table = R"(
        CREATE TABLE IF NOT EXISTS trades (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id TEXT NOT NULL,
            timestamp_ms INTEGER NOT NULL,
            bar_index INTEGER NOT NULL,
            symbol TEXT NOT NULL,
            action TEXT NOT NULL,
            quantity REAL NOT NULL,
            price REAL NOT NULL,
            trade_value REAL NOT NULL,
            fees REAL NOT NULL,
            cash_before REAL NOT NULL,
            equity_before REAL NOT NULL,
            positions_before TEXT NOT NULL,
            cash_after REAL NOT NULL,
            equity_after REAL NOT NULL,
            positions_after TEXT NOT NULL,
            signal_probability REAL,
            signal_confidence REAL,
            execution_reason TEXT,
            rejection_reason TEXT,
            conflict_check_passed BOOLEAN NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            chain_hash TEXT,
            FOREIGN KEY (run_id) REFERENCES runs(run_id)
        )
    )";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, create_runs_table, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        return false;
    }
    rc = sqlite3_exec(db_, create_trades_table, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(err_msg);
        return false;
    }

    // Indexes
    sqlite3_exec(db_, "CREATE INDEX IF NOT EXISTS idx_trades_run_timestamp ON trades(run_id, timestamp_ms)", nullptr, nullptr, nullptr);
    return true;
}

} // namespace sentio


