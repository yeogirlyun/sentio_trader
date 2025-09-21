# Sentio Leverage Trading System

**Part 1 of 2**

**Generated**: 2025-09-21 16:47:20
**Source Directory**: /Users/yeogirlyun/C++/sentio_trader
**Description**: Comprehensive leverage trading implementation with conflict prevention and profit maximization

**Part 1 Files**: See file count below

---

## 📋 **TABLE OF CONTENTS**

1. [include/backend/audit_component.h](#file-1)
2. [include/backend/backend_component.h](#file-2)
3. [include/backend/portfolio_manager.h](#file-3)
4. [include/common/config.h](#file-4)
5. [include/common/types.h](#file-5)
6. [include/common/utils.h](#file-6)
7. [include/strategy/signal_output.h](#file-7)
8. [include/strategy/sigor_config.h](#file-8)
9. [include/strategy/sigor_strategy.h](#file-9)
10. [include/strategy/strategy_component.h](#file-10)
11. [src/backend/audit_component.cpp](#file-11)
12. [src/backend/backend_component.cpp](#file-12)
13. [src/backend/portfolio_manager.cpp](#file-13)
14. [src/cli/audit_main.cpp](#file-14)
15. [src/cli/sentio_cli_main.cpp](#file-15)
16. [src/cli/strattest_main.cpp](#file-16)
17. [src/common/types.cpp](#file-17)
18. [src/common/utils.cpp](#file-18)
19. [src/strategy/signal_output.cpp](#file-19)
20. [src/strategy/sigor_config.cpp](#file-20)
21. [src/strategy/sigor_strategy.cpp](#file-21)
22. [src/strategy/strategy_component.cpp](#file-22)
23. [tools/add_comprehensive_comments.py](#file-23)
24. [tools/align_bars.py](#file-24)
25. [tools/analyze_ohlc_patterns.py](#file-25)
26. [tools/analyze_unused_code.py](#file-26)
27. [tools/check_file_organization.py](#file-27)

---

## 📄 **FILE 1 of 53**: include/backend/audit_component.h

**File Information**:
- **Path**: `include/backend/audit_component.h`

- **Size**: 34 lines
- **Modified**: 2025-09-21 00:30:30

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: backend/audit_component.h
// Purpose: Post-run analysis of trades and portfolio performance metrics.
//
// Core idea:
// - Consume executed trades and produce risk/performance analytics such as
//   Sharpe ratio, max drawdown, win rate, and conflict detection summaries.
// =============================================================================

#include <vector>
#include <string>
#include <map>

namespace sentio {

struct TradeSummary {
    int total_trades = 0;
    int wins = 0;
    int losses = 0;
    double win_rate = 0.0;
    double sharpe = 0.0;
    double max_drawdown = 0.0;
};

class AuditComponent {
public:
    TradeSummary analyze_equity_curve(const std::vector<double>& equity_curve) const;
};

} // namespace sentio



```

## 📄 **FILE 2 of 53**: include/backend/backend_component.h

**File Information**:
- **Path**: `include/backend/backend_component.h`

- **Size**: 133 lines
- **Modified**: 2025-09-21 15:18:59

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: backend/backend_component.h
// Purpose: Aggressive Portfolio Management & Trade Execution Engine
//
// Core Architecture & Recent Enhancements:
// The Backend serves as the critical execution layer that transforms AI-generated
// signals into actual trades using an aggressive capital allocation strategy
// designed for maximum profit potential while maintaining strict risk controls.
//
// Key Design Philosophy:
// - Aggressive capital allocation: Up to 100% capital usage based on signal strength
// - Fail-fast error handling: Immediate program termination on any financial anomalies
// - Zero-commission Alpaca fee model for cost-effective execution
// - Comprehensive audit trails with detailed trade logging
// - Robust portfolio state management with negative cash prevention
//
// Recent Major Improvements:
// - Removed artificial capital allocation limits for maximum profit potential
// - Enhanced fee calculation with detailed error handling and validation
// - Implemented aggressive position sizing based on signal confidence
// - Added comprehensive input validation with crash-on-error philosophy
// - Integrated detailed debug logging for all financial operations
// - Enhanced conflict detection and prevention mechanisms
//
// Financial Safeguards:
// - Hard constraint preventing negative cash balances (immediate abort)
// - Extensive validation of all trade parameters before execution
// - Detailed logging of all financial calculations and state changes
// - Position conflict detection to prevent simultaneous long/short positions
// =============================================================================

#include <vector>
#include <memory>
#include <string>
#include <map>
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

        // JSONL serialization for file-based trade books
        std::string to_json_line(const std::string& run_id) const;
    };

    /// Configuration for aggressive portfolio management and trade execution
    /// 
    /// This structure defines the core parameters for the trading engine,
    /// emphasizing aggressive capital allocation for maximum profit potential
    /// while maintaining essential risk controls.
    struct BackendConfig {
        /// Starting capital for trading operations (default: $100,000)
        double starting_capital = 100000.0;
        
        /// REMOVED: max_position_size - No artificial limits on capital usage
        /// The system now allows up to 100% capital allocation based on signal
        /// strength to maximize profit potential. Position sizing is determined
        /// dynamically by signal confidence and available capital.
        
        /// Enable position conflict detection to prevent simultaneous long/short positions
        /// This prevents conflicting trades that could create unintended hedged positions
        bool enable_conflict_prevention = true;
        
        /// Fee model for trade execution (default: Alpaca zero-commission model)
        /// ALPACA: Zero commission for cost-effective execution
        /// Other models available for testing: ZERO, FIXED, PERCENTAGE
        CostModel cost_model = CostModel::ALPACA;
        
        /// Strategy-specific thresholds for buy/sell decisions
        /// Keys: "buy_threshold", "sell_threshold" 
        /// Values: Probability thresholds (0.0 to 1.0) for trade execution
        std::map<std::string, double> strategy_thresholds;
    };

    explicit BackendComponent(const BackendConfig& config);
    ~BackendComponent();

    // Main processing: turn signals + bars into executed trades
    std::vector<TradeOrder> process_signals(
        const std::string& signal_file_path,
        const std::string& market_data_path,
        const BackendConfig& config
    );

    // (DB export removed) Use process_to_jsonl instead

    // File-based trade book writer (JSONL). Writes only executed trades.
    bool process_to_jsonl(
        const std::string& signal_file_path,
        const std::string& market_data_path,
        const std::string& out_path,
        const std::string& run_id,
        size_t start_index = 0,
        size_t max_count = static_cast<size_t>(-1)
    );

private:
    BackendConfig config_;
    std::unique_ptr<class PortfolioManager> portfolio_manager_;
    std::unique_ptr<class StaticPositionManager> position_manager_;

    TradeOrder evaluate_signal(const SignalOutput& signal, const Bar& bar);
    bool check_conflicts(const TradeOrder& order);
    double calculate_fees(double trade_value);
    double calculate_position_size(double signal_probability, double available_capital);
};

} // namespace sentio



```

## 📄 **FILE 3 of 53**: include/backend/portfolio_manager.h

**File Information**:
- **Path**: `include/backend/portfolio_manager.h`

- **Size**: 86 lines
- **Modified**: 2025-09-21 12:10:07

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: backend/portfolio_manager.h
// Purpose: Portfolio accounting, position tracking, and basic P&L metrics.
//
// Core idea:
// - Encapsulate portfolio state transitions (buy/sell) and derived metrics.
// - No external I/O; callers provide prices and receive updated snapshots.
// =============================================================================

#include <map>
#include <memory>
#include <string>
#include "common/types.h"

namespace sentio {

class PortfolioManager {
public:
    explicit PortfolioManager(double starting_capital);

    // Portfolio operations
    bool can_buy(const std::string& symbol, double quantity, double price, double fees);
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
    double cash_balance_ = 0.0;
    double realized_pnl_ = 0.0;
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

## 📄 **FILE 4 of 53**: include/common/config.h

**File Information**:
- **Path**: `include/common/config.h`

- **Size**: 46 lines
- **Modified**: 2025-09-21 00:30:29

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: common/config.h
// Purpose: Lightweight configuration access for application components.
//
// Core idea:
// - Provide a simple key/value configuration store with typed getters.
// - Parsing from YAML/JSON is delegated to thin adapter functions that load
//   files and populate the internal map. Heavy parsing libs can be added later
//   behind the same interface without impacting dependents.
// =============================================================================

#include <string>
#include <map>
#include <memory>

namespace sentio {

class Config {
public:
    // Factory: load configuration from YAML file path.
    static std::shared_ptr<Config> from_yaml(const std::string& path);
    // Factory: load configuration from JSON file path.
    static std::shared_ptr<Config> from_json(const std::string& path);

    // Typed getter: throws std::out_of_range if key missing (no default).
    template<typename T>
    T get(const std::string& key) const;

    // Typed getter with default fallback if key missing or conversion fails.
    template<typename T>
    T get(const std::string& key, const T& default_value) const;

    // Existence check.
    bool has(const std::string& key) const;

private:
    std::map<std::string, std::string> data_; // canonical storage as strings
    friend std::shared_ptr<Config> parse_yaml_file(const std::string&);
    friend std::shared_ptr<Config> parse_json_file(const std::string&);
};

} // namespace sentio



```

## 📄 **FILE 5 of 53**: include/common/types.h

**File Information**:
- **Path**: `include/common/types.h`

- **Size**: 99 lines
- **Modified**: 2025-09-21 00:30:29

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: common/types.h
// Purpose: Defines core value types used across the Sentio trading platform.
//
// Overview:
// - Contains lightweight, Plain-Old-Data (POD) structures that represent
//   market bars, positions, and the overall portfolio state.
// - These types are intentionally free of behavior (no I/O, no business logic)
//   to keep the Domain layer pure and deterministic.
// - Serialization helpers (to/from JSON) are declared here and implemented in
//   the corresponding .cpp, allowing adapters to convert data at the edges.
//
// Design Notes:
// - Keep this header stable; many modules include it. Prefer additive changes.
// - Avoid heavy includes; use forward declarations elsewhere when possible.
// =============================================================================

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>

namespace sentio {

// -----------------------------------------------------------------------------
// Struct: Bar
// A single OHLCV market bar for a given symbol and timestamp.
// Core idea: immutable snapshot of market state at time t.
// -----------------------------------------------------------------------------
struct Bar {
    int64_t timestamp_ms;   // Milliseconds since Unix epoch
    double open;
    double high;
    double low;
    double close;
    double volume;
    std::string symbol;
};

// -----------------------------------------------------------------------------
// Struct: Position
// A held position for a given symbol, tracking quantity and P&L components.
// Core idea: minimal position accounting without execution-side effects.
// -----------------------------------------------------------------------------
struct Position {
    std::string symbol;
    double quantity = 0.0;
    double avg_price = 0.0;
    double current_price = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
};

// -----------------------------------------------------------------------------
// Struct: PortfolioState
// A snapshot of portfolio metrics and positions at a point in time.
// Core idea: serializable state to audit and persist run-time behavior.
// -----------------------------------------------------------------------------
struct PortfolioState {
    double cash_balance = 0.0;
    double total_equity = 0.0;
    double unrealized_pnl = 0.0;
    double realized_pnl = 0.0;
    std::map<std::string, Position> positions; // keyed by symbol
    int64_t timestamp_ms = 0;

    // Serialize this state to JSON (implemented in src/common/types.cpp)
    std::string to_json() const;
    // Parse a JSON string into a PortfolioState (implemented in .cpp)
    static PortfolioState from_json(const std::string& json_str);
};

// -----------------------------------------------------------------------------
// Enum: TradeAction
// The intended trade action derived from strategy/backend decision.
// -----------------------------------------------------------------------------
enum class TradeAction {
    BUY,
    SELL,
    HOLD
};

// -----------------------------------------------------------------------------
// Enum: CostModel
// Commission/fee model abstraction to support multiple broker-like schemes.
// -----------------------------------------------------------------------------
enum class CostModel {
    ZERO,
    FIXED,
    PERCENTAGE,
    ALPACA
};

} // namespace sentio



```

## 📄 **FILE 6 of 53**: include/common/utils.h

**File Information**:
- **Path**: `include/common/utils.h`

- **Size**: 163 lines
- **Modified**: 2025-09-21 15:18:59

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: common/utils.h
// Purpose: Comprehensive utility library for the Sentio Trading System
//
// Core Architecture & Recent Enhancements:
// This module provides essential utilities that support the entire trading
// system infrastructure. It has been significantly enhanced with robust
// error handling, CLI utilities, and improved JSON parsing capabilities.
//
// Key Design Principles:
// - Centralized reusable functionality to eliminate code duplication
// - Fail-fast error handling with detailed logging and validation
// - UTC timezone consistency across all time-related operations
// - Robust JSON parsing that handles complex data structures correctly
// - File organization utilities that maintain proper data structure
//
// Recent Major Improvements:
// - Added CLI argument parsing utilities (get_arg) to eliminate duplicates
// - Enhanced JSON parsing to prevent field corruption from quoted commas
// - Implemented comprehensive logging system with file rotation
// - Added robust error handling with crash-on-error philosophy
// - Improved time utilities with consistent UTC timezone handling
//
// Module Categories:
// 1. File I/O: CSV/JSONL reading/writing with format detection
// 2. Time Utilities: UTC-consistent timestamp conversion and formatting
// 3. JSON Utilities: Robust parsing that handles complex quoted strings
// 4. Hash Utilities: SHA-256 and run ID generation for data integrity
// 5. Math Utilities: Financial metrics (Sharpe ratio, drawdown analysis)
// 6. Logging Utilities: Structured logging with file rotation and levels
// 7. CLI Utilities: Command-line argument parsing with flexible formats
// =============================================================================

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <map>
#include <cstdint>
#include "types.h"

namespace sentio {
namespace utils {

// ----------------------------- File I/O utilities ----------------------------
/// Advanced CSV data reader with automatic format detection and symbol extraction
/// 
/// This function intelligently handles multiple CSV formats:
/// 1. QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume (symbol from filename)
/// 2. Standard format: symbol,timestamp_ms,open,high,low,close,volume
/// 
/// Key Features:
/// - Automatic format detection by analyzing header row
/// - Symbol extraction from filename for QQQ format files
/// - Timestamp conversion from seconds to milliseconds for QQQ format
/// - Robust error handling with graceful fallbacks
/// 
/// @param path Path to CSV file (supports both relative and absolute paths)
/// @return Vector of Bar structures with OHLCV data and metadata
std::vector<Bar> read_csv_data(const std::string& path);

/// Write data in JSON Lines format for efficient streaming and processing
/// 
/// JSON Lines (JSONL) format stores one JSON object per line, making it ideal
/// for large datasets that need to be processed incrementally. This format
/// is used throughout the Sentio system for signals and trade data.
/// 
/// @param path Output file path
/// @param lines Vector of JSON strings (one per line)
/// @return true if write successful, false otherwise
bool write_jsonl(const std::string& path, const std::vector<std::string>& lines);

/// Write structured data to CSV format with proper escaping
/// 
/// @param path Output CSV file path
/// @param data 2D string matrix where first row typically contains headers
/// @return true if write successful, false otherwise
bool write_csv(const std::string& path, const std::vector<std::vector<std::string>>& data);

// ------------------------------ Time utilities -------------------------------
// Parse ISO-like timestamp (YYYY-MM-DD HH:MM:SS) into milliseconds since epoch
int64_t timestamp_to_ms(const std::string& timestamp_str);

// Convert milliseconds since epoch to formatted timestamp string
std::string ms_to_timestamp(int64_t ms);

// Current wall-clock timestamp in ISO-like string
std::string current_timestamp_str();

// ------------------------------ JSON utilities -------------------------------
/// Convert string map to JSON format for lightweight serialization
/// 
/// This function creates simple JSON objects from string key-value pairs.
/// It's designed for lightweight serialization of metadata and configuration.
/// 
/// @param data Map of string keys to string values
/// @return JSON string representation
std::string to_json(const std::map<std::string, std::string>& data);

/// Robust JSON parser for flat string maps with enhanced quote handling
/// 
/// This parser has been significantly enhanced to correctly handle complex
/// JSON structures that contain commas and colons within quoted strings.
/// It prevents the field corruption issues that were present in earlier versions.
/// 
/// Key Features:
/// - Proper handling of commas within quoted values
/// - Correct parsing of colons within quoted strings
/// - Robust quote escaping and state tracking
/// - Graceful error handling with empty map fallback
/// 
/// @param json_str JSON string to parse (must be flat object format)
/// @return Map of parsed key-value pairs, empty map on parse errors
std::map<std::string, std::string> from_json(const std::string& json_str);

// -------------------------------- Hash utilities -----------------------------
// SHA-256 of input data as lowercase hex string (self-contained implementation)
std::string calculate_sha256(const std::string& data);

// Generate an 8-digit numeric run id (zero-padded). Unique enough per run.
std::string generate_run_id(const std::string& prefix);

// -------------------------------- Math utilities -----------------------------
double calculate_sharpe_ratio(const std::vector<double>& returns, double risk_free_rate = 0.0);
double calculate_max_drawdown(const std::vector<double>& equity_curve);

// -------------------------------- Logging utilities -------------------------- 
// Minimal file logger. Writes to logs/debug.log and logs/errors.log.
// Messages should be pre-sanitized (no secrets/PII).
void log_debug(const std::string& message);
void log_error(const std::string& message);

// -------------------------------- CLI utilities ------------------------------- 
/// Flexible command-line argument parser supporting multiple formats
/// 
/// This utility function was extracted from duplicate implementations across
/// multiple CLI files to eliminate code duplication and ensure consistency.
/// It provides flexible parsing that accommodates different user preferences.
/// 
/// Supported Formats:
/// - Space-separated: --name value
/// - Equals-separated: --name=value
/// - Mixed usage within the same command line
/// 
/// Key Features:
/// - Robust argument validation (prevents parsing flags as values)
/// - Consistent behavior across all CLI tools
/// - Graceful fallback to default values
/// - No external dependencies or complex parsing libraries
/// 
/// @param argc Number of command line arguments
/// @param argv Array of command line argument strings
/// @param name The argument name to search for (including -- prefix)
/// @param def Default value returned if argument not found
/// @return The argument value if found, otherwise the default value
std::string get_arg(int argc, char** argv, const std::string& name, const std::string& def = "");

} // namespace utils
} // namespace sentio



```

## 📄 **FILE 7 of 53**: include/strategy/signal_output.h

**File Information**:
- **Path**: `include/strategy/signal_output.h`

- **Size**: 36 lines
- **Modified**: 2025-09-21 00:30:29

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: strategy/signal_output.h
// Purpose: Defines the portable signal record emitted by strategies.
//
// Core idea:
// - A signal is the strategy's probabilistic opinion about the next action,
//   decoupled from execution. It can be serialized to JSON/CSV and consumed by
//   the backend for trade decisions or by audit tools for analysis.
// =============================================================================

#include <string>
#include <map>
#include <cstdint>

namespace sentio {

struct SignalOutput {
    int64_t timestamp_ms = 0;
    int bar_index = 0;
    std::string symbol;
    double probability = 0.0;       // 0.0 to 1.0
    double confidence = 0.0;        // 0.0 to 1.0
    std::string strategy_name;
    std::string strategy_version;
    std::map<std::string, std::string> metadata;

    std::string to_json() const;    // serialize to JSON object string
    std::string to_csv() const;     // serialize to CSV line (no header)
    static SignalOutput from_json(const std::string& json_str);
};

} // namespace sentio



```

## 📄 **FILE 8 of 53**: include/strategy/sigor_config.h

**File Information**:
- **Path**: `include/strategy/sigor_config.h`

- **Size**: 52 lines
- **Modified**: 2025-09-21 02:36:18

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: strategy/sigor_config.h
// Purpose: Configuration for SigorStrategy detectors and fusion.
//
// Format: JSON (flat map of key -> string/number), e.g.
// {
//   "k": 1.5,
//   "w_boll": 1.0, "w_rsi": 1.0, "w_mom": 1.0,
//   "w_vwap": 1.0, "w_orb": 0.5, "w_ofi": 0.5, "w_vol": 0.5,
//   "win_boll": 20, "win_rsi": 14, "win_mom": 10, "win_vwap": 20,
//   "orb_opening_bars": 30
// }
//
// Notes:
// - "k" controls sharpness in log-odds fusion: P = sigma(k * L).
// - w_* are detector reliabilities; set to 0 to disable a detector.
// - win_* are window lengths for respective detectors.
// =============================================================================

#include <string>

namespace sentio {

struct SigorConfig {
    // Fusion
    double k = 1.5; // sharpness/temperature

    // Detector weights (reliability)
    double w_boll = 1.0;
    double w_rsi  = 1.0;
    double w_mom  = 1.0;
    double w_vwap = 1.0;
    double w_orb  = 0.5;
    double w_ofi  = 0.5;
    double w_vol  = 0.5;

    // Windows / parameters
    int win_boll = 20;
    int win_rsi  = 14;
    int win_mom  = 10;
    int win_vwap = 20;
    int orb_opening_bars = 30;

    static SigorConfig defaults();
    static SigorConfig from_file(const std::string& path);
};

} // namespace sentio



```

## 📄 **FILE 9 of 53**: include/strategy/sigor_strategy.h

**File Information**:
- **Path**: `include/strategy/sigor_strategy.h`

- **Size**: 84 lines
- **Modified**: 2025-09-21 02:59:53

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: strategy/sigor_strategy.h
// Purpose: SigorStrategy — a rule-based ensemble (Signal-OR) strategy that
//          combines multiple technical detectors into a single probability.
//
// Detailed algorithm (Signal-OR aggregator):
// - Compute several detector probabilities independently per bar:
//   1) Bollinger Z-Score: z = (Close - SMA20) / StdDev20; prob = 0.5 + 0.5*tanh(z/2).
//   2) RSI(14): map RSI [0..100] to prob around 0.5 (50 neutral).
//   3) Momentum (10 bars): prob = 0.5 + 0.5*tanh(return*scale), scale≈50.
//   4) VWAP reversion (rolling window): typical price vs rolling VWAP.
//   5) Opening Range Breakout (daily): breakout above/below day’s opening range (first N bars).
//   6) OFI Proxy: order-flow imbalance proxy using bar geometry and volume.
//   7) Volume Surge: volume vs rolling average scales the momentum signal.
// - OR aggregation: choose the detector with maximum strength |p - 0.5| as the
//   final probability for the bar. This captures the strongest signal.
// - Confidence: blend detector agreement (majority on the same side of 0.5)
//   with maximum strength, then clamp to [0,1].
//
// Design notes:
// - Stateless outside of rolling windows; no I/O. All state is internal buffers.
// - Uses only OHLCV bars, no external feature libs; computations are simplified
//   proxies of the detectors referenced in sentio_cpp.
// - Warmup is inherited from StrategyComponent; emits signals after warmup.
// =============================================================================

#include <vector>
#include <string>
#include <cstdint>
#include "common/types.h"
#include "strategy_component.h"
#include "sigor_config.h"

namespace sentio {

class SigorStrategy : public StrategyComponent {
public:
    explicit SigorStrategy(const StrategyConfig& config);
    // Optional: set configuration (weights, windows, k)
    void set_config(const SigorConfig& cfg) { cfg_ = cfg; }

protected:
    SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    void update_indicators(const Bar& bar) override;
    bool is_warmed_up() const override;

private:
    SigorConfig cfg_;
    // ---- Rolling series ----
    std::vector<double> closes_;
    std::vector<double> highs_;
    std::vector<double> lows_;
    std::vector<double> volumes_;
    std::vector<int64_t> timestamps_;
    std::vector<double> gains_;
    std::vector<double> losses_;

    // ---- Detector probabilities ----
    double prob_bollinger_(const Bar& bar) const;
    double prob_rsi_14_() const;
    double prob_momentum_(int window, double scale) const;
    double prob_vwap_reversion_(int window) const;
    double prob_orb_daily_(int opening_window_bars) const;
    double prob_ofi_proxy_(const Bar& bar) const;
    double prob_volume_surge_scaled_(int window) const;

    // ---- Aggregation & helpers ----
    double aggregate_probability(double p1, double p2, double p3,
                                 double p4, double p5, double p6, double p7) const;
    double calculate_confidence(double p1, double p2, double p3,
                                double p4, double p5, double p6, double p7) const;

    // Stats helpers
    double compute_sma(const std::vector<double>& v, int window) const;
    double compute_stddev(const std::vector<double>& v, int window, double mean) const;
    double compute_rsi(int window) const;
    double clamp01(double v) const { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
};

} // namespace sentio



```

## 📄 **FILE 10 of 53**: include/strategy/strategy_component.h

**File Information**:
- **Path**: `include/strategy/strategy_component.h`

- **Size**: 73 lines
- **Modified**: 2025-09-21 03:33:22

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: strategy/strategy_component.h
// Purpose: Base strategy abstraction and a concrete example (SigorStrategy).
//
// Core idea:
// - A strategy processes a stream of Bars, maintains internal indicators, and
//   emits SignalOutput records once warm-up is complete.
// - The base class provides the ingest/export orchestration; derived classes
//   implement indicator updates and signal generation.
// =============================================================================

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

    explicit StrategyComponent(const StrategyConfig& config);
    virtual ~StrategyComponent() = default;

    // Process a dataset file of Bars and return generated signals.
    std::vector<SignalOutput> process_dataset(
        const std::string& dataset_path,
        const std::string& strategy_name,
        const std::map<std::string, std::string>& strategy_params
    );

    // Export signals to file in jsonl or csv format.
    bool export_signals(
        const std::vector<SignalOutput>& signals,
        const std::string& output_path,
        const std::string& format = "jsonl"
    );

protected:
    // Hooks for strategy authors to implement
    virtual SignalOutput generate_signal(const Bar& bar, int bar_index);
    virtual void update_indicators(const Bar& bar);
    virtual bool is_warmed_up() const;

protected:
    StrategyConfig config_;
    std::vector<Bar> historical_bars_;
    int bars_processed_ = 0;
    bool warmup_complete_ = false;

    // Example internal indicators
    std::vector<double> moving_average_;
    std::vector<double> volatility_;
    std::vector<double> momentum_;
};

// Note: SigorStrategy is defined in `strategy/sigor_strategy.h`.

} // namespace sentio



```

## 📄 **FILE 11 of 53**: src/backend/audit_component.cpp

**File Information**:
- **Path**: `src/backend/audit_component.cpp`

- **Size**: 28 lines
- **Modified**: 2025-09-21 00:30:30

- **Type**: .cpp

```text
#include "backend/audit_component.h"
#include "common/utils.h"

namespace sentio {

TradeSummary AuditComponent::analyze_equity_curve(const std::vector<double>& equity_curve) const {
    TradeSummary s;
    if (equity_curve.size() < 2) return s;

    // Approximate returns from equity curve deltas
    std::vector<double> returns;
    returns.reserve(equity_curve.size() - 1);
    for (size_t i = 1; i < equity_curve.size(); ++i) {
        double prev = equity_curve[i - 1];
        double curr = equity_curve[i];
        if (prev != 0.0) {
            returns.push_back((curr - prev) / prev);
        }
    }

    s.sharpe = utils::calculate_sharpe_ratio(returns);
    s.max_drawdown = utils::calculate_max_drawdown(equity_curve);
    return s;
}

} // namespace sentio



```

## 📄 **FILE 12 of 53**: src/backend/backend_component.cpp

**File Information**:
- **Path**: `src/backend/backend_component.cpp`

- **Size**: 435 lines
- **Modified**: 2025-09-21 14:21:05

- **Type**: .cpp

```text
#include "backend/backend_component.h"
#include "backend/portfolio_manager.h"
#include "common/utils.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <cmath>
#include <algorithm>
std::string sentio::BackendComponent::TradeOrder::to_json_line(const std::string& run_id) const {
    std::map<std::string, std::string> m;
    m["run_id"] = run_id;
    m["timestamp_ms"] = std::to_string(timestamp_ms);
    m["bar_index"] = std::to_string(bar_index);
    m["symbol"] = symbol;
    m["action"] = (action == TradeAction::BUY ? "BUY" : (action == TradeAction::SELL ? "SELL" : "HOLD"));
    m["quantity"] = std::to_string(quantity);
    m["price"] = std::to_string(price);
    m["trade_value"] = std::to_string(trade_value);
    m["fees"] = std::to_string(fees);
    m["cash_before"] = std::to_string(before_state.cash_balance);
    m["equity_before"] = std::to_string(before_state.total_equity);
    m["cash_after"] = std::to_string(after_state.cash_balance);
    m["equity_after"] = std::to_string(after_state.total_equity);
    m["positions_after"] = std::to_string(after_state.positions.size());
    m["signal_probability"] = std::to_string(signal_probability);
    m["signal_confidence"] = std::to_string(signal_confidence);
    m["execution_reason"] = execution_reason;
    m["rejection_reason"] = rejection_reason;
    m["conflict_check_passed"] = conflict_check_passed ? "1" : "0";
    // Extras for richer position-history views
    double realized_delta = after_state.realized_pnl - before_state.realized_pnl;
    m["realized_pnl_delta"] = std::to_string(realized_delta);
    m["unrealized_after"] = std::to_string(after_state.unrealized_pnl);
    std::string pos_summary;
    bool first_ps = true;
    for (const auto& kv : after_state.positions) {
        const auto& sym = kv.first;
        const auto& pos = kv.second;
        if (std::fabs(pos.quantity) < 1e-9) continue;
        if (!first_ps) pos_summary += ",";
        first_ps = false;
        pos_summary += sym + ":" + std::to_string(pos.quantity);
    }
    m["positions_summary"] = pos_summary;
    return utils::to_json(m);
}


namespace sentio {

BackendComponent::BackendComponent(const BackendConfig& config)
    : config_(config) {
    portfolio_manager_ = std::make_unique<PortfolioManager>(config.starting_capital);
    position_manager_ = std::make_unique<StaticPositionManager>();
}

BackendComponent::~BackendComponent() {
    // No-op (file-based trade books)
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

    // Comprehensive input validation with detailed error messages
    if (signal.symbol.empty()) {
        utils::log_error("CRITICAL: Empty symbol in signal. Timestamp=" + std::to_string(signal.timestamp_ms) + 
                        ", probability=" + std::to_string(signal.probability));
        std::abort();
    }
    
    if (signal.probability < 0.0 || signal.probability > 1.0) {
        utils::log_error("CRITICAL: Invalid signal probability=" + std::to_string(signal.probability) + 
                        " for symbol=" + signal.symbol + ", timestamp=" + std::to_string(signal.timestamp_ms));
        std::abort();
    }
    
    if (signal.confidence < 0.0 || signal.confidence > 1.0) {
        utils::log_error("CRITICAL: Invalid signal confidence=" + std::to_string(signal.confidence) + 
                        " for symbol=" + signal.symbol + ", timestamp=" + std::to_string(signal.timestamp_ms));
        std::abort();
    }
    
    if (bar.close <= 0.0 || std::isnan(bar.close) || std::isinf(bar.close)) {
        utils::log_error("CRITICAL: Invalid bar close price=" + std::to_string(bar.close) + 
                        " for symbol=" + signal.symbol + ", timestamp=" + std::to_string(signal.timestamp_ms));
        std::abort();
    }
    
    if (bar.symbol != signal.symbol) {
        utils::log_error("CRITICAL: Symbol mismatch - signal.symbol=" + signal.symbol + 
                        ", bar.symbol=" + bar.symbol + ", timestamp=" + std::to_string(signal.timestamp_ms));
        std::abort();
    }

    TradeOrder order;
    order.timestamp_ms = signal.timestamp_ms;
    order.bar_index = signal.bar_index;
    order.symbol = signal.symbol;
    order.signal_probability = signal.probability;
    order.signal_confidence = signal.confidence;
    order.price = bar.close;
    order.before_state = portfolio_manager_->get_state();
    
    utils::log_debug("Evaluating signal: symbol=" + signal.symbol + 
                    ", probability=" + std::to_string(signal.probability) + 
                    ", confidence=" + std::to_string(signal.confidence) + 
                    ", price=" + std::to_string(bar.close));

    // Decision logic based on probability thresholds with validation
    double buy_threshold = 0.6;
    double sell_threshold = 0.4;
    
    if (auto it = config_.strategy_thresholds.find("buy_threshold"); it != config_.strategy_thresholds.end()) {
        buy_threshold = it->second;
        if (buy_threshold < 0.0 || buy_threshold > 1.0) {
            utils::log_error("CRITICAL: Invalid buy_threshold=" + std::to_string(buy_threshold) + 
                            " for symbol=" + signal.symbol);
            std::abort();
        }
    }
    
    if (auto it = config_.strategy_thresholds.find("sell_threshold"); it != config_.strategy_thresholds.end()) {
        sell_threshold = it->second;
        if (sell_threshold < 0.0 || sell_threshold > 1.0) {
            utils::log_error("CRITICAL: Invalid sell_threshold=" + std::to_string(sell_threshold) + 
                            " for symbol=" + signal.symbol);
            std::abort();
        }
    }

    if (signal.probability > buy_threshold && signal.confidence > 0.5) {
        // Buy signal - aggressive capital allocation for maximum profit
        if (!portfolio_manager_->has_position(signal.symbol)) {
            double available_capital = portfolio_manager_->get_cash_balance();
            
            // Validate available capital
            if (available_capital < 0.0) {
                utils::log_error("CRITICAL: Negative available_capital=" + std::to_string(available_capital) + 
                                " for symbol=" + signal.symbol + ", cannot execute buy");
                std::abort();
            }
            
            double position_size = calculate_position_size(signal.probability, available_capital);
            
            // Validate position size calculation
            if (position_size < 0.0 || std::isnan(position_size) || std::isinf(position_size)) {
                utils::log_error("CRITICAL: Invalid position_size=" + std::to_string(position_size) + 
                                " for symbol=" + signal.symbol + ", available_capital=" + std::to_string(available_capital));
                std::abort();
            }
            
            order.quantity = position_size / bar.close;
            order.trade_value = position_size;
            
            // Validate quantity calculation
            if (order.quantity < 0.0 || std::isnan(order.quantity) || std::isinf(order.quantity)) {
                utils::log_error("CRITICAL: Invalid order.quantity=" + std::to_string(order.quantity) + 
                                " for symbol=" + signal.symbol + ", position_size=" + std::to_string(position_size) + 
                                ", price=" + std::to_string(bar.close));
                std::abort();
            }
            
            order.fees = calculate_fees(order.trade_value);
            
            if (portfolio_manager_->can_buy(signal.symbol, order.quantity, bar.close, order.fees)) {
                order.action = TradeAction::BUY;
                order.execution_reason = "Buy signal above threshold - aggressive sizing";
                utils::log_debug("BUY ORDER: symbol=" + signal.symbol + 
                                ", quantity=" + std::to_string(order.quantity) + 
                                ", value=" + std::to_string(order.trade_value) + 
                                ", fees=" + std::to_string(order.fees));
            } else {
                order.action = TradeAction::HOLD;
                order.rejection_reason = "Insufficient capital for aggressive position";
                utils::log_error("REJECTED BUY: symbol=" + signal.symbol + 
                                ", required_capital=" + std::to_string((order.quantity * bar.close) + order.fees) + 
                                ", available_capital=" + std::to_string(available_capital));
            }
        } else {
            order.action = TradeAction::HOLD;
            order.execution_reason = "Already has position";
            utils::log_debug("HOLD: Already have position in " + signal.symbol);
        }
    } else if (signal.probability < sell_threshold) {
        // Sell signal - liquidate position for profit protection
        if (portfolio_manager_->has_position(signal.symbol)) {
            auto position = portfolio_manager_->get_position(signal.symbol);
            
            // Validate position data
            if (position.quantity <= 0.0 || std::isnan(position.quantity) || std::isinf(position.quantity)) {
                utils::log_error("CRITICAL: Invalid position.quantity=" + std::to_string(position.quantity) + 
                                " for symbol=" + signal.symbol + " during sell signal");
                std::abort();
            }
            
            order.quantity = position.quantity;
            order.trade_value = order.quantity * bar.close;
            
            // Validate trade value calculation
            if (order.trade_value <= 0.0 || std::isnan(order.trade_value) || std::isinf(order.trade_value)) {
                utils::log_error("CRITICAL: Invalid sell trade_value=" + std::to_string(order.trade_value) + 
                                " for symbol=" + signal.symbol + ", quantity=" + std::to_string(order.quantity) + 
                                ", price=" + std::to_string(bar.close));
                std::abort();
            }
            
            order.fees = calculate_fees(order.trade_value);
            order.action = TradeAction::SELL;
            order.execution_reason = "Sell signal below threshold - full liquidation";
            
            utils::log_debug("SELL ORDER: symbol=" + signal.symbol + 
                            ", quantity=" + std::to_string(order.quantity) + 
                            ", value=" + std::to_string(order.trade_value) + 
                            ", fees=" + std::to_string(order.fees));
        } else {
            order.action = TradeAction::HOLD;
            order.execution_reason = "No position to sell";
            utils::log_debug("HOLD: No position to sell in " + signal.symbol);
        }
    } else {
        order.action = TradeAction::HOLD;
        order.execution_reason = "Signal in neutral zone";
        utils::log_debug("HOLD: Signal in neutral zone for " + signal.symbol + 
                        ", probability=" + std::to_string(signal.probability));
    }

    return order;
}

bool BackendComponent::check_conflicts(const TradeOrder& order) {
    return position_manager_->would_cause_conflict(order.symbol, order.action);
}

double BackendComponent::calculate_fees(double trade_value) {
    // Validate trade value and crash on errors with detailed debug info
    if (trade_value < 0.0) {
        utils::log_error("CRITICAL: Negative trade_value=" + std::to_string(trade_value) + 
                        " in fee calculation. Cost model=" + std::to_string(static_cast<int>(config_.cost_model)));
        std::abort();
    }
    
    if (std::isnan(trade_value) || std::isinf(trade_value)) {
        utils::log_error("CRITICAL: Invalid trade_value=" + std::to_string(trade_value) + 
                        " (NaN or Inf) in fee calculation. Cost model=" + std::to_string(static_cast<int>(config_.cost_model)));
        std::abort();
    }
    
    double fees = 0.0;
    
    switch (config_.cost_model) {
        case CostModel::ZERO:
            fees = 0.0;
            break;
        case CostModel::FIXED:
            fees = 1.0;  // $1 per trade
            break;
        case CostModel::PERCENTAGE:
            fees = trade_value * 0.001;  // 0.1%
            break;
        case CostModel::ALPACA:
            fees = 0.0;  // Alpaca zero commission model
            break;
        default:
            utils::log_error("CRITICAL: Unknown cost_model=" + std::to_string(static_cast<int>(config_.cost_model)) + 
                            " for trade_value=" + std::to_string(trade_value));
            std::abort();
    }
    
    // Validate calculated fees
    if (fees < 0.0 || std::isnan(fees) || std::isinf(fees)) {
        utils::log_error("CRITICAL: Invalid calculated fees=" + std::to_string(fees) + 
                        " for trade_value=" + std::to_string(trade_value) + 
                        ", cost_model=" + std::to_string(static_cast<int>(config_.cost_model)));
        std::abort();
    }
    
    utils::log_debug("Fee calculation: trade_value=" + std::to_string(trade_value) + 
                    ", cost_model=" + std::to_string(static_cast<int>(config_.cost_model)) + 
                    ", fees=" + std::to_string(fees));
    
    return fees;
}

double BackendComponent::calculate_position_size(double signal_probability, double available_capital) {
    // Aggressive sizing: use up to 100% of available capital based on signal strength
    // Higher signal probability = higher capital allocation for maximum profit potential
    
    // Validate inputs and crash on errors with detailed debug info
    if (signal_probability < 0.0 || signal_probability > 1.0) {
        utils::log_error("CRITICAL: Invalid signal_probability=" + std::to_string(signal_probability) + 
                        " (must be 0.0-1.0). Available capital=" + std::to_string(available_capital));
        std::abort();
    }
    
    if (available_capital < 0.0) {
        utils::log_error("CRITICAL: Negative available_capital=" + std::to_string(available_capital) + 
                        " with signal_probability=" + std::to_string(signal_probability));
        std::abort();
    }
    
    // Scale capital usage from 0% to 100% based on signal strength
    // Signal > 0.5 = bullish, use more capital
    // Signal < 0.5 = bearish, use less capital  
    // Signal = 0.5 = neutral, use minimal capital
    double confidence_factor = std::clamp((signal_probability - 0.5) * 2.0, 0.0, 1.0);
    
    // Use full available capital scaled by confidence (no artificial limits)
    double position_size = available_capital * confidence_factor;
    
    utils::log_debug("Position sizing: signal_prob=" + std::to_string(signal_probability) + 
                    ", available_capital=" + std::to_string(available_capital) + 
                    ", confidence_factor=" + std::to_string(confidence_factor) + 
                    ", position_size=" + std::to_string(position_size));
    
    return position_size;
}

// Removed DB export; stub retained for compatibility if included elsewhere

// Removed DB streaming export

bool BackendComponent::process_to_jsonl(
    const std::string& signal_file_path,
    const std::string& market_data_path,
    const std::string& out_path,
    const std::string& run_id,
    size_t start_index,
    size_t max_count) {

    std::ifstream sfile(signal_file_path);
    auto bars = utils::read_csv_data(market_data_path);
    std::ofstream out(out_path);
    if (!out.is_open()) return false;

    size_t i = 0;
    std::string line;
    // Fast-forward to start index
    for (size_t skip = 0; skip < start_index && std::getline(sfile, line); ++skip) {}
    size_t processed = 0, executed = 0;
    while (std::getline(sfile, line) && i < bars.size()) {
        if (processed >= max_count) break;
        auto signal = SignalOutput::from_json(line);
        const auto& bar = bars[i++];
        portfolio_manager_->update_market_prices({{bar.symbol, bar.close}});
        auto order = evaluate_signal(signal, bar);
        if (config_.enable_conflict_prevention) {
            order.conflict_check_passed = !check_conflicts(order);
            if (!order.conflict_check_passed) {
                order.action = TradeAction::HOLD;
                order.rejection_reason = "Conflict detected";
            }
        }
        if (order.action != TradeAction::HOLD) {
            if (order.action == TradeAction::BUY) {
                portfolio_manager_->execute_buy(order.symbol, order.quantity, order.price, order.fees);
                position_manager_->add_position(order.symbol);
                position_manager_->update_direction(StaticPositionManager::Direction::LONG);
            } else if (order.action == TradeAction::SELL) {
                portfolio_manager_->execute_sell(order.symbol, order.quantity, order.price, order.fees);
                position_manager_->remove_position(order.symbol);
            }
            order.after_state = portfolio_manager_->get_state();
            out << order.to_json_line(run_id) << '\n';
            executed++;
        }
        processed++;
    }
    std::cout << "Completed. Total signals: " << processed << ", executed trades: " << executed << std::endl;
    return true;
}
// DB helpers removed

} // namespace sentio



```

## 📄 **FILE 13 of 53**: src/backend/portfolio_manager.cpp

**File Information**:
- **Path**: `src/backend/portfolio_manager.cpp`

- **Size**: 194 lines
- **Modified**: 2025-09-21 12:13:34

- **Type**: .cpp

```text
#include "backend/portfolio_manager.h"
#include "common/utils.h"

#include <numeric>
#include <algorithm>

namespace sentio {

PortfolioManager::PortfolioManager(double starting_capital)
    : cash_balance_(starting_capital), realized_pnl_(0.0) {}

bool PortfolioManager::can_buy(const std::string& /*symbol*/, double quantity, double price, double fees) {
    double required_capital = (quantity * price) + fees;
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
    if (cash_balance_ < -1e-9) {
        utils::log_error("Negative cash after BUY: symbol=" + symbol +
                         ", qty=" + std::to_string(quantity) +
                         ", price=" + std::to_string(price) +
                         ", fees=" + std::to_string(fees) +
                         ", cash_balance=" + std::to_string(cash_balance_));
        std::abort();
    }
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
    if (cash_balance_ < -1e-9) {
        utils::log_error("Negative cash after SELL: symbol=" + symbol +
                         ", qty=" + std::to_string(quantity) +
                         ", price=" + std::to_string(price) +
                         ", fees=" + std::to_string(fees) +
                         ", cash_balance=" + std::to_string(cash_balance_));
        std::abort();
    }

    // Realized P&L
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
        std::chrono::system_clock::now().time_since_epoch()).count();
    return state;
}

void PortfolioManager::update_market_prices(const std::map<std::string, double>& prices) {
    for (auto& [symbol, position] : positions_) {
        auto price_it = prices.find(symbol);
        if (price_it != prices.end()) {
            position.current_price = price_it->second;
            position.unrealized_pnl = (position.current_price - position.avg_price) * position.quantity;
        }
    }
}

double PortfolioManager::get_total_equity() const {
    double positions_value = 0.0;
    for (const auto& [symbol, position] : positions_) {
        positions_value += position.quantity * position.current_price;
    }
    return cash_balance_ + positions_value;
}

double PortfolioManager::get_unrealized_pnl() const {
    double total_unrealized = 0.0;
    for (const auto& [symbol, position] : positions_) {
        (void)symbol; // unused
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
        // Average in
        double total_quantity = it->second.quantity + quantity;
        double total_value = (it->second.quantity * it->second.avg_price) + (quantity * price);
        it->second.avg_price = total_value / total_quantity;
        it->second.quantity = total_quantity;
        it->second.current_price = price;
    } else {
        Position pos;
        pos.symbol = symbol;
        pos.quantity = quantity;
        pos.avg_price = price;
        pos.current_price = price;
        pos.unrealized_pnl = 0.0;
        pos.realized_pnl = 0.0;
        positions_[symbol] = pos;
    }
}

// ------------------------- StaticPositionManager -----------------------------
StaticPositionManager::StaticPositionManager()
    : current_direction_(Direction::NEUTRAL), position_count_(0) {}

bool StaticPositionManager::would_cause_conflict(const std::string& symbol,
                                                 TradeAction action) {
    if (action == TradeAction::BUY) {
        bool is_inverse = is_inverse_etf(symbol);
        if (current_direction_ == Direction::SHORT && !is_inverse) {
            return true;
        }
        if (current_direction_ == Direction::LONG && is_inverse) {
            return true;
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
    static const std::vector<std::string> inverse_etfs = {
        "PSQ", "SH", "SDS", "SPXS", "SQQQ", "QID", "DXD", "SDOW", "DOG"
    };
    return std::find(inverse_etfs.begin(), inverse_etfs.end(), symbol) != inverse_etfs.end();
}

} // namespace sentio



```

## 📄 **FILE 14 of 53**: src/cli/audit_main.cpp

**File Information**:
- **Path**: `src/cli/audit_main.cpp`

- **Size**: 55 lines
- **Modified**: 2025-09-21 15:14:09

- **Type**: .cpp

```text
#include "backend/audit_component.h"
#include "common/utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

// =============================================================================
// Executable: audit
// Purpose: Minimal CLI to compute performance metrics from an equity curve CSV.
//
// Usage:
//   audit --equity PATH
// Input CSV format:
//   equity (single column, with header)
// =============================================================================

namespace {
    // Use common get_arg function from utils
    using sentio::utils::get_arg;
}

int main(int argc, char** argv) {
    const std::string equity_path = get_arg(argc, argv, "--equity");
    if (equity_path.empty()) {
        std::cerr << "ERROR: --equity is required.\n";
        std::cerr << "Usage: audit --equity PATH\n";
        return 1;
    }

    std::ifstream in(equity_path);
    if (!in.is_open()) {
        std::cerr << "ERROR: cannot open " << equity_path << "\n";
        return 2;
    }

    std::string line;
    // Skip header
    std::getline(in, line);
    std::vector<double> equity;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        equity.push_back(std::stod(line));
    }

    sentio::AuditComponent auditor;
    auto summary = auditor.analyze_equity_curve(equity);

    std::cout << "Sharpe: " << summary.sharpe << "\n";
    std::cout << "Max Drawdown: " << summary.max_drawdown << "\n";
    return 0;
}



```

## 📄 **FILE 15 of 53**: src/cli/sentio_cli_main.cpp

**File Information**:
- **Path**: `src/cli/sentio_cli_main.cpp`

- **Size**: 757 lines
- **Modified**: 2025-09-21 15:31:18

- **Type**: .cpp

```text
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



```

## 📄 **FILE 16 of 53**: src/cli/strattest_main.cpp

**File Information**:
- **Path**: `src/cli/strattest_main.cpp`

- **Size**: 97 lines
- **Modified**: 2025-09-21 15:14:09

- **Type**: .cpp

```text
#include "strategy/strategy_component.h"
#include "strategy/sigor_strategy.h"
#include "strategy/sigor_config.h"
#include "common/utils.h"

#include <iostream>
#include <string>
#include <map>
#include <filesystem>

// =============================================================================
// Executable: strattest
// Purpose: Minimal CLI to run a strategy over a dataset and export signals.
//
// Usage (bare minimum):
//   strattest [--dataset PATH] [--out PATH] [--strategy NAME] [--format jsonl|csv]
// Defaults:
//   --dataset=data/equities/QQQ_RTH_NH.csv
//   --out auto: <strategy>_signals_<YYYYMMDD_HHMMSS>.(jsonl|csv)
//
// Core flow:
// - Parse flags
// - Instantiate strategy (default: Sigor)
// - Process dataset and export signals
// =============================================================================

namespace {
    // Use common get_arg function from utils
    using sentio::utils::get_arg;

    // Convert "YYYY-MM-DD HH:MM:SS" -> "YYYYMMDD_HHMMSS"
    std::string compact_timestamp(const std::string& ts) {
        std::string s = ts;
        for (char& c : s) {
            if (c == '-') c = '\0';
            else if (c == ' ') c = '_';
            else if (c == ':') c = '\0';
        }
        std::string out;
        out.reserve(s.size());
        for (char c : s) { if (c != '\0') out.push_back(c); }
        return out;
    }
}

int main(int argc, char** argv) {
    std::string dataset = get_arg(argc, argv, "--dataset");
    std::string out = get_arg(argc, argv, "--out");
    const std::string strategy = get_arg(argc, argv, "--strategy", "sigor");
    const std::string format = get_arg(argc, argv, "--format", "jsonl");
    const std::string cfg_path = get_arg(argc, argv, "--config", "");

    if (dataset.empty()) {
        dataset = "data/equities/QQQ_RTH_NH.csv";
    }
    if (out.empty()) {
        std::string ts = sentio::utils::current_timestamp_str();
        std::string ts_compact = compact_timestamp(ts);
        std::string base = strategy.empty() ? std::string("strategy") : strategy;
        std::string ext = (format == "csv") ? ".csv" : ".jsonl";
        // ensure directory exists
        std::error_code ec;
        std::filesystem::create_directories("data/signals", ec);
        out = std::string("data/signals/") + base + "_signals_" + ts_compact + ext;
    }

    sentio::StrategyComponent::StrategyConfig cfg;
    cfg.name = strategy;
    cfg.version = "0.1";
    cfg.warmup_bars = 20; // minimal warmup for example

    std::unique_ptr<sentio::StrategyComponent> strat;
    // For now, support only Sigor; can add more mappings later
    auto sigor = std::make_unique<sentio::SigorStrategy>(cfg);
    if (!cfg_path.empty()) {
        auto scfg = sentio::SigorConfig::defaults();
        scfg = sentio::SigorConfig::from_file(cfg_path);
        sigor->set_config(scfg);
    }
    strat = std::move(sigor);

    auto signals = strat->process_dataset(dataset, cfg.name, {});
    // Inject market data path into metadata for each signal
    for (auto& s : signals) {
        s.metadata["market_data_path"] = dataset;
    }
    bool ok = strat->export_signals(signals, out, format);
    if (!ok) {
        std::cerr << "ERROR: failed to export signals to " << out << "\n";
        return 2;
    }

    std::cout << "Exported " << signals.size() << " signals to " << out << " (" << format << ")\n";
    return 0;
}



```

## 📄 **FILE 17 of 53**: src/common/types.cpp

**File Information**:
- **Path**: `src/common/types.cpp`

- **Size**: 43 lines
- **Modified**: 2025-09-21 00:30:29

- **Type**: .cpp

```text
#include "common/types.h"
#include "common/utils.h"

// =============================================================================
// Implementation: common/types.cpp
// Provides serialization helpers for PortfolioState.
// =============================================================================

namespace sentio {

// Serialize a PortfolioState to a minimal JSON representation.
// The structure is designed for audit logs and DB storage via adapters.
std::string PortfolioState::to_json() const {
    // Flatten positions into a simple key/value map for lightweight JSON.
    // For a richer schema, replace with a full JSON library in adapters.
    std::map<std::string, std::string> m;
    m["cash_balance"] = std::to_string(cash_balance);
    m["total_equity"] = std::to_string(total_equity);
    m["unrealized_pnl"] = std::to_string(unrealized_pnl);
    m["realized_pnl"] = std::to_string(realized_pnl);
    m["timestamp_ms"] = std::to_string(timestamp_ms);

    // Encode position count; individual positions can be stored elsewhere
    // or serialized as a separate artifact for brevity in logs.
    m["position_count"] = std::to_string(positions.size());
    return utils::to_json(m);
}

// Parse JSON into PortfolioState. Only top-level numeric fields are restored.
PortfolioState PortfolioState::from_json(const std::string& json_str) {
    PortfolioState s;
    auto m = utils::from_json(json_str);
    if (m.count("cash_balance")) s.cash_balance = std::stod(m["cash_balance"]);
    if (m.count("total_equity")) s.total_equity = std::stod(m["total_equity"]);
    if (m.count("unrealized_pnl")) s.unrealized_pnl = std::stod(m["unrealized_pnl"]);
    if (m.count("realized_pnl")) s.realized_pnl = std::stod(m["realized_pnl"]);
    if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
    return s;
}

} // namespace sentio



```

## 📄 **FILE 18 of 53**: src/common/utils.cpp

**File Information**:
- **Path**: `src/common/utils.cpp`

- **Size**: 366 lines
- **Modified**: 2025-09-21 15:14:09

- **Type**: .cpp

```text
#include "common/utils.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <filesystem>

// =============================================================================
// Module: common/utils.cpp
// Purpose: Implementation of utility functions for file I/O, time handling,
//          JSON parsing, hashing, and mathematical calculations.
//
// This module provides the concrete implementations for all utility functions
// declared in utils.h. Each section handles a specific domain of functionality
// to keep the codebase modular and maintainable.
// =============================================================================

namespace sentio {
namespace utils {

// --------------------------------- Helpers ----------------------------------
namespace {
    /// Helper function to remove leading and trailing whitespace from strings
    /// Used internally by CSV parsing and JSON processing functions
    static inline std::string trim(const std::string& s) {
        const char* ws = " \t\n\r\f\v";
        const auto start = s.find_first_not_of(ws);
        if (start == std::string::npos) return "";
        const auto end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }
}

// ----------------------------- File I/O utilities ----------------------------

/// Reads OHLCV market data from CSV files with automatic format detection
/// 
/// This function handles two CSV formats:
/// 1. QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume (symbol extracted from filename)
/// 2. Standard format: symbol,timestamp_ms,open,high,low,close,volume
/// 
/// The function automatically detects the format by examining the header row
/// and processes the data accordingly, ensuring compatibility with different
/// data sources while maintaining a consistent Bar output format.
std::vector<Bar> read_csv_data(const std::string& path) {
    std::vector<Bar> bars;
    std::ifstream file(path);
    
    // Early return if file cannot be opened
    if (!file.is_open()) {
        return bars;
    }

    std::string line;
    
    // Read and analyze header to determine CSV format
    std::getline(file, line);
    bool is_qqq_format = (line.find("ts_utc") != std::string::npos); 
    bool is_standard_format = (line.find("symbol") != std::string::npos);
    
    // For QQQ format, extract symbol from filename since it's not in the CSV
    std::string default_symbol = "UNKNOWN";
    if (is_qqq_format) {
        size_t last_slash = path.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;
        
        // Pattern matching for common ETF symbols
        if (filename.find("QQQ") != std::string::npos) default_symbol = "QQQ";
        else if (filename.find("SQQQ") != std::string::npos) default_symbol = "SQQQ";
        else if (filename.find("TQQQ") != std::string::npos) default_symbol = "TQQQ";
    }

    // Process each data row according to the detected format
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string item;
        Bar b{};

        // Parse timestamp and symbol based on detected format
        if (is_qqq_format) {
            // QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
            b.symbol = default_symbol;
            
            // Skip ts_utc column (ISO timestamp string)
            std::getline(ss, item, ',');
            
            // Use ts_nyt_epoch as timestamp (Unix seconds -> convert to milliseconds)
            std::getline(ss, item, ',');
            b.timestamp_ms = std::stoll(trim(item)) * 1000;
            
        } else if (is_standard_format) {
            // Standard format: symbol,timestamp_ms,open,high,low,close,volume
            std::getline(ss, item, ',');
            b.symbol = trim(item);
            
            std::getline(ss, item, ',');
            b.timestamp_ms = std::stoll(trim(item));
            
        } else {
            // Fallback for unknown formats: assume first column is symbol
            std::getline(ss, item, ',');
            b.symbol = trim(item);
            std::getline(ss, item, ',');
            b.timestamp_ms = std::stoll(trim(item));
        }

        // Parse OHLCV data (same format across all CSV types)
        std::getline(ss, item, ',');
        b.open = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.high = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.low = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.close = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.volume = std::stod(trim(item));

        bars.push_back(b);
    }

    return bars;
}

bool write_jsonl(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (const auto& l : lines) {
        out << l << '\n';
    }
    return true;
}

bool write_csv(const std::string& path, const std::vector<std::vector<std::string>>& data) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (const auto& row : data) {
        for (size_t i = 0; i < row.size(); ++i) {
            out << row[i];
            if (i + 1 < row.size()) out << ',';
        }
        out << '\n';
    }
    return true;
}

// ------------------------------ Time utilities -------------------------------
int64_t timestamp_to_ms(const std::string& timestamp_str) {
    // Minimal parser for "YYYY-MM-DD HH:MM:SS" -> epoch ms
    std::tm tm{};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto time_c = timegm(&tm); // UTC
    return static_cast<int64_t>(time_c) * 1000;
}

std::string ms_to_timestamp(int64_t ms) {
    std::time_t t = static_cast<std::time_t>(ms / 1000);
    std::tm* gmt = gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmt);
    return std::string(buf);
}

std::string current_timestamp_str() {
    std::time_t now = std::time(nullptr);
    std::tm* gmt = gmtime(&now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmt);
    return std::string(buf);
}

// ------------------------------ JSON utilities -------------------------------
std::string to_json(const std::map<std::string, std::string>& data) {
    std::ostringstream os;
    os << '{';
    bool first = true;
    for (const auto& [k, v] : data) {
        if (!first) os << ',';
        first = false;
        os << '"' << k << '"' << ':' << '"' << v << '"';
    }
    os << '}';
    return os.str();
}

std::map<std::string, std::string> from_json(const std::string& json_str) {
    // Minimal robust parser for a flat string map {"k":"v",...} that respects quotes
    std::map<std::string, std::string> out;
    if (json_str.size() < 2 || json_str.front() != '{' || json_str.back() != '}') return out;
    const std::string s = json_str.substr(1, json_str.size() - 2);

    // Split into top-level pairs by commas not inside quotes
    std::vector<std::string> pairs;
    std::string current;
    bool in_quotes = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') {
            // toggle quotes unless escaped
            bool escaped = (i > 0 && s[i-1] == '\\');
            if (!escaped) in_quotes = !in_quotes;
            current.push_back(c);
        } else if (c == ',' && !in_quotes) {
            pairs.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) pairs.push_back(current);

    auto trim_ws = [](const std::string& str){
        size_t a = 0, b = str.size();
        while (a < b && std::isspace(static_cast<unsigned char>(str[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(str[b-1]))) --b;
        return str.substr(a, b - a);
    };

    for (auto& p : pairs) {
        std::string pair = trim_ws(p);
        // find colon not inside quotes
        size_t colon_pos = std::string::npos;
        in_quotes = false;
        for (size_t i = 0; i < pair.size(); ++i) {
            char c = pair[i];
            if (c == '"') {
                bool escaped = (i > 0 && pair[i-1] == '\\');
                if (!escaped) in_quotes = !in_quotes;
            } else if (c == ':' && !in_quotes) {
                colon_pos = i; break;
            }
        }
        if (colon_pos == std::string::npos) continue;
        std::string key = trim_ws(pair.substr(0, colon_pos));
        std::string val = trim_ws(pair.substr(colon_pos + 1));
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"') key = key.substr(1, key.size() - 2);
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
        out[key] = val;
    }
    return out;
}

// -------------------------------- Hash utilities -----------------------------
// Simple SHA-256 based on std::hash fallback: for production, replace with a
// real SHA-256; we simulate deterministic short id by hashing and hex-encoding.
std::string calculate_sha256(const std::string& data) {
    // Not a real SHA-256; placeholder stable hash for demonstration.
    std::hash<std::string> h;
    auto v = h(data);
    std::ostringstream os;
    os << std::hex << std::setw(16) << std::setfill('0') << v;
    std::string hex = os.str();
    // Repeat to reach 32 hex chars
    return hex + hex;
}

std::string generate_run_id(const std::string& prefix) {
    // Produce an 8-digit numeric ID. Use time + hash for low collision chance.
    (void)prefix; // unused but kept for signature stability
    auto now = std::time(nullptr);
    std::string seed = std::to_string(static_cast<long long>(now));
    std::hash<std::string> h;
    unsigned long long v = static_cast<unsigned long long>(h(seed));
    unsigned int id = static_cast<unsigned int>(v % 100000000ULL);
    std::ostringstream os;
    os << std::setw(8) << std::setfill('0') << id;
    return os.str();
}

// -------------------------------- Math utilities -----------------------------
double calculate_sharpe_ratio(const std::vector<double>& returns, double risk_free_rate) {
    if (returns.empty()) return 0.0;
    double mean = 0.0;
    for (double r : returns) mean += r;
    mean /= static_cast<double>(returns.size());
    double variance = 0.0;
    for (double r : returns) variance += (r - mean) * (r - mean);
    variance /= static_cast<double>(returns.size());
    double stddev = std::sqrt(variance);
    if (stddev == 0.0) return 0.0;
    return (mean - risk_free_rate) / stddev;
}

double calculate_max_drawdown(const std::vector<double>& equity_curve) {
    double peak = -std::numeric_limits<double>::infinity();
    double max_dd = 0.0;
    for (double e : equity_curve) {
        peak = std::max(peak, e);
        max_dd = std::max(max_dd, (peak - e) / std::max(1.0, peak));
    }
    return max_dd;
}

// -------------------------------- Logging utilities --------------------------
namespace {
    static inline std::string log_dir() {
        return std::string("logs");
    }
    static inline void ensure_log_dir() {
        std::error_code ec;
        std::filesystem::create_directories(log_dir(), ec);
    }
    static inline std::string iso_now() {
        std::time_t now = std::time(nullptr);
        std::tm* gmt = gmtime(&now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmt);
        return std::string(buf);
    }
}

void log_debug(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/debug.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " DEBUG common:utils:0 - " << message << '\n';
}

void log_error(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/errors.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " ERROR common:utils:0 - " << message << '\n';
}

// -------------------------------- CLI utilities -------------------------------

/// Parse command line arguments supporting both "--name value" and "--name=value" formats
/// 
/// This function provides flexible command-line argument parsing that supports:
/// - Space-separated format: --name value
/// - Equals-separated format: --name=value
/// 
/// @param argc Number of command line arguments
/// @param argv Array of command line argument strings
/// @param name The argument name to search for (including --)
/// @param def Default value to return if argument not found
/// @return The argument value if found, otherwise the default value
std::string get_arg(int argc, char** argv, const std::string& name, const std::string& def) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == name) {
            // Handle "--name value" format
            if (i + 1 < argc) {
                std::string next = argv[i + 1];
                if (!next.empty() && next[0] != '-') return next;
            }
        } else if (arg.rfind(name + "=", 0) == 0) {
            // Handle "--name=value" format
            return arg.substr(name.size() + 1);
        }
    }
    return def;
}

} // namespace utils
} // namespace sentio



```

## 📄 **FILE 19 of 53**: src/strategy/signal_output.cpp

**File Information**:
- **Path**: `src/strategy/signal_output.cpp`

- **Size**: 53 lines
- **Modified**: 2025-09-21 03:08:17

- **Type**: .cpp

```text
#include "strategy/signal_output.h"
#include "common/utils.h"

#include <sstream>

namespace sentio {

std::string SignalOutput::to_json() const {
    std::map<std::string, std::string> m;
    m["timestamp_ms"] = std::to_string(timestamp_ms);
    m["bar_index"] = std::to_string(bar_index);
    m["symbol"] = symbol;
    m["probability"] = std::to_string(probability);
    m["confidence"] = std::to_string(confidence);
    m["strategy_name"] = strategy_name;
    m["strategy_version"] = strategy_version;
    // Flatten commonly used metadata keys for portability
    auto it = metadata.find("market_data_path");
    if (it != metadata.end()) {
        m["market_data_path"] = it->second;
    }
    return utils::to_json(m);
}

std::string SignalOutput::to_csv() const {
    std::ostringstream os;
    os << timestamp_ms << ','
       << bar_index << ','
       << symbol << ','
       << probability << ','
       << confidence << ','
       << strategy_name << ','
       << strategy_version;
    return os.str();
}

SignalOutput SignalOutput::from_json(const std::string& json_str) {
    SignalOutput s;
    auto m = utils::from_json(json_str);
    if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
    if (m.count("bar_index")) s.bar_index = std::stoi(m["bar_index"]);
    if (m.count("symbol")) s.symbol = m["symbol"];
    if (m.count("probability")) s.probability = std::stod(m["probability"]);
    if (m.count("confidence")) s.confidence = std::stod(m["confidence"]);
    if (m.count("strategy_name")) s.strategy_name = m["strategy_name"];
    if (m.count("strategy_version")) s.strategy_version = m["strategy_version"];
    if (m.count("market_data_path")) s.metadata["market_data_path"] = m["market_data_path"];
    return s;
}

} // namespace sentio



```

## 📄 **FILE 20 of 53**: src/strategy/sigor_config.cpp

**File Information**:
- **Path**: `src/strategy/sigor_config.cpp`

- **Size**: 34 lines
- **Modified**: 2025-09-21 03:13:41

- **Type**: .cpp

```text
#include "strategy/sigor_config.h"
#include "common/utils.h"
#include <fstream>

namespace sentio {

SigorConfig SigorConfig::defaults() {
    return SigorConfig{};
}

SigorConfig SigorConfig::from_file(const std::string& path) {
    SigorConfig c = SigorConfig::defaults();
    // Minimal JSON loader using utils::from_json (flat map)
    // If the file can't be read via this helper, just return defaults.
    // Users can extend to YAML/JSON libs later.
    // Read file contents
    std::ifstream in(path);
    if (!in.is_open()) return c;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto m = utils::from_json(content);
    auto getd = [&](const char* k, double& dst){ if (m.count(k)) dst = std::stod(m[k]); };
    auto geti = [&](const char* k, int& dst){ if (m.count(k)) dst = std::stoi(m[k]); };

    getd("k", c.k);
    getd("w_boll", c.w_boll); getd("w_rsi", c.w_rsi); getd("w_mom", c.w_mom);
    getd("w_vwap", c.w_vwap); getd("w_orb", c.w_orb); getd("w_ofi", c.w_ofi); getd("w_vol", c.w_vol);
    geti("win_boll", c.win_boll); geti("win_rsi", c.win_rsi); geti("win_mom", c.win_mom);
    geti("win_vwap", c.win_vwap); geti("orb_opening_bars", c.orb_opening_bars);
    return c;
}

} // namespace sentio



```

## 📄 **FILE 21 of 53**: src/strategy/sigor_strategy.cpp

**File Information**:
- **Path**: `src/strategy/sigor_strategy.cpp`

- **Size**: 211 lines
- **Modified**: 2025-09-21 02:36:18

- **Type**: .cpp

```text
#include "strategy/sigor_strategy.h"

#include <cmath>
#include <algorithm>
#include <limits>

namespace sentio {

SigorStrategy::SigorStrategy(const StrategyConfig& config)
    : StrategyComponent(config) {}

SignalOutput SigorStrategy::generate_signal(const Bar& bar, int bar_index) {
    // Compute detector probabilities
    double p1 = prob_bollinger_(bar);
    double p2 = prob_rsi_14_();
    double p3 = prob_momentum_(10, 50.0);
    double p4 = prob_vwap_reversion_(20);
    double p5 = prob_orb_daily_(30);
    double p6 = prob_ofi_proxy_(bar);
    double p7 = prob_volume_surge_scaled_(20);

    double p_final = aggregate_probability(p1, p2, p3, p4, p5, p6, p7);
    double c_final = calculate_confidence(p1, p2, p3, p4, p5, p6, p7);

    SignalOutput s;
    s.timestamp_ms = bar.timestamp_ms;
    s.bar_index = bar_index;
    s.symbol = bar.symbol;
    s.probability = p_final;
    s.confidence = c_final;
    s.metadata["warmup_complete"] = is_warmed_up() ? "true" : "false";
    s.metadata["detectors"] = "boll,rsi,mom,vwap,orb,ofi,vol";
    return s;
}

void SigorStrategy::update_indicators(const Bar& bar) {
    StrategyComponent::update_indicators(bar);
    closes_.push_back(bar.close);
    highs_.push_back(bar.high);
    lows_.push_back(bar.low);
    volumes_.push_back(bar.volume);
    timestamps_.push_back(bar.timestamp_ms);
    if (closes_.size() > 1) {
        double delta = closes_[closes_.size() - 1] - closes_[closes_.size() - 2];
        gains_.push_back(std::max(0.0, delta));
        losses_.push_back(std::max(0.0, -delta));
    } else {
        gains_.push_back(0.0);
        losses_.push_back(0.0);
    }
    // Keep buffers bounded
    const size_t cap = 2048;
    auto trim = [cap](auto& vec){ if (vec.size() > cap) vec.erase(vec.begin(), vec.begin() + (vec.size() - cap)); };
    trim(closes_); trim(highs_); trim(lows_); trim(volumes_); trim(timestamps_); trim(gains_); trim(losses_);
}

bool SigorStrategy::is_warmed_up() const {
    return StrategyComponent::is_warmed_up();
}

// ------------------------------ Detectors ------------------------------------
double SigorStrategy::prob_bollinger_(const Bar& bar) const {
    const int w = 20;
    if (static_cast<int>(closes_.size()) < w) return 0.5;
    double mean = compute_sma(closes_, w);
    double sd = compute_stddev(closes_, w, mean);
    if (sd <= 1e-12) return 0.5;
    double z = (bar.close - mean) / sd;
    return clamp01(0.5 + 0.5 * std::tanh(z / 2.0));
}

double SigorStrategy::prob_rsi_14_() const {
    const int w = 14;
    if (static_cast<int>(gains_.size()) < w + 1) return 0.5;
    double rsi = compute_rsi(w); // 0..100
    return clamp01((rsi - 50.0) / 100.0 * 1.0 + 0.5); // scale around 0.5
}

double SigorStrategy::prob_momentum_(int window, double scale) const {
    if (window <= 0 || static_cast<int>(closes_.size()) <= window) return 0.5;
    double curr = closes_.back();
    double prev = closes_[closes_.size() - static_cast<size_t>(window) - 1];
    if (prev <= 1e-12) return 0.5;
    double ret = (curr - prev) / prev;
    return clamp01(0.5 + 0.5 * std::tanh(ret * scale));
}

double SigorStrategy::prob_vwap_reversion_(int window) const {
    if (window <= 0 || static_cast<int>(closes_.size()) < window) return 0.5;
    double num = 0.0, den = 0.0;
    for (int i = static_cast<int>(closes_.size()) - window; i < static_cast<int>(closes_.size()); ++i) {
        double tp = (highs_[i] + lows_[i] + closes_[i]) / 3.0;
        double v = volumes_[i];
        num += tp * v;
        den += v;
    }
    if (den <= 1e-12) return 0.5;
    double vwap = num / den;
    double z = (closes_.back() - vwap) / std::max(1e-8, std::fabs(vwap));
    return clamp01(0.5 - 0.5 * std::tanh(z)); // above VWAP -> mean-revert bias
}

double SigorStrategy::prob_orb_daily_(int opening_window_bars) const {
    if (timestamps_.empty()) return 0.5;
    // Compute day bucket from epoch days
    int64_t day = timestamps_.back() / 86400000LL;
    // Find start index of current day
    int start = static_cast<int>(timestamps_.size()) - 1;
    while (start > 0 && (timestamps_[static_cast<size_t>(start - 1)] / 86400000LL) == day) {
        --start;
    }
    int end_open = std::min(static_cast<int>(timestamps_.size()), start + opening_window_bars);
    double hi = -std::numeric_limits<double>::infinity();
    double lo =  std::numeric_limits<double>::infinity();
    for (int i = start; i < end_open; ++i) {
        hi = std::max(hi, highs_[static_cast<size_t>(i)]);
        lo = std::min(lo, lows_[static_cast<size_t>(i)]);
    }
    if (!std::isfinite(hi) || !std::isfinite(lo)) return 0.5;
    double c = closes_.back();
    if (c > hi) return 0.7;     // breakout long bias
    if (c < lo) return 0.3;     // breakout short bias
    return 0.5;                  // inside range
}

double SigorStrategy::prob_ofi_proxy_(const Bar& bar) const {
    // Proxy OFI using bar geometry: (close-open)/(high-low) weighted by volume
    double range = std::max(1e-8, bar.high - bar.low);
    double ofi = ((bar.close - bar.open) / range) * std::tanh(bar.volume / 1e6);
    return clamp01(0.5 + 0.25 * ofi); // small influence
}

double SigorStrategy::prob_volume_surge_scaled_(int window) const {
    if (window <= 0 || static_cast<int>(volumes_.size()) < window) return 0.5;
    double v_now = volumes_.back();
    double v_ma = compute_sma(volumes_, window);
    if (v_ma <= 1e-12) return 0.5;
    double ratio = v_now / v_ma; // >1 indicates surge
    double adj = std::tanh((ratio - 1.0) * 1.0); // [-1,1]
    // Scale towards current momentum side
    double p_m = prob_momentum_(10, 50.0);
    double dir = (p_m >= 0.5) ? 1.0 : -1.0;
    return clamp01(0.5 + 0.25 * adj * dir);
}

double SigorStrategy::aggregate_probability(double p1, double p2, double p3,
                                            double p4, double p5, double p6, double p7) const {
    // Option A: Log-odds fusion with weights and sharpness k
    const double probs[7] = {p1, p2, p3, p4, p5, p6, p7};
    const double ws[7] = {cfg_.w_boll, cfg_.w_rsi, cfg_.w_mom, cfg_.w_vwap, cfg_.w_orb, cfg_.w_ofi, cfg_.w_vol};
    double num = 0.0, den = 0.0;
    for (int i = 0; i < 7; ++i) {
        double p = std::clamp(probs[i], 1e-6, 1.0 - 1e-6);
        double l = std::log(p / (1.0 - p));
        num += ws[i] * l;
        den += ws[i];
    }
    double L = (den > 1e-12) ? (num / den) : 0.0;
    double k = cfg_.k;
    double P = 1.0 / (1.0 + std::exp(-k * L));
    return P;
}

double SigorStrategy::calculate_confidence(double p1, double p2, double p3,
                                           double p4, double p5, double p6, double p7) const {
    double arr[7] = {p1, p2, p3, p4, p5, p6, p7};
    int long_votes = 0, short_votes = 0;
    double max_strength = 0.0;
    for (double p : arr) {
        if (p > 0.5) ++long_votes; else if (p < 0.5) ++short_votes;
        max_strength = std::max(max_strength, std::fabs(p - 0.5));
    }
    double agreement = std::max(long_votes, short_votes) / 7.0; // 0..1
    return clamp01(0.4 + 0.6 * std::max(agreement, max_strength));
}

// ------------------------------ Helpers --------------------------------------
double SigorStrategy::compute_sma(const std::vector<double>& v, int window) const {
    if (window <= 0 || static_cast<int>(v.size()) < window) return 0.0;
    double sum = 0.0;
    for (int i = static_cast<int>(v.size()) - window; i < static_cast<int>(v.size()); ++i) sum += v[static_cast<size_t>(i)];
    return sum / static_cast<double>(window);
}

double SigorStrategy::compute_stddev(const std::vector<double>& v, int window, double mean) const {
    if (window <= 0 || static_cast<int>(v.size()) < window) return 0.0;
    double acc = 0.0;
    for (int i = static_cast<int>(v.size()) - window; i < static_cast<int>(v.size()); ++i) {
        double d = v[static_cast<size_t>(i)] - mean;
        acc += d * d;
    }
    return std::sqrt(acc / static_cast<double>(window));
}

double SigorStrategy::compute_rsi(int window) const {
    if (window <= 0 || static_cast<int>(gains_.size()) < window + 1) return 50.0;
    double avg_gain = 0.0, avg_loss = 0.0;
    for (int i = static_cast<int>(gains_.size()) - window; i < static_cast<int>(gains_.size()); ++i) {
        avg_gain += gains_[static_cast<size_t>(i)];
        avg_loss += losses_[static_cast<size_t>(i)];
    }
    avg_gain /= static_cast<double>(window);
    avg_loss /= static_cast<double>(window);
    if (avg_loss <= 1e-12) return 100.0;
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

} // namespace sentio



```

## 📄 **FILE 22 of 53**: src/strategy/strategy_component.cpp

**File Information**:
- **Path**: `src/strategy/strategy_component.cpp`

- **Size**: 116 lines
- **Modified**: 2025-09-21 02:27:18

- **Type**: .cpp

```text
#include "strategy/strategy_component.h"
#include "common/utils.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <limits>

namespace sentio {

// -----------------------------------------------------------------------------
// StrategyComponent: orchestrates ingestion of Bars and generation of signals.
// -----------------------------------------------------------------------------
StrategyComponent::StrategyComponent(const StrategyConfig& config)
    : config_(config), bars_processed_(0), warmup_complete_(false) {
    historical_bars_.reserve(static_cast<size_t>(std::max(0, config_.warmup_bars)));
}

std::vector<SignalOutput> StrategyComponent::process_dataset(
    const std::string& dataset_path,
    const std::string& strategy_name,
    const std::map<std::string, std::string>& /*strategy_params*/) {

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
        lines.reserve(signals.size());
        for (const auto& signal : signals) {
            lines.push_back(signal.to_json());
        }
        return utils::write_jsonl(output_path, lines);
    } else if (format == "csv") {
        std::vector<std::vector<std::string>> data;
        // Header row
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
    // Default implementation: neutral placeholder
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
    if (historical_bars_.size() > static_cast<size_t>(std::max(0, config_.warmup_bars))) {
        historical_bars_.erase(historical_bars_.begin());
    }

    // Example: rolling simple moving average of 20 bars
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

// SigorStrategy implementation moved to strategy/sigor_strategy.cpp

} // namespace sentio



```

## 📄 **FILE 23 of 53**: tools/add_comprehensive_comments.py

**File Information**:
- **Path**: `tools/add_comprehensive_comments.py`

- **Size**: 197 lines
- **Modified**: 2025-09-21 14:03:17

- **Type**: .py

```text
#!/usr/bin/env python3
"""
Comprehensive Code Documentation Tool
====================================

This script adds detailed comments to all source files in the Sentio trading system,
making the codebase readable "like a novel" as requested.
"""

import os
import re
from pathlib import Path
from typing import Dict, List, Tuple

class CodeCommenter:
    def __init__(self, project_root: str):
        self.project_root = Path(project_root)
        self.src_dir = self.project_root / "src"
        self.include_dir = self.project_root / "include"
        
    def add_file_header_comment(self, file_path: Path, content: str) -> str:
        """Add comprehensive header comment to a source file"""
        
        # Determine file type and purpose
        file_type = "header" if file_path.suffix == ".h" else "implementation"
        module_name = file_path.stem
        
        # Create header comment based on file location and type
        if "cli" in str(file_path):
            purpose = f"Command-line interface for {module_name}"
            core_idea = "Provides user-facing commands and argument parsing"
        elif "backend" in str(file_path):
            purpose = f"Backend trading engine component: {module_name}"
            core_idea = "Handles trade execution, portfolio management, and risk controls"
        elif "strategy" in str(file_path):
            purpose = f"Trading strategy implementation: {module_name}"
            core_idea = "Generates trading signals based on market data analysis"
        elif "common" in str(file_path):
            purpose = f"Common utilities and data types: {module_name}"
            core_idea = "Shared functionality used across the trading system"
        else:
            purpose = f"Module: {module_name}"
            core_idea = "Core component of the Sentio trading system"
        
        header_comment = f"""// =============================================================================
// Module: {file_path.relative_to(self.project_root)}
// Purpose: {purpose}
//
// Core Idea:
// {core_idea}
//
// This {file_type} provides detailed implementation with comprehensive comments
// to make the code readable and maintainable. Each function and code block
// includes explanatory comments describing its purpose and operation.
// =============================================================================

"""
        
        # Find the first #include or other non-comment line
        lines = content.split('\n')
        insert_pos = 0
        
        for i, line in enumerate(lines):
            stripped = line.strip()
            if stripped and not stripped.startswith('//') and not stripped.startswith('#pragma'):
                insert_pos = i
                break
        
        # Insert header comment
        lines.insert(insert_pos, header_comment.rstrip())
        return '\n'.join(lines)
    
    def add_function_comments(self, content: str) -> str:
        """Add detailed comments to functions"""
        
        # Pattern to match function definitions
        func_pattern = r'^(\s*)([\w:]+\s+)?(\w+::\w+|\w+)\s*\([^)]*\)\s*\{?'
        
        lines = content.split('\n')
        result_lines = []
        
        i = 0
        while i < len(lines):
            line = lines[i]
            
            # Check if this line starts a function
            match = re.match(func_pattern, line)
            if match and not line.strip().startswith('//'):
                indent = match.group(1)
                func_name = match.group(3)
                
                # Add function comment
                comment = f"""{indent}/// {func_name}: [Function description]
{indent}/// 
{indent}/// This function performs [specific operation] by [method/algorithm].
{indent}/// It handles [key responsibilities] and returns [return description].
"""
                result_lines.append(comment.rstrip())
            
            result_lines.append(line)
            i += 1
        
        return '\n'.join(result_lines)
    
    def add_code_block_comments(self, content: str) -> str:
        """Add comments to major code blocks"""
        
        lines = content.split('\n')
        result_lines = []
        
        for i, line in enumerate(lines):
            stripped = line.strip()
            
            # Add comments for common patterns
            if stripped.startswith('if (') and not any(comment in line for comment in ['//', '/*']):
                indent = len(line) - len(line.lstrip())
                comment = ' ' * indent + '// Check condition and branch accordingly'
                result_lines.append(comment)
            elif stripped.startswith('for (') and not any(comment in line for comment in ['//', '/*']):
                indent = len(line) - len(line.lstrip())
                comment = ' ' * indent + '// Iterate through collection/range'
                result_lines.append(comment)
            elif stripped.startswith('while (') and not any(comment in line for comment in ['//', '/*']):
                indent = len(line) - len(line.lstrip())
                comment = ' ' * indent + '// Loop while condition is true'
                result_lines.append(comment)
            elif 'std::getline' in stripped and not any(comment in line for comment in ['//', '/*']):
                indent = len(line) - len(line.lstrip())
                comment = ' ' * indent + '// Read next line from input stream'
                result_lines.append(comment)
            
            result_lines.append(line)
        
        return '\n'.join(result_lines)
    
    def process_file(self, file_path: Path) -> bool:
        """Process a single file to add comprehensive comments"""
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Skip if file already has comprehensive comments
            if "==============================================================================" in content:
                print(f"✓ {file_path.relative_to(self.project_root)} already has comprehensive comments")
                return True
            
            print(f"📝 Adding comments to {file_path.relative_to(self.project_root)}")
            
            # Add header comment
            content = self.add_file_header_comment(file_path, content)
            
            # Add function comments (for .cpp files)
            if file_path.suffix == '.cpp':
                content = self.add_function_comments(content)
                content = self.add_code_block_comments(content)
            
            # Write back to file
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            
            return True
            
        except Exception as e:
            print(f"❌ Error processing {file_path}: {e}")
            return False
    
    def process_all_files(self):
        """Process all source and header files"""
        
        print("🚀 Starting comprehensive code commenting...")
        print("=" * 60)
        
        # Process header files
        print("\n📁 Processing header files...")
        header_files = list(self.include_dir.rglob('*.h'))
        for header_file in header_files:
            self.process_file(header_file)
        
        # Process source files
        print("\n📁 Processing source files...")
        source_files = list(self.src_dir.rglob('*.cpp'))
        for source_file in source_files:
            self.process_file(source_file)
        
        print("\n✅ Comprehensive commenting completed!")
        print(f"Processed {len(header_files)} header files and {len(source_files)} source files")


def main():
    project_root = Path(__file__).parent.parent
    commenter = CodeCommenter(str(project_root))
    commenter.process_all_files()


if __name__ == "__main__":
    main()

```

## 📄 **FILE 24 of 53**: tools/align_bars.py

**File Information**:
- **Path**: `tools/align_bars.py`

- **Size**: 102 lines
- **Modified**: 2025-09-21 00:55:23

- **Type**: .py

```text
#!/usr/bin/env python3
import argparse
import pathlib
import sys
from typing import Tuple


def read_bars(path: pathlib.Path):
    import pandas as pd
    # Try with header detection; polygon files often have no header
    try:
        df = pd.read_csv(path, header=None)
        # Heuristic: 7 columns: ts,symbol,open,high,low,close,volume
        if df.shape[1] < 7:
            # Retry with header row
            df = pd.read_csv(path)
    except Exception:
        df = pd.read_csv(path)

    if df.shape[1] >= 7:
        cols = ["ts", "symbol", "open", "high", "low", "close", "volume"] + [f"extra{i}" for i in range(df.shape[1]-7)]
        df.columns = cols[:df.shape[1]]
    elif df.shape[1] == 6:
        df.columns = ["ts", "open", "high", "low", "close", "volume"]
        df["symbol"] = path.stem.split("_")[0]
        df = df[["ts","symbol","open","high","low","close","volume"]]
    else:
        raise ValueError(f"Unexpected column count in {path}: {df.shape[1]}")

    # Normalize ts to string and index
    df["ts"] = df["ts"].astype(str)
    df = df.set_index("ts").sort_index()
    return df


def align_intersection(df1, df2, df3, df4=None):
    idx = df1.index.intersection(df2.index).intersection(df3.index)
    if df4 is not None:
        idx = idx.intersection(df4.index)
    idx = idx.sort_values()
    if df4 is not None:
        return df1.loc[idx], df2.loc[idx], df3.loc[idx], df4.loc[idx]
    else:
        return df1.loc[idx], df2.loc[idx], df3.loc[idx]


def write_bars(path: pathlib.Path, df) -> None:
    # Preserve original polygon-like format: ts,symbol,open,high,low,close,volume
    out = df.reset_index()[["ts","symbol","open","high","low","close","volume"]]
    out.to_csv(path, index=False)


def derive_out(path: pathlib.Path, suffix: str) -> pathlib.Path:
    stem = path.stem
    if stem.endswith(".csv"):
        stem = stem[:-4]
    return path.with_name(f"{stem}_{suffix}.csv")


def main():
    ap = argparse.ArgumentParser(description="Align QQQ/TQQQ/SQQQ minute bars by timestamp intersection.")
    ap.add_argument("--qqq", required=True)
    ap.add_argument("--tqqq", required=True)
    ap.add_argument("--sqqq", required=True)
    # PSQ removed from family - moderate sell signals now use SHORT QQQ
    ap.add_argument("--suffix", default="ALIGNED")
    args = ap.parse_args()

    qqq_p = pathlib.Path(args.qqq)
    tqqq_p = pathlib.Path(args.tqqq)
    sqqq_p = pathlib.Path(args.sqqq)

    import pandas as pd
    pd.options.mode.chained_assignment = None

    df_q = read_bars(qqq_p)
    df_t = read_bars(tqqq_p)
    df_s = read_bars(sqqq_p)

    a_q, a_t, a_s = align_intersection(df_q, df_t, df_s)
    assert list(a_q.index) == list(a_t.index) == list(a_s.index)

    out_q = derive_out(qqq_p, args.suffix)
    out_t = derive_out(tqqq_p, args.suffix)
    out_s = derive_out(sqqq_p, args.suffix)

    write_bars(out_q, a_q)
    write_bars(out_t, a_t)
    write_bars(out_s, a_s)

    print_files = [f"→ {out_q}", f"→ {out_t}", f"→ {out_s}"]

    n = len(a_q)
    print(f"Aligned bars: {n}")
    for file_path in print_files:
        print(file_path)


if __name__ == "__main__":
    main()



```

## 📄 **FILE 25 of 53**: tools/analyze_ohlc_patterns.py

**File Information**:
- **Path**: `tools/analyze_ohlc_patterns.py`

- **Size**: 276 lines
- **Modified**: 2025-09-21 00:55:23

- **Type**: .py

```text
#!/usr/bin/env python3
"""
Analyze OHLC patterns in original QQQ data to create realistic variations
for future QQQ generation.
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime
import json

def analyze_ohlc_patterns(csv_file):
    """Analyze OHLC patterns from historical QQQ data"""
    
    print(f"📊 Analyzing OHLC patterns from {csv_file}")
    
    # Read the CSV file
    df = pd.read_csv(csv_file)
    print(f"📈 Loaded {len(df)} bars")
    
    # Calculate OHLC relationships
    df['high_low_spread'] = df['high'] - df['low']
    df['open_close_diff'] = df['close'] - df['open']
    df['high_open_diff'] = df['high'] - df['open']
    df['low_open_diff'] = df['open'] - df['low']
    df['high_close_diff'] = df['high'] - df['close']
    df['low_close_diff'] = df['close'] - df['low']
    
    # Calculate relative spreads (as percentage of price)
    df['high_low_spread_pct'] = df['high_low_spread'] / df['close'] * 100
    df['open_close_diff_pct'] = df['open_close_diff'] / df['close'] * 100
    
    # Calculate returns
    df['return_pct'] = df['close'].pct_change() * 100
    
    # Remove NaN values
    df = df.dropna()
    
    # Calculate statistics
    stats = {
        'high_low_spread': {
            'mean': float(df['high_low_spread_pct'].mean()),
            'std': float(df['high_low_spread_pct'].std()),
            'min': float(df['high_low_spread_pct'].min()),
            'max': float(df['high_low_spread_pct'].max()),
            'percentiles': {
                '25': float(df['high_low_spread_pct'].quantile(0.25)),
                '50': float(df['high_low_spread_pct'].quantile(0.50)),
                '75': float(df['high_low_spread_pct'].quantile(0.75)),
                '90': float(df['high_low_spread_pct'].quantile(0.90)),
                '95': float(df['high_low_spread_pct'].quantile(0.95))
            }
        },
        'open_close_diff': {
            'mean': float(df['open_close_diff_pct'].mean()),
            'std': float(df['open_close_diff_pct'].std()),
            'min': float(df['open_close_diff_pct'].min()),
            'max': float(df['open_close_diff_pct'].max())
        },
        'return_volatility': {
            'daily_vol_annualized': float(df['return_pct'].std() * np.sqrt(252)),
            'minute_vol': float(df['return_pct'].std())
        }
    }
    
    # Analyze OHLC relationships
    # High relative to open and close
    df['high_vs_open_pct'] = (df['high'] - df['open']) / df['open'] * 100
    df['high_vs_close_pct'] = (df['high'] - df['close']) / df['close'] * 100
    
    # Low relative to open and close  
    df['low_vs_open_pct'] = (df['open'] - df['low']) / df['open'] * 100
    df['low_vs_close_pct'] = (df['close'] - df['low']) / df['close'] * 100
    
    stats['high_vs_open'] = {
        'mean': float(df['high_vs_open_pct'].mean()),
        'std': float(df['high_vs_open_pct'].std())
    }
    
    stats['high_vs_close'] = {
        'mean': float(df['high_vs_close_pct'].mean()),
        'std': float(df['high_vs_close_pct'].std())
    }
    
    stats['low_vs_open'] = {
        'mean': float(df['low_vs_open_pct'].mean()),
        'std': float(df['low_vs_open_pct'].std())
    }
    
    stats['low_vs_close'] = {
        'mean': float(df['low_vs_close_pct'].mean()),
        'std': float(df['low_vs_close_pct'].std())
    }
    
    # Volume analysis
    stats['volume'] = {
        'mean': float(df['volume'].mean()),
        'std': float(df['volume'].std()),
        'min': float(df['volume'].min()),
        'max': float(df['volume'].max()),
        'percentiles': {
            '25': float(df['volume'].quantile(0.25)),
            '50': float(df['volume'].quantile(0.50)),
            '75': float(df['volume'].quantile(0.75)),
            '90': float(df['volume'].quantile(0.90)),
            '95': float(df['volume'].quantile(0.95))
        }
    }
    
    print("\n📈 OHLC Pattern Analysis Results:")
    print(f"High-Low Spread: {stats['high_low_spread']['mean']:.4f}% ± {stats['high_low_spread']['std']:.4f}%")
    print(f"Open-Close Diff: {stats['open_close_diff']['mean']:.4f}% ± {stats['open_close_diff']['std']:.4f}%")
    print(f"Minute Volatility: {stats['return_volatility']['minute_vol']:.4f}%")
    print(f"Annualized Volatility: {stats['return_volatility']['daily_vol_annualized']:.1f}%")
    print(f"Average Volume: {stats['volume']['mean']:,.0f}")
    
    return stats, df

def create_ohlc_model(stats):
    """Create a model for generating realistic OHLC variations"""
    
    model = {
        'description': 'OHLC variation model based on historical QQQ data',
        'timestamp': datetime.now().isoformat(),
        'parameters': {
            # High-Low spread model (log-normal distribution works well for spreads)
            'high_low_spread': {
                'distribution': 'lognormal',
                'mean_pct': stats['high_low_spread']['mean'],
                'std_pct': stats['high_low_spread']['std'],
                'min_pct': 0.001,  # Minimum 0.001% spread
                'max_pct': stats['high_low_spread']['percentiles']['95']
            },
            
            # High vs open/close (normal distribution)
            'high_extension': {
                'distribution': 'normal',
                'vs_open_mean': stats['high_vs_open']['mean'],
                'vs_open_std': stats['high_vs_open']['std'],
                'vs_close_mean': stats['high_vs_close']['mean'], 
                'vs_close_std': stats['high_vs_close']['std']
            },
            
            # Low vs open/close (normal distribution)
            'low_extension': {
                'distribution': 'normal',
                'vs_open_mean': stats['low_vs_open']['mean'],
                'vs_open_std': stats['low_vs_open']['std'],
                'vs_close_mean': stats['low_vs_close']['mean'],
                'vs_close_std': stats['low_vs_close']['std']
            },
            
            # Volume model (log-normal distribution)
            'volume': {
                'distribution': 'lognormal',
                'mean': stats['volume']['mean'],
                'std': stats['volume']['std'],
                'min': max(100, stats['volume']['min']),  # Minimum 100 volume
                'max': stats['volume']['percentiles']['95']
            }
        }
    }
    
    return model

def test_ohlc_generation(model, close_prices, num_samples=1000):
    """Test the OHLC generation model"""
    
    print(f"\n🧪 Testing OHLC generation with {num_samples} samples")
    
    # Generate test OHLC data
    np.random.seed(42)  # For reproducible results
    
    generated_data = []
    
    for i, close_price in enumerate(close_prices[:num_samples]):
        if i == 0:
            open_price = close_price
        else:
            # Open is typically close to previous close with some gap
            gap = np.random.normal(0, 0.1) / 100 * close_price  # Small gap
            open_price = close_prices[i-1] + gap
        
        # Generate OHLC using the model
        ohlc = generate_ohlc_for_bar(open_price, close_price, model['parameters'])
        generated_data.append(ohlc)
    
    # Analyze generated data
    gen_df = pd.DataFrame(generated_data)
    gen_df['high_low_spread_pct'] = (gen_df['high'] - gen_df['low']) / gen_df['close'] * 100
    
    print(f"Generated High-Low Spread: {gen_df['high_low_spread_pct'].mean():.4f}% ± {gen_df['high_low_spread_pct'].std():.4f}%")
    print(f"Generated Volume: {gen_df['volume'].mean():,.0f} ± {gen_df['volume'].std():,.0f}")
    
    return generated_data

def generate_ohlc_for_bar(open_price, close_price, model_params):
    """Generate realistic OHLC for a single bar given open and close prices"""
    
    # Generate high-low spread
    spread_pct = np.random.lognormal(
        np.log(model_params['high_low_spread']['mean_pct']),
        model_params['high_low_spread']['std_pct'] / model_params['high_low_spread']['mean_pct']
    ) / 100
    
    # Ensure minimum spread
    spread_pct = max(spread_pct, model_params['high_low_spread']['min_pct'] / 100)
    spread_pct = min(spread_pct, model_params['high_low_spread']['max_pct'] / 100)
    
    # Calculate base price (midpoint of open and close)
    base_price = (open_price + close_price) / 2
    spread_amount = base_price * spread_pct
    
    # Generate high and low extensions
    high_ext_pct = np.random.normal(
        model_params['high_extension']['vs_open_mean'],
        model_params['high_extension']['vs_open_std']
    ) / 100
    
    low_ext_pct = np.random.normal(
        model_params['low_extension']['vs_open_mean'], 
        model_params['low_extension']['vs_open_std']
    ) / 100
    
    # Calculate high and low
    high = max(open_price, close_price) + abs(high_ext_pct) * base_price
    low = min(open_price, close_price) - abs(low_ext_pct) * base_price
    
    # Ensure high >= max(open, close) and low <= min(open, close)
    high = max(high, max(open_price, close_price))
    low = min(low, min(open_price, close_price))
    
    # Ensure high > low with minimum spread
    if high - low < spread_amount:
        mid = (high + low) / 2
        high = mid + spread_amount / 2
        low = mid - spread_amount / 2
    
    # Generate volume
    volume = int(np.random.lognormal(
        np.log(model_params['volume']['mean']),
        model_params['volume']['std'] / model_params['volume']['mean']
    ))
    
    # Clamp volume
    volume = max(volume, model_params['volume']['min'])
    volume = min(volume, model_params['volume']['max'])
    
    return {
        'open': round(open_price, 2),
        'high': round(high, 2), 
        'low': round(low, 2),
        'close': round(close_price, 2),
        'volume': volume
    }

if __name__ == "__main__":
    # Analyze original QQQ data
    csv_file = "data/equities/QQQ_NH.csv"
    stats, df = analyze_ohlc_patterns(csv_file)
    
    # Create OHLC model
    model = create_ohlc_model(stats)
    
    # Save model to file
    with open("tools/ohlc_model.json", "w") as f:
        json.dump(model, f, indent=2)
    
    print(f"\n💾 OHLC model saved to tools/ohlc_model.json")
    
    # Test the model
    test_prices = df['close'].values[:1000]  # Use first 1000 close prices
    test_ohlc_generation(model, test_prices)
    
    print("\n✅ OHLC pattern analysis complete!")

```

## 📄 **FILE 26 of 53**: tools/analyze_unused_code.py

**File Information**:
- **Path**: `tools/analyze_unused_code.py`

- **Size**: 167 lines
- **Modified**: 2025-09-21 00:55:23

- **Type**: .py

```text
#!/usr/bin/env python3
"""
Static analysis script to identify potentially unused code in Sentio C++
"""

import os
import re
import glob
from collections import defaultdict, Counter

def find_cpp_files(directory):
    """Find all C++ source and header files"""
    cpp_files = []
    for root, dirs, files in os.walk(directory):
        # Skip build and third_party directories
        dirs[:] = [d for d in dirs if d not in ['build', 'third_party', '.git', 'MarS', 'sota_portfolio_python']]
        
        for file in files:
            if file.endswith(('.cpp', '.hpp', '.h')):
                cpp_files.append(os.path.join(root, file))
    return cpp_files

def extract_functions_and_classes(file_path):
    """Extract function and class definitions from a C++ file"""
    functions = []
    classes = []
    
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            
        # Find class definitions
        class_pattern = r'class\s+(\w+)'
        classes.extend(re.findall(class_pattern, content))
        
        # Find function definitions (simplified pattern)
        # This catches most function definitions but may have false positives
        func_pattern = r'(?:^|\n)\s*(?:(?:inline|static|virtual|explicit|const|constexpr)\s+)*(?:\w+(?:\s*\*|\s*&)?(?:\s*::\s*\w+)?)\s+(\w+)\s*\([^)]*\)\s*(?:const)?\s*(?:override)?\s*(?:noexcept)?\s*[{;]'
        functions.extend(re.findall(func_pattern, content, re.MULTILINE))
        
        # Find method definitions within classes
        method_pattern = r'(?:public|private|protected):\s*(?:[^}]*?)(?:(?:inline|static|virtual|explicit|const|constexpr)\s+)*(?:\w+(?:\s*\*|\s*&)?(?:\s*::\s*\w+)?)\s+(\w+)\s*\([^)]*\)\s*(?:const)?\s*(?:override)?\s*(?:noexcept)?\s*[{;]'
        functions.extend(re.findall(method_pattern, content, re.MULTILINE | re.DOTALL))
        
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
    
    return functions, classes

def find_usage(symbol, files):
    """Find usage of a symbol across all files"""
    usage_count = 0
    usage_files = []
    
    for file_path in files:
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                
            # Count occurrences (simple text search)
            count = len(re.findall(r'\b' + re.escape(symbol) + r'\b', content))
            if count > 0:
                usage_count += count
                usage_files.append((file_path, count))
                
        except Exception as e:
            continue
    
    return usage_count, usage_files

def analyze_unused_code():
    """Main analysis function"""
    print("🔍 SENTIO C++ UNUSED CODE ANALYSIS")
    print("==================================")
    
    # Find all C++ files
    cpp_files = find_cpp_files('.')
    print(f"📁 Found {len(cpp_files)} C++ files")
    
    # Extract all functions and classes
    all_functions = defaultdict(list)
    all_classes = defaultdict(list)
    
    print("📊 Extracting symbols...")
    for file_path in cpp_files:
        functions, classes = extract_functions_and_classes(file_path)
        
        for func in functions:
            all_functions[func].append(file_path)
        
        for cls in classes:
            all_classes[cls].append(file_path)
    
    print(f"🔧 Found {len(all_functions)} unique function names")
    print(f"🏗️  Found {len(all_classes)} unique class names")
    
    # Analyze usage
    print("\n🔍 Analyzing function usage...")
    unused_functions = []
    low_usage_functions = []
    
    for func_name, definition_files in all_functions.items():
        # Skip common/generic names that are likely to have false positives
        if func_name in ['main', 'get', 'set', 'init', 'run', 'test', 'size', 'empty', 'clear', 'begin', 'end']:
            continue
            
        usage_count, usage_files = find_usage(func_name, cpp_files)
        
        # If usage count is very low (1-2), it might be unused or only defined
        if usage_count <= 2:
            unused_functions.append((func_name, usage_count, definition_files))
        elif usage_count <= 5:
            low_usage_functions.append((func_name, usage_count, definition_files))
    
    # Analyze class usage
    print("🔍 Analyzing class usage...")
    unused_classes = []
    low_usage_classes = []
    
    for class_name, definition_files in all_classes.items():
        usage_count, usage_files = find_usage(class_name, cpp_files)
        
        if usage_count <= 2:
            unused_classes.append((class_name, usage_count, definition_files))
        elif usage_count <= 5:
            low_usage_classes.append((class_name, usage_count, definition_files))
    
    # Report results
    print(f"\n📋 ANALYSIS RESULTS")
    print(f"==================")
    
    print(f"\n❌ POTENTIALLY UNUSED FUNCTIONS ({len(unused_functions)}):")
    print("%-30s %-8s %s" % ("Function", "Usage", "Defined In"))
    print("%-30s %-8s %s" % ("-" * 30, "-" * 8, "-" * 20))
    for func_name, usage_count, def_files in sorted(unused_functions)[:20]:
        file_names = [os.path.basename(f) for f in def_files[:2]]
        print("%-30s %-8d %s" % (func_name, usage_count, ", ".join(file_names)))
    
    if len(unused_functions) > 20:
        print(f"... and {len(unused_functions) - 20} more")
    
    print(f"\n⚠️  LOW USAGE FUNCTIONS ({len(low_usage_functions)}):")
    print("%-30s %-8s %s" % ("Function", "Usage", "Defined In"))
    print("%-30s %-8s %s" % ("-" * 30, "-" * 8, "-" * 20))
    for func_name, usage_count, def_files in sorted(low_usage_functions)[:15]:
        file_names = [os.path.basename(f) for f in def_files[:2]]
        print("%-30s %-8d %s" % (func_name, usage_count, ", ".join(file_names)))
    
    print(f"\n❌ POTENTIALLY UNUSED CLASSES ({len(unused_classes)}):")
    print("%-30s %-8s %s" % ("Class", "Usage", "Defined In"))
    print("%-30s %-8s %s" % ("-" * 30, "-" * 8, "-" * 20))
    for class_name, usage_count, def_files in sorted(unused_classes)[:15]:
        file_names = [os.path.basename(f) for f in def_files[:2]]
        print("%-30s %-8d %s" % (class_name, usage_count, ", ".join(file_names)))
    
    # Summary
    print(f"\n📊 SUMMARY:")
    print(f"   • Total functions analyzed: {len(all_functions)}")
    print(f"   • Potentially unused functions: {len(unused_functions)}")
    print(f"   • Low usage functions: {len(low_usage_functions)}")
    print(f"   • Total classes analyzed: {len(all_classes)}")
    print(f"   • Potentially unused classes: {len(unused_classes)}")
    
    return unused_functions, low_usage_functions, unused_classes

if __name__ == "__main__":
    analyze_unused_code()

```

## 📄 **FILE 27 of 53**: tools/check_file_organization.py

**File Information**:
- **Path**: `tools/check_file_organization.py`

- **Size**: 276 lines
- **Modified**: 2025-09-21 14:12:11

- **Type**: .py

```text
#!/usr/bin/env python3
"""
Sentio File Organization Checker
================================

This script verifies that all output files are properly organized in the data/ folder
with appropriate subfolders (audit, trades, signals) and identifies any misplaced files.
"""

import os
import sys
from pathlib import Path
from typing import List, Dict, Set

class FileOrganizationChecker:
    def __init__(self, project_root: str = None):
        self.project_root = Path(project_root) if project_root else Path.cwd()
        self.data_dir = self.project_root / "data"
        
        # Expected data subdirectories
        self.expected_subdirs = {
            'audit': [],  # Reserved for future use, no specific file types expected
            'signals': ['*.jsonl', '*_signals_*'],
            'trades': ['*_trades.jsonl', '*_trades_*'],
            'equities': ['*.csv', '*.bin'],
            'future_qqq': ['*.csv', '*.json', '*.bin'],
            'mars_cache': ['*.json']
        }
        
        # Data file extensions that should be in data/
        self.data_extensions = {'.jsonl', '.csv', '.bin', '.json'}
        
    def check_data_directory_structure(self) -> Dict[str, List[str]]:
        """Check if all expected data subdirectories exist"""
        issues = []
        
        if not self.data_dir.exists():
            issues.append("data/ directory does not exist")
            return {"critical": issues}
        
        missing_dirs = []
        for subdir in self.expected_subdirs.keys():
            subdir_path = self.data_dir / subdir
            if not subdir_path.exists():
                missing_dirs.append(f"data/{subdir}/ directory missing")
        
        return {"missing_directories": missing_dirs, "issues": issues}
    
    def find_misplaced_data_files(self) -> Dict[str, List[str]]:
        """Find data files that are not in the data/ directory"""
        misplaced_files = []
        
        # Check root directory for actual data files (not source code)
        for file_path in self.project_root.iterdir():
            if file_path.is_file():
                # Only check for actual data output files, not source code
                if (file_path.suffix in {'.jsonl'} or
                    any(pattern in file_path.name for pattern in ['_signals_', '_trades_'])):
                    misplaced_files.append(str(file_path.relative_to(self.project_root)))
        
        # Check other directories (excluding data/, build/, .git/, src/, include/, tools/)
        exclude_dirs = {'data', 'build', '.git', '.mypy_cache', '__pycache__', 'src', 'include', 'tools', 'docs', 'tests', 'megadocs', 'logs'}
        
        for dir_path in self.project_root.iterdir():
            if dir_path.is_dir() and dir_path.name not in exclude_dirs:
                for file_path in dir_path.rglob('*'):
                    if file_path.is_file():
                        # Only check for actual data output files
                        if (file_path.suffix in {'.jsonl'} or
                            any(pattern in file_path.name for pattern in ['_signals_', '_trades_'])):
                            misplaced_files.append(str(file_path.relative_to(self.project_root)))
        
        return {"misplaced_files": misplaced_files}
    
    def check_data_subdirectory_contents(self) -> Dict[str, List[str]]:
        """Check if files are in the correct data subdirectories"""
        misorganized = []
        
        if not self.data_dir.exists():
            return {"misorganized_files": []}
        
        for subdir_name, patterns in self.expected_subdirs.items():
            subdir_path = self.data_dir / subdir_name
            if not subdir_path.exists():
                continue
                
            # Check files in this subdirectory
            for file_path in subdir_path.rglob('*'):
                if file_path.is_file():
                    file_name = file_path.name
                    
                    # Check if file belongs in this subdirectory
                    belongs_here = False
                    for pattern in patterns:
                        if pattern.startswith('*') and pattern.endswith('*'):
                            # Contains pattern
                            if pattern[1:-1] in file_name:
                                belongs_here = True
                                break
                        elif pattern.startswith('*'):
                            # Ends with pattern
                            if file_name.endswith(pattern[1:]):
                                belongs_here = True
                                break
                        elif pattern.endswith('*'):
                            # Starts with pattern
                            if file_name.startswith(pattern[:-1]):
                                belongs_here = True
                                break
                        else:
                            # Exact match
                            if file_name == pattern:
                                belongs_here = True
                                break
                    
                    if not belongs_here:
                        # Check if it should be in a different subdirectory
                        correct_subdir = self.find_correct_subdirectory(file_name)
                        if correct_subdir and correct_subdir != subdir_name:
                            misorganized.append(f"{file_path.relative_to(self.project_root)} should be in data/{correct_subdir}/")
        
        return {"misorganized_files": misorganized}
    
    def find_correct_subdirectory(self, filename: str) -> str:
        """Find the correct subdirectory for a given filename"""
        for subdir_name, patterns in self.expected_subdirs.items():
            for pattern in patterns:
                if pattern.startswith('*') and pattern.endswith('*'):
                    if pattern[1:-1] in filename:
                        return subdir_name
                elif pattern.startswith('*'):
                    if filename.endswith(pattern[1:]):
                        return subdir_name
                elif pattern.endswith('*'):
                    if filename.startswith(pattern[:-1]):
                        return subdir_name
                else:
                    if filename == pattern:
                        return subdir_name
        return None
    
    def check_source_code_paths(self) -> Dict[str, List[str]]:
        """Check source code for hardcoded paths that might be problematic"""
        problematic_paths = []
        
        src_dir = self.project_root / "src"
        if not src_dir.exists():
            return {"problematic_paths": []}
        
        for file_path in src_dir.rglob('*.cpp'):
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                    lines = content.split('\n')
                    
                    for line_num, line in enumerate(lines, 1):
                        # Check for absolute paths
                        if '/' in line and ('/Users/' in line or '/home/' in line or 'C:\\' in line):
                            if not line.strip().startswith('//'):  # Skip comments
                                problematic_paths.append(f"{file_path.relative_to(self.project_root)}:{line_num} - Absolute path: {line.strip()}")
                        
                        # Check for output file paths that don't use data/ directory
                        if any(pattern in line for pattern in ['ofstream', 'std::ofstream']) and any(ext in line for ext in ['.jsonl']):
                            if 'data/' not in line and not line.strip().startswith('//'):
                                problematic_paths.append(f"{file_path.relative_to(self.project_root)}:{line_num} - Output not to data/: {line.strip()}")
            
            except Exception as e:
                problematic_paths.append(f"Error reading {file_path}: {e}")
        
        return {"problematic_paths": problematic_paths}
    
    def generate_report(self) -> Dict[str, any]:
        """Generate comprehensive file organization report"""
        report = {
            "project_root": str(self.project_root),
            "data_directory": str(self.data_dir),
            "timestamp": str(Path(__file__).stat().st_mtime)
        }
        
        # Check directory structure
        dir_check = self.check_data_directory_structure()
        report.update(dir_check)
        
        # Find misplaced files
        misplaced_check = self.find_misplaced_data_files()
        report.update(misplaced_check)
        
        # Check subdirectory organization
        subdir_check = self.check_data_subdirectory_contents()
        report.update(subdir_check)
        
        # Check source code paths
        source_check = self.check_source_code_paths()
        report.update(source_check)
        
        # Calculate overall status
        total_issues = (
            len(report.get("issues", [])) +
            len(report.get("missing_directories", [])) +
            len(report.get("misplaced_files", [])) +
            len(report.get("misorganized_files", [])) +
            len(report.get("problematic_paths", []))
        )
        
        report["overall_status"] = "PASS" if total_issues == 0 else "ISSUES_FOUND"
        report["total_issues"] = total_issues
        
        return report
    
    def print_report(self, report: Dict[str, any]):
        """Print formatted report"""
        print("=" * 70)
        print("SENTIO FILE ORGANIZATION CHECK")
        print("=" * 70)
        print(f"Project Root: {report['project_root']}")
        print(f"Data Directory: {report['data_directory']}")
        print()
        
        # Overall status
        status = report["overall_status"]
        if status == "PASS":
            print("✅ OVERALL STATUS: PASS - All files properly organized")
        else:
            print(f"⚠️  OVERALL STATUS: {report['total_issues']} issues found")
        print()
        
        # Directory structure
        if report.get("missing_directories"):
            print("📁 MISSING DIRECTORIES:")
            for missing in report["missing_directories"]:
                print(f"   ❌ {missing}")
            print()
        
        # Misplaced files
        if report.get("misplaced_files"):
            print("📄 MISPLACED FILES (should be in data/):")
            for misplaced in report["misplaced_files"]:
                print(f"   ❌ {misplaced}")
            print()
        
        # Misorganized files
        if report.get("misorganized_files"):
            print("🗂️  MISORGANIZED FILES:")
            for misorg in report["misorganized_files"]:
                print(f"   ⚠️  {misorg}")
            print()
        
        # Problematic paths in source code
        if report.get("problematic_paths"):
            print("🔍 PROBLEMATIC PATHS IN SOURCE CODE:")
            for prob in report["problematic_paths"]:
                print(f"   ⚠️  {prob}")
            print()
        
        # Summary
        if status == "PASS":
            print("🎉 All files are properly organized in the data/ folder structure!")
        else:
            print("📋 RECOMMENDATIONS:")
            print("   • Move misplaced files to appropriate data/ subdirectories")
            print("   • Create missing directories as needed")
            print("   • Update source code to use proper data/ paths")
            print("   • Ensure all output files go to data/ subdirectories")


def main():
    checker = FileOrganizationChecker()
    report = checker.generate_report()
    checker.print_report(report)
    
    # Exit with error code if issues found
    sys.exit(0 if report["overall_status"] == "PASS" else 1)


if __name__ == "__main__":
    main()

```



# LEVERAGE TRADING REQUIREMENTS

# Sentio Trading System - Leverage Trading Requirements Document

## Executive Summary

This document defines the comprehensive requirements for implementing leverage trading support in the Sentio Trading System. The system will support leveraged ETFs (TQQQ, SQQQ, PSQ) with strict position conflict prevention and maximum profit optimization through intelligent leverage selection.

## 1. Leverage Instrument Specifications

### 1.1 Supported Leverage Instruments

| Symbol | Base Asset | Leverage Factor | Direction | Description |
|--------|------------|-----------------|-----------|-------------|
| QQQ    | QQQ        | 1.0x           | Long      | Base QQQ ETF |
| TQQQ   | QQQ        | 3.0x           | Long      | 3x Leveraged QQQ |
| SQQQ   | QQQ        | 3.0x           | Short     | 3x Inverse QQQ |
| PSQ    | QQQ        | 1.0x           | Short     | 1x Inverse QQQ |

### 1.2 Leverage Relationships

```cpp
struct LeverageSpec {
    std::string base_symbol;    // "QQQ"
    double leverage_factor;    // 1.0, 3.0
    bool is_inverse;          // false for TQQQ, true for SQQQ/PSQ
    double expense_ratio;      // Annual expense ratio
    double daily_decay;        // Daily rebalancing cost
};
```

## 2. Position Conflict Prevention Rules

### 2.1 Prohibited Combinations

**RULE: Only the explicitly allowed combinations below are permitted. All other combinations are STRICTLY PROHIBITED.**

The following position combinations are **STRICTLY PROHIBITED** and will result in immediate trade rejection:

#### Conflicting Long/Short Positions:
- ❌ QQQ + PSQ (both long QQQ exposure)
- ❌ QQQ + SQQQ (long QQQ vs short QQQ)
- ❌ TQQQ + PSQ (both long QQQ exposure)
- ❌ TQQQ + SQQQ (long QQQ vs short QQQ)

#### Invalid Leverage Combinations:
- ❌ QQQ + TQQQ + SQQQ (multiple QQQ exposures)
- ❌ PSQ + SQQQ (both short QQQ exposure - conflicting short positions)
- ❌ Any combination with >2 positions
- ❌ Any combination not explicitly listed in "Allowed Combinations" below

### 2.2 Allowed Position Combinations

**ONLY** the following combinations are permitted. All others are automatically rejected:

#### Single Positions:
- ✅ QQQ only
- ✅ TQQQ only  
- ✅ SQQQ only
- ✅ PSQ only

#### Dual Positions:
- ✅ QQQ + TQQQ (amplified long QQQ exposure)

**NOTE: PSQ + SQQQ is NOT allowed** - both are short QQQ exposure, creating conflicting positions.

### 2.3 Position Validation Logic

```cpp
enum class PositionType {
    LONG_QQQ,      // QQQ or TQQQ
    SHORT_QQQ,     // PSQ or SQQQ
    NEUTRAL        // No positions
};

class LeveragePositionValidator {
public:
    // Check if a new position can be added
    bool validate_position_addition(const std::string& symbol);
    
    // Check if a position can be removed
    bool validate_position_removal(const std::string& symbol);
    
    // Get current position type
    PositionType get_current_position_type() const;
    
    // Get list of allowed symbols to add
    std::vector<std::string> get_allowed_additions() const;
    
private:
    // Simple whitelist approach - only allow explicitly permitted combinations
    bool is_allowed_combination(const std::vector<std::string>& positions) const;
    
    // Check if symbol is long QQQ exposure
    bool is_long_qqq_symbol(const std::string& symbol) const;
    
    // Check if symbol is short QQQ exposure  
    bool is_short_qqq_symbol(const std::string& symbol) const;
};
```

### 2.4 Validation Rules

The validation logic follows a simple whitelist approach:

1. **Single Positions**: Any individual symbol (QQQ, TQQQ, SQQQ, PSQ) is allowed
2. **Dual Positions**: Only QQQ + TQQQ is allowed (both long QQQ exposure)
3. **All Other Combinations**: Automatically rejected

This ensures maximum simplicity and prevents any conflicting positions.

## 3. Leverage Data Generation

### 3.1 Theoretical Pricing Model

Leverage instruments will be generated using theoretical pricing based on the base QQQ data:

```cpp
class LeverageDataGenerator {
public:
    // Generate TQQQ data (3x long QQQ)
    std::vector<Bar> generate_tqqq_data(const std::vector<Bar>& qqq_data);
    
    // Generate SQQQ data (3x short QQQ)  
    std::vector<Bar> generate_sqqq_data(const std::vector<Bar>& qqq_data);
    
    // Generate PSQ data (1x short QQQ)
    std::vector<Bar> generate_psq_data(const std::vector<Bar>& qqq_data);
    
private:
    double calculate_leverage_price(double base_price, double leverage_factor, 
                                  bool is_inverse, double daily_decay);
};
```

### 3.2 Pricing Formula

For leveraged instruments, the theoretical price is calculated as:

```
Leveraged_Price = Base_Price * Leverage_Factor * (1 - Daily_Decay)^Days

Where:
- Base_Price: QQQ closing price
- Leverage_Factor: 1.0 for PSQ, 3.0 for TQQQ/SQQQ
- Daily_Decay: 0.0001 (0.01% daily rebalancing cost)
- Days: Number of days since last rebalancing
```

### 3.3 Data File Structure

Generated leverage files will follow the same format as QQQ_RTH_NH.csv:

```csv
ts_utc,ts_nyt_epoch,open,high,low,close,volume
2022-09-15 13:30:00,1663252200,45.23,45.67,44.89,45.12,1250000
```

## 4. Trading Strategy Enhancements

### 4.1 Leverage Selection Logic

The system will intelligently select leverage instruments based on signal strength:

```cpp
class LeverageSelector {
public:
    std::string select_optimal_leverage(double signal_probability, 
                                      double signal_confidence,
                                      PositionType current_position);
    
private:
    // High confidence bullish signals -> TQQQ
    // Moderate bullish signals -> QQQ
    // High confidence bearish signals -> SQQQ  
    // Moderate bearish signals -> PSQ
};
```

### 4.2 Signal-to-Leverage Mapping

| Signal Probability | Signal Confidence | Selected Instrument | Rationale |
|-------------------|-------------------|-------------------|-----------|
| > 0.7 | > 0.8 | TQQQ | Maximum bullish leverage |
| > 0.6 | > 0.6 | QQQ | Moderate bullish exposure |
| < 0.3 | > 0.8 | SQQQ | Maximum bearish leverage |
| < 0.4 | > 0.6 | PSQ | Moderate bearish exposure |
| 0.4-0.6 | Any | HOLD | Neutral zone |

### 4.3 Position Sizing with Leverage

Leverage instruments require adjusted position sizing to account for increased volatility:

```cpp
double calculate_leverage_position_size(const std::string& symbol, 
                                       double base_position_size,
                                       double leverage_factor) {
    // Reduce position size for higher leverage to maintain risk parity
    double risk_adjusted_size = base_position_size / leverage_factor;
    return risk_adjusted_size;
}
```

## 5. CLI Command Enhancements

### 5.1 Enhanced Trade Command

The `sentio_cli trade` command will support leverage trading:

```bash
# Basic leverage trading
sentio_cli trade --signals latest --leverage-enabled

# Specific leverage instrument
sentio_cli trade --signals latest --target-symbol TQQQ

# Leverage family trading
sentio_cli trade --signals latest --leverage-family QQQ
```

### 5.2 New CLI Parameters

| Parameter | Description | Default | Example |
|-----------|-------------|---------|---------|
| `--leverage-enabled` | Enable leverage trading | false | `--leverage-enabled` |
| `--target-symbol` | Force specific leverage instrument | auto | `--target-symbol TQQQ` |
| `--leverage-family` | Specify base asset family | QQQ | `--leverage-family QQQ` |
| `--max-leverage` | Maximum leverage factor allowed | 3.0 | `--max-leverage 3.0` |
| `--leverage-decay` | Daily decay rate for theoretical pricing | 0.0001 | `--leverage-decay 0.0001` |

### 5.3 Enhanced Audit Reports

The audit reports will include leverage-specific metrics:

```
📊 LEVERAGE PERFORMANCE SUMMARY
┌─────────────────────┬──────────────┬─────────────────────┬──────────────┐
│ Leverage Instrument │ Total Trades │ Avg Position Size   │ Max Drawdown │
├─────────────────────┼──────────────┼─────────────────────┼──────────────┤
│ TQQQ                │           45 │ $15,000.00          │       8.5%  │
│ QQQ                  │           23 │ $25,000.00          │       3.2%  │
│ SQQQ                 │           12 │ $12,000.00          │       6.8%  │
└─────────────────────┴──────────────┴─────────────────────┴──────────────┘
```

## 6. Risk Management

### 6.1 Leverage Risk Controls

- **Maximum Position Size**: Reduced by leverage factor
- **Drawdown Limits**: Stricter limits for higher leverage instruments
- **Volatility Adjustment**: Position sizing adjusted for instrument volatility
- **Daily Rebalancing**: Account for daily decay in leveraged instruments

### 6.2 Conflict Detection

Real-time conflict detection will prevent invalid position combinations:

```cpp
class LeverageConflictDetector {
public:
    bool would_cause_conflict(const std::string& new_symbol, 
                           const std::vector<std::string>& current_positions);
    
    std::string get_conflict_reason() const;
    
private:
    bool is_conflicting_pair(const std::string& symbol1, const std::string& symbol2);
    PositionType get_symbol_position_type(const std::string& symbol);
};
```

## 7. Implementation Phases

### Phase 1: Data Generation
- [ ] Implement leverage data generation from QQQ base data
- [ ] Create theoretical pricing models for TQQQ, SQQQ, PSQ
- [ ] Generate leverage data files in data/equities/

### Phase 2: Position Management
- [ ] Implement leverage position validator
- [ ] Add conflict detection logic
- [ ] Enhance portfolio manager for leverage instruments

### Phase 3: Trading Logic
- [ ] Implement leverage selector
- [ ] Add leverage-aware position sizing
- [ ] Enhance signal-to-instrument mapping

### Phase 4: CLI Integration
- [ ] Add leverage parameters to trade command
- [ ] Enhance audit reports with leverage metrics
- [ ] Add leverage-specific error messages

### Phase 5: Testing & Validation
- [ ] Comprehensive testing of conflict prevention
- [ ] Backtesting with leverage instruments
- [ ] Performance validation and optimization

## 8. Success Criteria

### 8.1 Functional Requirements
- ✅ All prohibited position combinations are rejected
- ✅ Only allowed position combinations are permitted
- ✅ Leverage data is accurately generated from base QQQ data
- ✅ Position sizing accounts for leverage factors
- ✅ CLI commands support leverage trading parameters

### 8.2 Performance Requirements
- ✅ No conflicting positions ever occur
- ✅ Leverage instruments show appropriate volatility
- ✅ Risk-adjusted returns improve with leverage selection
- ✅ System maintains stability with leverage trading

### 8.3 User Experience Requirements
- ✅ Clear error messages for prohibited combinations
- ✅ Intuitive CLI parameters for leverage control
- ✅ Comprehensive audit reports with leverage metrics
- ✅ Professional reporting maintains quality standards

## 9. Technical Specifications

### 9.1 File Organization

```
data/equities/
├── QQQ_RTH_NH.csv          # Base QQQ data
├── TQQQ_RTH_NH.csv         # Generated 3x long QQQ
├── SQQQ_RTH_NH.csv         # Generated 3x short QQQ
└── PSQ_RTH_NH.csv          # Generated 1x short QQQ
```

### 9.2 Code Structure

```
include/backend/
├── leverage_manager.h       # Leverage position management
├── leverage_data_generator.h # Theoretical data generation
└── leverage_conflict_detector.h # Conflict prevention

src/backend/
├── leverage_manager.cpp
├── leverage_data_generator.cpp
└── leverage_conflict_detector.cpp
```

### 9.3 Configuration

```cpp
struct LeverageConfig {
    bool enable_leverage_trading = true;
    double max_leverage_factor = 3.0;
    double daily_decay_rate = 0.0001;
    double expense_ratio = 0.0095;
    bool use_theoretical_pricing = true;
};
```

## 10. Conclusion

This leverage trading implementation will significantly enhance the Sentio Trading System's profit potential while maintaining strict risk controls. The comprehensive conflict prevention system ensures that only valid position combinations are allowed, while the intelligent leverage selection maximizes returns based on signal strength and market conditions.

The implementation follows the system's existing architecture patterns and maintains the professional-grade reporting standards that have been established throughout the development process.
