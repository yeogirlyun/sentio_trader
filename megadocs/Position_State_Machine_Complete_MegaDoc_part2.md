# Position State Machine Complete Implementation

**Part 2 of 8**

**Generated**: 2025-09-22 01:45:56
**Source Directory**: /Users/yeogirlyun/C++/sentio_trader
**Description**: Comprehensive requirements document and source code analysis for implementing a theoretically sound Position State Machine to replace ad-hoc trading logic with systematic state-based decision making. Includes complete requirements specification, current system analysis, and integration architecture.

**Part 2 Files**: See file count below

---

## 📋 **TABLE OF CONTENTS**

6. [include/backend/leverage_manager.h](#file-6)
7. [include/backend/leverage_types.h](#file-7)
8. [include/backend/leverage_utils.h](#file-8)
9. [include/backend/portfolio_manager.h](#file-9)
10. [include/common/config.h](#file-10)

---

## 📄 **FILE 6 of 39**: include/backend/leverage_manager.h

**File Information**:
- **Path**: `include/backend/leverage_manager.h`

- **Size**: 144 lines
- **Modified**: 2025-09-21 20:49:05

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: backend/leverage_manager.h
// Purpose: Implements leverage trading rules, including position conflict
//          prevention and intelligent instrument selection.
//
// Core Architecture & Design:
// This module encapsulates the strict position conflict prevention rules from
// the Leverage Trading Requirements document. It provides a whitelist validator
// for allowed position combinations and implements the corrected directional
// conflict logic.
//
// Key Design Principles:
// - Strict whitelist approach: Only explicitly allowed combinations permitted
// - Directional conflict prevention: Prevents long + short combinations
// - Flexible allocation: Allows multiple instruments on same side of market
// - Clear error messages: Provides specific reasons for conflicts
//
// Recent Improvements:
// - Corrected conflict logic based on directional analysis
// - PSQ + SQQQ now allowed (both provide short QQQ exposure)
// - Clear separation between allocation decisions and directional conflicts
// =============================================================================

#include "common/types.h"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace sentio {

/// Leverage position validator implementing strict conflict prevention rules
/// 
/// This class implements the corrected leverage trading logic that prevents
/// directional conflicts while allowing flexible allocation decisions.
/// 
/// CORE RULE: A conflict exists if, and only if, the portfolio holds both
/// a LONG instrument (QQQ/TQQQ) and a SHORT/INVERSE instrument (PSQ/SQQQ)
/// at the same time.
class LeveragePositionValidator {
public:
    /// Constructor - initializes with leverage specifications and allowed combinations
    LeveragePositionValidator();
    
    /// Checks if adding a new symbol would violate conflict rules
    /// 
    /// This method implements the strict whitelist approach from the requirements.
    /// It checks the proposed portfolio state against the allowed combinations
    /// and provides specific conflict reasons.
    /// 
    /// @param new_symbol The symbol of the proposed trade
    /// @param current_positions A map of current positions held
    /// @return A reason string if it conflicts, otherwise an empty string
    std::string would_cause_conflict(const std::string& new_symbol,
                                   const std::map<std::string, Position>& current_positions) const;
    
    /// Get all allowed position combinations
    /// @return Set of allowed position combinations
    const std::set<std::set<std::string>>& get_allowed_combinations() const {
        return allowed_combinations_;
    }
    
    /// Check if a symbol is a leverage instrument
    /// @param symbol The symbol to check
    /// @return true if the symbol is a known leverage instrument
    bool is_leverage_instrument(const std::string& symbol) const;

private:
    /// Returns true if a symbol has long QQQ exposure (QQQ, TQQQ)
    bool is_long_qqq(const std::string& symbol) const;
    
    /// Returns true if a symbol has short QQQ exposure (SQQQ, PSQ)
    bool is_short_qqq(const std::string& symbol) const;
    
    /// Get current active symbols with non-zero quantity
    std::set<std::string> get_active_symbols(const std::map<std::string, Position>& positions) const;
    
    /// Check if a combination is in the allowed whitelist
    bool is_allowed_combination(const std::set<std::string>& symbols) const;
    
    /// Generate specific conflict reason based on symbol types
    std::string generate_conflict_reason(const std::string& new_symbol, 
                                       const std::set<std::string>& active_symbols) const;
    
    // Leverage instrument specifications
    std::set<std::string> long_symbols_;      // QQQ, TQQQ
    std::set<std::string> short_symbols_;     // SQQQ, PSQ
    
    // Allowed position combinations (whitelist approach)
    std::set<std::set<std::string>> allowed_combinations_;
};

/// Leverage instrument selector for intelligent instrument selection
/// 
/// This class implements the logic for selecting the optimal leverage
/// instrument based on signal strength and market conditions.
class LeverageSelector {
public:
    /// Select the optimal leverage instrument based on signal characteristics
    /// 
    /// This method implements the instrument selection logic from the requirements:
    /// - High confidence bullish signals -> TQQQ (maximum leverage)
    /// - Moderate bullish signals -> QQQ (moderate exposure)
    /// - High confidence bearish signals -> SQQQ (maximum inverse leverage)
    /// - Moderate bearish signals -> PSQ (moderate inverse exposure)
    /// - Neutral zone or low confidence -> HOLD
    /// 
    /// @param probability Signal probability (0.0 to 1.0)
    /// @param confidence Signal confidence (0.0 to 1.0)
    /// @return Recommended symbol for the trade, or "HOLD" if no recommendation
    std::string select_optimal_instrument(double probability, double confidence) const;
    
    /// Calculate risk-adjusted position size based on leverage factor
    /// 
    /// This method implements risk parity by reducing position size for higher
    /// leverage instruments to maintain consistent risk exposure.
    /// 
    /// @param symbol The leverage instrument symbol
    /// @param base_position_size Base position size for 1x leverage
    /// @return Risk-adjusted position size
    double calculate_risk_adjusted_size(const std::string& symbol, double base_position_size) const;
    
    /// Get leverage factor for a symbol
    /// @param symbol The instrument symbol
    /// @return Leverage factor (1.0 for QQQ/PSQ, 3.0 for TQQQ/SQQQ)
    double get_leverage_factor(const std::string& symbol) const;

private:
    /// Validate signal parameters
    void validate_signal_params(double probability, double confidence) const;
    
    /// Check if signal is in bullish zone
    bool is_bullish_signal(double probability, double confidence) const;
    
    /// Check if signal is in bearish zone
    bool is_bearish_signal(double probability, double confidence) const;
    
    /// Check if signal is in neutral zone
    bool is_neutral_signal(double probability, double confidence) const;
};

} // namespace sentio

```

## 📄 **FILE 7 of 39**: include/backend/leverage_types.h

**File Information**:
- **Path**: `include/backend/leverage_types.h`

- **Size**: 169 lines
- **Modified**: 2025-09-21 16:48:54

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: backend/leverage_types.h
// Purpose: Centralized types and specifications for leverage trading
//
// Core Architecture & Design:
// This module provides the fundamental types and specifications for leverage
// trading instruments. It centralizes all leverage-related definitions to
// ensure consistency across the system and prevent logical inconsistencies.
//
// Key Design Principles:
// - Clear classification of instruments by market direction and leverage
// - Centralized specifications to prevent scattered definitions
// - Type-safe enums to prevent invalid instrument combinations
// - Extensible design for future leverage instruments
//
// Recent Improvements:
// - Corrected conflict logic based on directional analysis
// - Clear separation between long and inverse instruments
// - Robust type system to prevent logical errors
// =============================================================================

#include <string>
#include <map>

namespace sentio {

/// Enum to classify instruments by their market direction and leverage
/// 
/// This classification is crucial for conflict detection. The system prevents
/// holding both LONG and INVERSE instruments simultaneously, as this creates
/// directional conflicts that drain capital through fees.
enum class InstrumentType {
    NEUTRAL,        // Unknown or non-leverage instruments
    LONG_1X,        // QQQ - 1x long QQQ exposure
    LONG_3X,        // TQQQ - 3x long QQQ exposure  
    INVERSE_1X,     // PSQ - 1x short QQQ exposure
    INVERSE_3X      // SQQQ - 3x short QQQ exposure
};

/// Holds the specifications for a single leverage instrument
/// 
/// This structure contains all the essential information needed to:
/// 1. Classify the instrument for conflict detection
/// 2. Generate theoretical pricing data
/// 3. Calculate position sizing and risk metrics
struct LeverageSpec {
    std::string symbol;           // Instrument symbol (e.g., "TQQQ")
    std::string base_symbol;      // Base asset symbol (e.g., "QQQ")
    double leverage_factor;       // Leverage multiplier (1.0, 3.0)
    bool is_inverse;              // true for short/inverse instruments
    InstrumentType type;          // Classification for conflict detection
    
    /// Default constructor for unknown instruments
    LeverageSpec() : symbol(""), base_symbol(""), leverage_factor(1.0), 
                     is_inverse(false), type(InstrumentType::NEUTRAL) {}
    
    /// Constructor for known leverage instruments
    LeverageSpec(const std::string& sym, const std::string& base, 
                 double factor, bool inverse, InstrumentType inst_type)
        : symbol(sym), base_symbol(base), leverage_factor(factor),
          is_inverse(inverse), type(inst_type) {}
};

/// Registry of all supported leverage instruments
/// 
/// This registry provides centralized access to leverage specifications
/// and ensures consistent behavior across the system.
class LeverageRegistry {
public:
    /// Get the singleton instance
    static LeverageRegistry& instance() {
        static LeverageRegistry registry;
        return registry;
    }
    
    /// Get specification for a symbol
    /// @param symbol The instrument symbol to look up
    /// @return LeverageSpec for the symbol, or default spec if not found
    LeverageSpec get_spec(const std::string& symbol) const {
        auto it = specs_.find(symbol);
        if (it != specs_.end()) {
            return it->second;
        }
        return LeverageSpec(); // Return default (NEUTRAL) spec
    }
    
    /// Check if a symbol is a leverage instrument
    /// @param symbol The instrument symbol to check
    /// @return true if the symbol is a known leverage instrument
    bool is_leverage_instrument(const std::string& symbol) const {
        return specs_.find(symbol) != specs_.end();
    }
    
    /// Get all supported symbols
    /// @return Vector of all supported leverage instrument symbols
    std::vector<std::string> get_all_symbols() const {
        std::vector<std::string> symbols;
        for (const auto& [symbol, spec] : specs_) {
            symbols.push_back(symbol);
        }
        return symbols;
    }

private:
    std::map<std::string, LeverageSpec> specs_;
    
    /// Private constructor - initialize with default specifications
    LeverageRegistry() {
        // QQQ Family - Long Instruments
        specs_["QQQ"] = LeverageSpec("QQQ", "QQQ", 1.0, false, InstrumentType::LONG_1X);
        specs_["TQQQ"] = LeverageSpec("TQQQ", "QQQ", 3.0, false, InstrumentType::LONG_3X);
        
        // QQQ Family - Inverse Instruments  
        specs_["PSQ"] = LeverageSpec("PSQ", "QQQ", 1.0, true, InstrumentType::INVERSE_1X);
        specs_["SQQQ"] = LeverageSpec("SQQQ", "QQQ", 3.0, true, InstrumentType::INVERSE_3X);
        
        // Future: SPY Family
        // specs_["SPY"] = LeverageSpec("SPY", "SPY", 1.0, false, InstrumentType::LONG_1X);
        // specs_["UPRO"] = LeverageSpec("UPRO", "SPY", 3.0, false, InstrumentType::LONG_3X);
        // specs_["SPXU"] = LeverageSpec("SPXU", "SPY", 3.0, true, InstrumentType::INVERSE_3X);
    }
};

/// Convenience functions for instrument classification
namespace leverage_utils {

/// Check if an instrument type represents long exposure
/// @param type The instrument type to check
/// @return true if the type represents long market exposure
inline bool is_long_exposure(InstrumentType type) {
    return type == InstrumentType::LONG_1X || type == InstrumentType::LONG_3X;
}

/// Check if an instrument type represents short/inverse exposure
/// @param type The instrument type to check
/// @return true if the type represents short market exposure
inline bool is_short_exposure(InstrumentType type) {
    return type == InstrumentType::INVERSE_1X || type == InstrumentType::INVERSE_3X;
}

/// Check if two instrument types would create a directional conflict
/// @param type1 First instrument type
/// @param type2 Second instrument type
/// @return true if holding both types would create a conflict
inline bool would_create_conflict(InstrumentType type1, InstrumentType type2) {
    // A conflict exists if one is long and the other is short
    return (is_long_exposure(type1) && is_short_exposure(type2)) ||
           (is_short_exposure(type1) && is_long_exposure(type2));
}

/// Get a human-readable description of an instrument type
/// @param type The instrument type
/// @return String description of the type
inline std::string get_type_description(InstrumentType type) {
    switch (type) {
        case InstrumentType::NEUTRAL: return "Unknown/Non-leverage";
        case InstrumentType::LONG_1X: return "1x Long";
        case InstrumentType::LONG_3X: return "3x Long";
        case InstrumentType::INVERSE_1X: return "1x Short/Inverse";
        case InstrumentType::INVERSE_3X: return "3x Short/Inverse";
        default: return "Invalid Type";
    }
}

} // namespace leverage_utils

} // namespace sentio

```

## 📄 **FILE 8 of 39**: include/backend/leverage_utils.h

**File Information**:
- **Path**: `include/backend/leverage_utils.h`

- **Size**: 271 lines
- **Modified**: 2025-09-21 16:49:30

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: backend/leverage_utils.h
// Purpose: Core validation and classification logic for leverage trading
//
// Core Architecture & Design:
// This module provides the core logic for leverage instrument validation and
// conflict detection. It implements the corrected directional conflict rules
// that prevent capital drain from opposing leveraged positions.
//
// Key Design Principles:
// - Directional conflict detection: Prevent long + short combinations
// - Flexible allocation: Allow multiple instruments on same side of market
// - Robust validation: Clear, logical rules that prevent errors
// - Performance optimized: Fast lookup and validation
//
// Recent Improvements:
// - Corrected conflict logic based on directional analysis
// - Simplified rules: conflict only when holding long + short simultaneously
// - Clear separation between allocation decisions and directional conflicts
// =============================================================================

#include "leverage_types.h"
#include "common/types.h"
#include <vector>
#include <string>
#include <map>
#include <stdexcept>

namespace sentio {

/// Core leverage manager for position validation and conflict detection
/// 
/// This class implements the corrected leverage trading logic that prevents
/// directional conflicts while allowing flexible allocation decisions.
class LeverageManager {
public:
    /// Constructor - initializes with default leverage specifications
    LeverageManager() = default;
    
    /// Classify a symbol into an InstrumentType
    /// @param symbol The instrument symbol (e.g., "TQQQ")
    /// @return The corresponding InstrumentType
    InstrumentType classify(const std::string& symbol) const {
        auto spec = LeverageRegistry::instance().get_spec(symbol);
        return spec.type;
    }
    
    /// Determine if adding a new symbol would create a directional conflict
    /// 
    /// CORE RULE: A conflict exists if, and only if, the portfolio holds both
    /// a LONG instrument (QQQ/TQQQ) and a SHORT/INVERSE instrument (PSQ/SQQQ)
    /// at the same time.
    /// 
    /// @param new_symbol The symbol of the new position to be added
    /// @param current_positions Map of currently held positions (symbol -> Position)
    /// @return True if a conflict would be created, false otherwise
    bool would_cause_conflict(const std::string& new_symbol, 
                            const std::map<std::string, Position>& current_positions) const {
        
        InstrumentType new_type = classify(new_symbol);
        
        // If the new symbol is not a leverage instrument, no conflict possible
        if (new_type == InstrumentType::NEUTRAL) {
            return false;
        }
        
        bool new_is_long = leverage_utils::is_long_exposure(new_type);
        bool new_is_short = leverage_utils::is_short_exposure(new_type);
        
        // Check against all existing positions
        for (const auto& [symbol, position] : current_positions) {
            // Skip positions with zero quantity
            if (std::abs(position.quantity) < 1e-9) continue;
            
            InstrumentType existing_type = classify(symbol);
            
            // Skip non-leverage instruments
            if (existing_type == InstrumentType::NEUTRAL) continue;
            
            bool existing_is_long = leverage_utils::is_long_exposure(existing_type);
            bool existing_is_short = leverage_utils::is_short_exposure(existing_type);
            
            // THE CORE RULE: A conflict exists if adding a LONG to a SHORT, 
            // or a SHORT to a LONG
            if ((new_is_long && existing_is_short) || (new_is_short && existing_is_long)) {
                return true;
            }
        }
        
        return false;
    }
    
    /// Get the current position type based on existing positions
    /// @param current_positions Map of current positions
    /// @return The dominant position type, or NEUTRAL if no positions
    InstrumentType get_current_position_type(const std::map<std::string, Position>& current_positions) const {
        bool has_long = false;
        bool has_short = false;
        
        for (const auto& [symbol, position] : current_positions) {
            if (std::abs(position.quantity) < 1e-9) continue;
            
            InstrumentType type = classify(symbol);
            if (leverage_utils::is_long_exposure(type)) {
                has_long = true;
            } else if (leverage_utils::is_short_exposure(type)) {
                has_short = true;
            }
        }
        
        // Return the dominant type (this should never be both due to conflict prevention)
        if (has_long && has_short) {
            // This should never happen due to conflict prevention, but handle gracefully
            return InstrumentType::NEUTRAL;
        } else if (has_long) {
            return InstrumentType::LONG_1X; // Representative of long exposure
        } else if (has_short) {
            return InstrumentType::INVERSE_1X; // Representative of short exposure
        }
        
        return InstrumentType::NEUTRAL;
    }
    
    /// Get list of symbols that can be added without conflicts
    /// @param current_positions Map of current positions
    /// @return Vector of symbols that can be safely added
    std::vector<std::string> get_allowed_additions(const std::map<std::string, Position>& current_positions) const {
        std::vector<std::string> allowed;
        InstrumentType current_type = get_current_position_type(current_positions);
        
        // Get all available leverage instruments
        auto all_symbols = LeverageRegistry::instance().get_all_symbols();
        
        for (const auto& symbol : all_symbols) {
            // Skip if already holding this symbol
            if (current_positions.find(symbol) != current_positions.end()) {
                continue;
            }
            
            // Check if adding this symbol would cause a conflict
            if (!would_cause_conflict(symbol, current_positions)) {
                allowed.push_back(symbol);
            }
        }
        
        return allowed;
    }
    
    /// Get a human-readable conflict reason
    /// @param new_symbol The symbol that would cause conflict
    /// @param current_positions Current positions
    /// @return String describing why the conflict would occur
    std::string get_conflict_reason(const std::string& new_symbol, 
                                  const std::map<std::string, Position>& current_positions) const {
        InstrumentType new_type = classify(new_symbol);
        
        if (new_type == InstrumentType::NEUTRAL) {
            return "Symbol is not a leverage instrument";
        }
        
        bool new_is_long = leverage_utils::is_long_exposure(new_type);
        bool new_is_short = leverage_utils::is_short_exposure(new_type);
        
        std::string conflicting_symbols;
        for (const auto& [symbol, position] : current_positions) {
            if (std::abs(position.quantity) < 1e-9) continue;
            
            InstrumentType existing_type = classify(symbol);
            if (existing_type == InstrumentType::NEUTRAL) continue;
            
            bool existing_is_long = leverage_utils::is_long_exposure(existing_type);
            bool existing_is_short = leverage_utils::is_short_exposure(existing_type);
            
            if ((new_is_long && existing_is_short) || (new_is_short && existing_is_long)) {
                if (!conflicting_symbols.empty()) conflicting_symbols += ", ";
                conflicting_symbols += symbol;
            }
        }
        
        if (!conflicting_symbols.empty()) {
            std::string new_direction = new_is_long ? "long" : "short";
            return "Adding " + new_symbol + " (" + new_direction + " exposure) would conflict with existing " + conflicting_symbols;
        }
        
        return "No conflict detected";
    }
    
    /// Validate a complete position combination
    /// @param positions Map of positions to validate
    /// @return True if the combination is valid, false otherwise
    bool validate_position_combination(const std::map<std::string, Position>& positions) const {
        bool has_long = false;
        bool has_short = false;
        
        for (const auto& [symbol, position] : positions) {
            if (std::abs(position.quantity) < 1e-9) continue;
            
            InstrumentType type = classify(symbol);
            if (leverage_utils::is_long_exposure(type)) {
                has_long = true;
            } else if (leverage_utils::is_short_exposure(type)) {
                has_short = true;
            }
        }
        
        // Valid combination: either all long, all short, or neutral
        return !(has_long && has_short);
    }
};

/// Leverage selector for intelligent instrument selection
/// 
/// This class implements the logic for selecting the optimal leverage
/// instrument based on signal strength and current market conditions.
class LeverageSelector {
public:
    /// Select the optimal leverage instrument based on signal characteristics
    /// @param signal_probability Signal probability (0.0 to 1.0)
    /// @param signal_confidence Signal confidence (0.0 to 1.0)
    /// @param current_position_type Current dominant position type
    /// @return Recommended symbol for the trade
    std::string select_optimal_leverage(double signal_probability, 
                                      double signal_confidence,
                                      InstrumentType current_position_type) const {
        
        // Validate inputs
        if (signal_probability < 0.0 || signal_probability > 1.0) {
            throw std::invalid_argument("Signal probability must be between 0.0 and 1.0");
        }
        if (signal_confidence < 0.0 || signal_confidence > 1.0) {
            throw std::invalid_argument("Signal confidence must be between 0.0 and 1.0");
        }
        
        // High confidence bullish signals -> TQQQ (maximum leverage)
        if (signal_probability > 0.7 && signal_confidence > 0.8) {
            return "TQQQ";
        }
        
        // Moderate bullish signals -> QQQ (moderate exposure)
        if (signal_probability > 0.6 && signal_confidence > 0.6) {
            return "QQQ";
        }
        
        // High confidence bearish signals -> SQQQ (maximum inverse leverage)
        if (signal_probability < 0.3 && signal_confidence > 0.8) {
            return "SQQQ";
        }
        
        // Moderate bearish signals -> PSQ (moderate inverse exposure)
        if (signal_probability < 0.4 && signal_confidence > 0.6) {
            return "PSQ";
        }
        
        // Neutral zone or low confidence -> no recommendation
        return ""; // Indicates HOLD
    }
    
    /// Get recommended position size based on leverage factor
    /// @param base_position_size Base position size for 1x leverage
    /// @param leverage_factor Leverage factor of the instrument
    /// @return Risk-adjusted position size
    double calculate_leverage_position_size(double base_position_size, double leverage_factor) const {
        // Reduce position size for higher leverage to maintain risk parity
        // This prevents over-leveraging and maintains consistent risk exposure
        return base_position_size / leverage_factor;
    }
};

} // namespace sentio

```

## 📄 **FILE 9 of 39**: include/backend/portfolio_manager.h

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

## 📄 **FILE 10 of 39**: include/common/config.h

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

