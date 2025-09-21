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
    
private:
    /// Helper function to analyze position exposure types
    /// @param positions Map of positions to analyze
    /// @return Pair of (has_long, has_short) exposure
    std::pair<bool, bool> analyze_position_exposure(const std::map<std::string, Position>& positions) const {
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
        
        return {has_long, has_short};
    }

public:
    /// Get the current position type based on existing positions
    /// @param current_positions Map of current positions
    /// @return The dominant position type, or NEUTRAL if no positions
    InstrumentType get_current_position_type(const std::map<std::string, Position>& current_positions) const {
        auto [has_long, has_short] = analyze_position_exposure(current_positions);
        
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
        auto [has_long, has_short] = analyze_position_exposure(positions);
        
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
