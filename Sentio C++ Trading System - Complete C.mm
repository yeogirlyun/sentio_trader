# Sentio C++ Trading System - Complete Codebase

## Project Structure
```
sentio/
├── CMakeLists.txt
├── include/
│   ├── common/
│   │   ├── types.h
│   │   ├── config.h
│   │   └── utils.h
│   ├── strategy/
│   │   ├── strategy_component.h
│   │   └── signal_output.h
│   ├── backend/
│   │   ├── backend_component.h
│   │   ├── portfolio_manager.h
│   │   └── trade_executor.h
│   └── audit/
│       ├── audit_component.h
│       └── compliance_checker.h
├── src/
│   ├── common/
│   │   └── utils.cpp
│   ├── strategy/
│   │   ├── strategy_component.cpp
│   │   └── main_strategy.cpp
│   ├── backend/
│   │   ├── backend_component.cpp
│   │   ├── portfolio_manager.cpp
│   │   └── main_backend.cpp
│   └── audit/
│       ├── audit_component.cpp
│       └── main_audit.cpp
└── tests/
    └── ...
```

## 1. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(Sentio VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Add subdirectories
add_subdirectory(src/common)
add_subdirectory(src/strategy)
add_subdirectory(src/backend)
add_subdirectory(src/audit)

# Common library
add_library(sentio_common 
    src/common/utils.cpp
)

# Strategy executable
add_executable(sentio_strategy
    src/strategy/main_strategy.cpp
    src/strategy/strategy_component.cpp
)
target_link_libraries(sentio_strategy 
    sentio_common
    Threads::Threads
)

# Backend executable
add_executable(sentio_backend
    src/backend/main_backend.cpp
    src/backend/backend_component.cpp
    src/backend/portfolio_manager.cpp
)
target_link_libraries(sentio_backend 
    sentio_common
    SQLite3::SQLite3
    Threads::Threads
)

# Audit executable
add_executable(sentio_audit
    src/audit/main_audit.cpp
    src/audit/audit_component.cpp
)
target_link_libraries(sentio_audit 
    sentio_common
    SQLite3::SQLite3
    Threads::Threads
)
```

## 2. Common Headers

### include/common/types.h
```cpp
#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>

namespace sentio {

// Market data structure
struct Bar {
    int64_t timestamp_ms;
    double open;
    double high;
    double low;
    double close;
    double volume;
    std::string symbol;
};

// Position information
struct Position {
    std::string symbol;
    double quantity;
    double avg_price;
    double current_price;
    double unrealized_pnl;
    double realized_pnl;
};

// Portfolio state
struct PortfolioState {
    double cash_balance;
    double total_equity;
    double unrealized_pnl;
    double realized_pnl;
    std::map<std::string, Position> positions;
    int64_t timestamp_ms;
    
    std::string to_json() const;
    static PortfolioState from_json(const std::string& json_str);
};

// Trade types
enum class TradeAction {
    BUY,
    SELL,
    HOLD
};

// Cost model types
enum class CostModel {
    ZERO,
    FIXED,
    PERCENTAGE,
    ALPACA
};

} // namespace sentio
```

### src/backend/portfolio_manager.cpp
```cpp
#include "backend/portfolio_manager.h"
#include <numeric>
#include <algorithm>

namespace sentio {

PortfolioManager::PortfolioManager(double starting_capital)
    : cash_balance_(starting_capital), realized_pnl_(0.0) {
}

bool PortfolioManager::can_buy(const std::string& symbol, double quantity, double price) {
    double required_capital = quantity * price;
    return cash_balance_ >= required_capital;
}

bool PortfolioManager::can_sell(const std::string& symbol, double quantity) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return false;
    }
    return it->second.quantity >= quantity;
}

void PortfolioManager::execute_buy(const std::string& symbol, double quantity, 
                                   double price, double fees) {
    double total_cost = (quantity * price) + fees;
    cash_balance_ -= total_cost;
    
    update_position(symbol, quantity, price);
}

void PortfolioManager::execute_sell(const std::string& symbol, double quantity, 
                                    double price, double fees) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return;
    }
    
    double proceeds = (quantity * price) - fees;
    cash_balance_ += proceeds;
    
    // Calculate realized P&L
    double cost_basis = it->second.avg_price * quantity;
    double gross_pnl = (price - it->second.avg_price) * quantity;
    realized_pnl_ += (gross_pnl - fees);
    
    // Update or remove position
    if (it->second.quantity > quantity) {
        it->second.quantity -= quantity;
    } else {
        positions_.erase(it);
    }
}

PortfolioState PortfolioManager::get_state() const {
    PortfolioState state;
    state.cash_balance = cash_balance_;
    state.total_equity = get_total_equity();
    state.unrealized_pnl = get_unrealized_pnl();
    state.realized_pnl = realized_pnl_;
    state.positions = positions_;
    state.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return state;
}

void PortfolioManager::update_market_prices(const std::map<std::string, double>& prices) {
    for (auto& [symbol, position] : positions_) {
        auto price_it = prices.find(symbol);
        if (price_it != prices.end()) {
            position.current_price = price_it->second;
            position.unrealized_pnl = 
                (position.current_price - position.avg_price) * position.quantity;
        }
    }
}

double PortfolioManager::get_total_equity() const {
    double positions_value = 0;
    for (const auto& [symbol, position] : positions_) {
        positions_value += position.quantity * position.current_price;
    }
    return cash_balance_ + positions_value;
}

double PortfolioManager::get_unrealized_pnl() const {
    double total_unrealized = 0;
    for (const auto& [symbol, position] : positions_) {
        total_unrealized += position.unrealized_pnl;
    }
    return total_unrealized;
}

bool PortfolioManager::has_position(const std::string& symbol) const {
    return positions_.find(symbol) != positions_.end();
}

Position PortfolioManager::get_position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    if (it != positions_.end()) {
        return it->second;
    }
    return Position{};
}

void PortfolioManager::update_position(const std::string& symbol, 
                                       double quantity, double price) {
    auto it = positions_.find(symbol);
    if (it != positions_.end()) {
        // Update existing position (average in)
        double total_quantity = it->second.quantity + quantity;
        double total_value = (it->second.quantity * it->second.avg_price) + 
                           (quantity * price);
        it->second.avg_price = total_value / total_quantity;
        it->second.quantity = total_quantity;
        it->second.current_price = price;
    } else {
        // Create new position
        Position pos;
        pos.symbol = symbol;
        pos.quantity = quantity;
        pos.avg_price = price;
        pos.current_price = price;
        pos.unrealized_pnl = 0;
        pos.realized_pnl = 0;
        positions_[symbol] = pos;
    }
}

// StaticPositionManager implementation
StaticPositionManager::StaticPositionManager()
    : current_direction_(Direction::NEUTRAL), position_count_(0) {
}

bool StaticPositionManager::would_cause_conflict(const std::string& symbol, 
                                                 TradeAction action) {
    if (action == TradeAction::BUY) {
        bool is_inverse = is_inverse_etf(symbol);
        
        // Check if buying would conflict with current direction
        if (current_direction_ == Direction::SHORT && !is_inverse) {
            return true; // Conflict: trying to go long while short
        }
        if (current_direction_ == Direction::LONG && is_inverse) {
            return true; // Conflict: trying to short while long
        }
    }
    return false;
}

void StaticPositionManager::update_direction(Direction new_direction) {
    if (position_count_ == 0) {
        current_direction_ = Direction::NEUTRAL;
    } else {
        current_direction_ = new_direction;
    }
}

void StaticPositionManager::add_position(const std::string& symbol) {
    symbol_positions_[symbol]++;
    position_count_++;
}

void StaticPositionManager::remove_position(const std::string& symbol) {
    auto it = symbol_positions_.find(symbol);
    if (it != symbol_positions_.end()) {
        it->second--;
        if (it->second <= 0) {
            symbol_positions_.erase(it);
        }
        position_count_--;
    }
}

bool StaticPositionManager::is_inverse_etf(const std::string& symbol) const {
    // List of common inverse ETFs
    static const std::vector<std::string> inverse_etfs = {
        "PSQ", "SH", "SDS", "SPXS", "SQQQ", "QID", "DXD", "SDOW", "DOG"
    };
    
    return std::find(inverse_etfs.begin(), inverse_etfs.end(), symbol) 
           != inverse_etfs.end();
}

} // namespace sentio
```

### include/common/config.h
```cpp
#pragma once

#include <string>
#include <map>
#include <memory>
#include <fstream>

namespace sentio {

class Config {
public:
    static std::shared_ptr<Config> from_yaml(const std::string& path);
    static std::shared_ptr<Config> from_json(const std::string& path);
    
    template<typename T>
    T get(const std::string& key) const;
    
    template<typename T>
    T get(const std::string& key, const T& default_value) const;
    
    bool has(const std::string& key) const;
    
private:
    std::map<std::string, std::string> data_;
};

} // namespace sentio
```

### include/common/utils.h
```cpp
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include "types.h"

namespace sentio {
namespace utils {

// File I/O utilities
std::vector<Bar> read_csv_data(const std::string& path);
bool write_jsonl(const std::string& path, const std::vector<std::string>& lines);
bool write_csv(const std::string& path, const std::vector<std::vector<std::string>>& data);

// Time utilities
int64_t timestamp_to_ms(const std::string& timestamp_str);
std::string ms_to_timestamp(int64_t ms);
std::string current_timestamp_str();

// JSON utilities
std::string to_json(const std::map<std::string, std::string>& data);
std::map<std::string, std::string> from_json(const std::string& json_str);

// Hash utilities
std::string calculate_sha256(const std::string& data);
std::string generate_run_id(const std::string& prefix);

// Math utilities
double calculate_sharpe_ratio(const std::vector<double>& returns, double risk_free_rate = 0.0);
double calculate_max_drawdown(const std::vector<double>& equity_curve);

} // namespace utils
} // namespace sentio
```

## 3. Strategy Component

### include/strategy/signal_output.h
```cpp
#pragma once

#include <string>
#include <map>
#include <cstdint>

namespace sentio {

struct SignalOutput {
    int64_t timestamp_ms;
    int bar_index;
    std::string symbol;
    double probability;      // 0.0 to 1.0
    double confidence;       // 0.0 to 1.0
    std::string strategy_name;
    std::string strategy_version;
    std::map<std::string, std::string> metadata;
    
    std::string to_json() const;
    std::string to_csv() const;
    static SignalOutput from_json(const std::string& json_str);
};

} // namespace sentio
```

### include/strategy/strategy_component.h
```cpp
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>
#include "common/types.h"
#include "signal_output.h"

namespace sentio {

class StrategyComponent {
public:
    struct StrategyConfig {
        std::string name = "default";
        std::string version = "1.0";
        double buy_threshold = 0.6;
        double sell_threshold = 0.4;
        int warmup_bars = 250;
        std::map<std::string, double> params;
    };
    
    StrategyComponent(const StrategyConfig& config);
    virtual ~StrategyComponent() = default;
    
    // Main processing function
    std::vector<SignalOutput> process_dataset(
        const std::string& dataset_path,
        const std::string& strategy_name,
        const std::map<std::string, std::string>& strategy_params
    );
    
    // Export signals to file
    bool export_signals(
        const std::vector<SignalOutput>& signals,
        const std::string& output_path,
        const std::string& format = "jsonl"
    );
    
protected:
    // Override in derived strategies
    virtual SignalOutput generate_signal(const Bar& bar, int bar_index);
    virtual void update_indicators(const Bar& bar);
    virtual bool is_warmed_up() const;
    
private:
    StrategyConfig config_;
    std::vector<Bar> historical_bars_;
    int bars_processed_;
    bool warmup_complete_;
    
    // Internal indicators (example)
    std::vector<double> moving_average_;
    std::vector<double> volatility_;
    std::vector<double> momentum_;
};

// Concrete strategy implementation example
class SigorStrategy : public StrategyComponent {
public:
    SigorStrategy(const StrategyConfig& config);
    
protected:
    SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    void update_indicators(const Bar& bar) override;
    bool is_warmed_up() const override;
    
private:
    // Sigor-specific indicators
    double calculate_buy_probability(const Bar& bar);
    double calculate_confidence(const Bar& bar);
};

} // namespace sentio
```

### src/strategy/strategy_component.cpp
```cpp
#include "strategy/strategy_component.h"
#include "common/utils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <numeric>

namespace sentio {

StrategyComponent::StrategyComponent(const StrategyConfig& config)
    : config_(config), bars_processed_(0), warmup_complete_(false) {
    historical_bars_.reserve(config_.warmup_bars);
}

std::vector<SignalOutput> StrategyComponent::process_dataset(
    const std::string& dataset_path,
    const std::string& strategy_name,
    const std::map<std::string, std::string>& strategy_params) {
    
    std::vector<SignalOutput> signals;
    auto bars = utils::read_csv_data(dataset_path);
    
    for (size_t i = 0; i < bars.size(); ++i) {
        const auto& bar = bars[i];
        update_indicators(bar);
        
        if (is_warmed_up()) {
            auto signal = generate_signal(bar, static_cast<int>(i));
            signal.strategy_name = strategy_name;
            signal.strategy_version = config_.version;
            signals.push_back(signal);
        }
        
        bars_processed_++;
    }
    
    return signals;
}

bool StrategyComponent::export_signals(
    const std::vector<SignalOutput>& signals,
    const std::string& output_path,
    const std::string& format) {
    
    if (format == "jsonl") {
        std::vector<std::string> lines;
        for (const auto& signal : signals) {
            lines.push_back(signal.to_json());
        }
        return utils::write_jsonl(output_path, lines);
    } else if (format == "csv") {
        std::vector<std::vector<std::string>> data;
        // Add header
        data.push_back({"timestamp_ms", "bar_index", "symbol", "probability", 
                       "confidence", "strategy_name", "strategy_version"});
        
        for (const auto& signal : signals) {
            data.push_back({
                std::to_string(signal.timestamp_ms),
                std::to_string(signal.bar_index),
                signal.symbol,
                std::to_string(signal.probability),
                std::to_string(signal.confidence),
                signal.strategy_name,
                signal.strategy_version
            });
        }
        return utils::write_csv(output_path, data);
    }
    
    return false;
}

SignalOutput StrategyComponent::generate_signal(const Bar& bar, int bar_index) {
    // Default implementation - override in derived classes
    SignalOutput signal;
    signal.timestamp_ms = bar.timestamp_ms;
    signal.bar_index = bar_index;
    signal.symbol = bar.symbol;
    signal.probability = 0.5;
    signal.confidence = 0.5;
    signal.metadata["warmup_complete"] = warmup_complete_ ? "true" : "false";
    return signal;
}

void StrategyComponent::update_indicators(const Bar& bar) {
    historical_bars_.push_back(bar);
    if (historical_bars_.size() > config_.warmup_bars) {
        historical_bars_.erase(historical_bars_.begin());
    }
    
    // Update moving averages, volatility, etc.
    // This is a simplified example
    if (historical_bars_.size() >= 20) {
        double sum = 0;
        for (size_t i = historical_bars_.size() - 20; i < historical_bars_.size(); ++i) {
            sum += historical_bars_[i].close;
        }
        moving_average_.push_back(sum / 20.0);
    }
}

bool StrategyComponent::is_warmed_up() const {
    return bars_processed_ >= config_.warmup_bars;
}

// SigorStrategy implementation
SigorStrategy::SigorStrategy(const StrategyConfig& config) 
    : StrategyComponent(config) {
}

SignalOutput SigorStrategy::generate_signal(const Bar& bar, int bar_index) {
    SignalOutput signal;
    signal.timestamp_ms = bar.timestamp_ms;
    signal.bar_index = bar_index;
    signal.symbol = bar.symbol;
    signal.probability = calculate_buy_probability(bar);
    signal.confidence = calculate_confidence(bar);
    signal.metadata["warmup_complete"] = is_warmed_up() ? "true" : "false";
    signal.metadata["feature_count"] = "15";
    return signal;
}

void SigorStrategy::update_indicators(const Bar& bar) {
    StrategyComponent::update_indicators(bar);
    // Add Sigor-specific indicator updates
}

bool SigorStrategy::is_warmed_up() const {
    return StrategyComponent::is_warmed_up();
}

double SigorStrategy::calculate_buy_probability(const Bar& bar) {
    // Implement Sigor-specific logic
    // This is a placeholder implementation
    if (moving_average_.size() > 0) {
        double current_ma = moving_average_.back();
        double deviation = (bar.close - current_ma) / current_ma;
        return 0.5 + (deviation * 2.0); // Normalize to [0, 1]
    }
    return 0.5;
}

double SigorStrategy::calculate_confidence(const Bar& bar) {
    // Implement confidence calculation
    // This is a placeholder implementation
    if (volatility_.size() > 0) {
        double recent_vol = volatility_.back();
        return 1.0 / (1.0 + recent_vol); // Higher vol = lower confidence
    }
    return 0.5;
}

} // namespace sentio
```

## 4. Backend Component

### include/backend/backend_component.h
```cpp
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <sqlite3.h>
#include "common/types.h"
#include "strategy/signal_output.h"

namespace sentio {

class BackendComponent {
public:
    struct TradeOrder {
        int64_t timestamp_ms;
        int bar_index;
        std::string symbol;
        TradeAction action;
        double quantity;
        double price;
        double trade_value;
        double fees;
        
        PortfolioState before_state;
        PortfolioState after_state;
        
        double signal_probability;
        double signal_confidence;
        
        std::string execution_reason;
        std::string rejection_reason;
        bool conflict_check_passed;
        
        std::string to_sql_values() const;
    };
    
    struct BackendConfig {
        double starting_capital = 100000.0;
        double max_position_size = 0.25;
        bool enable_conflict_prevention = true;
        CostModel cost_model = CostModel::ALPACA;
        std::map<std::string, double> strategy_thresholds;
    };
    
    BackendComponent(const BackendConfig& config);
    ~BackendComponent();
    
    // Main processing function
    std::vector<TradeOrder> process_signals(
        const std::string& signal_file_path,
        const std::string& market_data_path,
        const BackendConfig& config
    );
    
    // Export trades to database
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
```

### include/backend/portfolio_manager.h
```cpp
#pragma once

#include <map>
#include <memory>
#include "common/types.h"

namespace sentio {

class PortfolioManager {
public:
    PortfolioManager(double starting_capital);
    
    // Portfolio operations
    bool can_buy(const std::string& symbol, double quantity, double price);
    bool can_sell(const std::string& symbol, double quantity);
    
    void execute_buy(const std::string& symbol, double quantity, double price, double fees);
    void execute_sell(const std::string& symbol, double quantity, double price, double fees);
    
    // State management
    PortfolioState get_state() const;
    void update_market_prices(const std::map<std::string, double>& prices);
    
    // Metrics
    double get_cash_balance() const { return cash_balance_; }
    double get_total_equity() const;
    double get_unrealized_pnl() const;
    double get_realized_pnl() const { return realized_pnl_; }
    
    // Position queries
    bool has_position(const std::string& symbol) const;
    Position get_position(const std::string& symbol) const;
    std::map<std::string, Position> get_all_positions() const { return positions_; }
    
private:
    double cash_balance_;
    double realized_pnl_;
    std::map<std::string, Position> positions_;
    
    void update_position(const std::string& symbol, double quantity, double price);
    void close_position(const std::string& symbol, double price);
};

// Static Position Manager for conflict prevention
class StaticPositionManager {
public:
    enum Direction {
        NEUTRAL = 0,
        LONG = 1,
        SHORT = -1
    };
    
    StaticPositionManager();
    
    // Conflict checking
    bool would_cause_conflict(const std::string& symbol, TradeAction action);
    void update_direction(Direction new_direction);
    
    // Position tracking
    void add_position(const std::string& symbol);
    void remove_position(const std::string& symbol);
    
    Direction get_direction() const { return current_direction_; }
    int get_position_count() const { return position_count_; }
    
private:
    Direction current_direction_;
    int position_count_;
    std::map<std::string, int> symbol_positions_;
    
    bool is_inverse_etf(const std::string& symbol) const;
};

} // namespace sentio
```

### src/backend/backend_component.cpp
```cpp
#include "backend/backend_component.h"
#include "backend/portfolio_manager.h"
#include "common/utils.h"
#include <fstream>
#include <iostream>
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
    const BackendConfig& config) {
    
    std::vector<TradeOrder> trades;
    
    // Read signals
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
                portfolio_manager_->execute_buy(
                    order.symbol, order.quantity, order.price, order.fees);
            } else if (order.action == TradeAction::SELL) {
                portfolio_manager_->execute_sell(
                    order.symbol, order.quantity, order.price, order.fees);
            }
            
            // Update position manager
            if (order.action == TradeAction::BUY) {
                position_manager_->add_position(order.symbol);
                position_manager_->update_direction(
                    StaticPositionManager::Direction::LONG);
            } else if (order.action == TradeAction::SELL) {
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
    double buy_threshold = config_.strategy_thresholds["buy_threshold"];
    double sell_threshold = config_.strategy_thresholds["sell_threshold"];
    
    if (signal.probability > buy_threshold && signal.confidence > 0.5) {
        // Buy signal
        if (!portfolio_manager_->has_position(signal.symbol)) {
            double available_capital = portfolio_manager_->get_cash_balance();
            double position_size = calculate_position_size(
                signal.probability, available_capital);
            
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
        case CostModel::ZERO:
            return 0.0;
        case CostModel::FIXED:
            return 1.0; // $1 per trade
        case CostModel::PERCENTAGE:
            return trade_value * 0.001; // 0.1%
        case CostModel::ALPACA:
            // Alpaca's fee structure
            return 0.0; // Alpaca has zero commission
        default:
            return 0.0;
    }
}

double BackendComponent::calculate_position_size(
    double signal_probability, double available_capital) {
    
    // Kelly Criterion inspired sizing
    double confidence_factor = (signal_probability - 0.5) * 2.0;
    double base_size = available_capital * config_.max_position_size;
    return base_size * std::min(1.0, std::max(0.0, confidence_factor));
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
    
    // Insert run information
    std::string run_sql = "INSERT INTO runs (run_id, strategy_name, dataset_path, "
                         "config_hash, started_at, status) VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_, run_sql.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, "backend", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, "signals", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, utils::calculate_sha256(run_id).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, utils::current_timestamp_str().c_str(), -1, SQLITE_TRANSIENT);
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
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
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
    
    const char* create_portfolio_table = R"(
        CREATE TABLE IF NOT EXISTS portfolio_snapshots (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id TEXT NOT NULL,
            timestamp_ms INTEGER NOT NULL,
            bar_index INTEGER NOT NULL,
            cash_balance REAL NOT NULL,
            total_equity REAL NOT NULL,
            unrealized_pnl REAL NOT NULL,
            realized_pnl REAL NOT NULL,
            position_count INTEGER NOT NULL,
            positions TEXT NOT NULL,
            spm_direction INTEGER NOT NULL,
            spm_position_count INTEGER NOT NULL,
            conflicts_detected BOOLEAN NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (run_id) REFERENCES runs(run_id)
        )
    )";
    
    // Execute table creation
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, create_runs_table, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    
    rc = sqlite3_exec(db_, create_trades_table, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    
    rc = sqlite3_exec(db_, create_portfolio_table, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    
    // Create indexes
    sqlite3_exec(db_, "CREATE INDEX IF NOT EXISTS idx_trades_run_timestamp ON trades(run_id, timestamp_ms)", 
                 nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "CREATE INDEX IF NOT EXISTS idx_portfolio_run_timestamp ON portfolio_snapshots(run_id, timestamp_ms)", 
                 nullptr, nullptr, nullptr);
    
    return true;
}

} // namespace sentio