#pragma once

// =============================================================================
// Module: backend/leverage_data_generator.h
// Purpose: Accurate leverage data generation using daily return compounding
//
// Core Architecture & Design:
// This module implements the corrected leverage data generation model that
// accurately captures the path-dependent nature of leveraged ETFs through
// daily return compounding rather than simple price multiplication.
//
// Key Design Principles:
// - Daily return compounding: Models actual ETF rebalancing mechanism
// - Path dependency: Captures volatility decay and compounding effects
// - Realistic pricing: Generates data that matches actual ETF behavior
// - Accurate OHLC: Maintains proper intraday relationships
//
// Recent Improvements:
// - Corrected pricing model based on daily returns, not price multiplication
// - Proper handling of inverse instruments with OHLC inversion
// - Accurate modeling of daily rebalancing costs and expense ratios
// - Realistic volume scaling for leverage instruments
// =============================================================================

#include "common/types.h"
#include "leverage_types.h"
#include <vector>
#include <string>
#include <cmath>

namespace sentio {

/// Accurate leverage data generator using daily return compounding
/// 
/// This class generates theoretical OHLCV data for leveraged ETFs using
/// the corrected daily return compounding model that accurately captures
/// the path-dependent nature of leveraged instruments.
class LeverageDataGenerator {
public:
    /// Constructor with configurable parameters
    /// @param daily_decay_rate Daily rebalancing cost (default: 0.0001 = 0.01%)
    /// @param expense_ratio Annual expense ratio (default: 0.0095 = 0.95%)
    LeverageDataGenerator(double daily_decay_rate = 0.0001, 
                         double expense_ratio = 0.0095);
    
    /// Generate a theoretical OHLCV series for a leveraged ETF
    /// 
    /// This method uses the corrected daily return compounding model:
    /// 1. Calculate daily return of base asset (QQQ)
    /// 2. Apply leverage factor to daily return
    /// 3. Apply daily costs (decay + expense ratio)
    /// 4. Compound the leveraged return onto previous day's price
    /// 5. Generate OHLC by scaling intraday movements proportionally
    /// 
    /// @param qqq_data The base QQQ data series
    /// @param symbol The target symbol to generate (e.g., "TQQQ", "SQQQ", "PSQ")
    /// @return A vector of Bars for the leveraged instrument
    /// @throws std::runtime_error if symbol is not supported or is base instrument
    std::vector<Bar> generate_series(const std::vector<Bar>& qqq_data, 
                                   const std::string& symbol);
    
    /// Generate series for all supported leverage instruments
    /// @param qqq_data The base QQQ data series
    /// @return Map of symbol -> generated series
    std::map<std::string, std::vector<Bar>> generate_all_series(const std::vector<Bar>& qqq_data);
    
    /// Get the leverage specification for a symbol
    /// @param symbol The instrument symbol
    /// @return LeverageSpec for the symbol
    LeverageSpec get_leverage_spec(const std::string& symbol) const;
    
    /// Check if a symbol is supported for data generation
    /// @param symbol The instrument symbol
    /// @return true if the symbol can be generated
    bool is_supported_symbol(const std::string& symbol) const;

private:
    double daily_decay_rate_;      /// Daily rebalancing cost
    double daily_expense_rate_;    /// Daily expense ratio (annual / 252)
    
    /// Generate OHLC for a single bar using proportional scaling
    /// @param qqq_bar The base QQQ bar
    /// @param prev_lev_close Previous leveraged close price
    /// @param spec Leverage specification
    /// @return Generated leveraged bar
    Bar generate_bar_ohlc(const Bar& qqq_bar, double prev_lev_close, 
                         const LeverageSpec& spec) const;
    
    /// Calculate the daily return of the base asset
    /// @param prev_close Previous day's close price
    /// @param curr_close Current day's close price
    /// @return Daily return (0.0 if prev_close is 0 or invalid)
    double calculate_daily_return(double prev_close, double curr_close) const;
    
    /// Apply leverage factor and inversion to daily return
    /// @param daily_return Base daily return
    /// @param spec Leverage specification
    /// @return Leveraged daily return
    double apply_leverage_factor(double daily_return, const LeverageSpec& spec) const;
    
    /// Calculate total daily costs (decay + expense)
    /// @return Total daily cost rate
    double calculate_daily_costs() const;
    
    /// Ensure price is positive and reasonable
    /// @param price The price to validate
    /// @return Validated price (minimum 0.01)
    double validate_price(double price) const;
};

/// Utility functions for leverage data generation
namespace leverage_data_utils {

/// Calculate theoretical leverage price using daily return compounding
/// @param prev_price Previous leveraged price
/// @param base_daily_return Daily return of base asset
/// @param leverage_factor Leverage multiplier
/// @param is_inverse Whether instrument is inverse
/// @param daily_costs Total daily costs
/// @return New leveraged price
inline double calculate_leveraged_price(double prev_price, 
                                       double base_daily_return,
                                       double leverage_factor,
                                       bool is_inverse,
                                       double daily_costs) {
    // Apply leverage factor to daily return
    double leveraged_return = base_daily_return * leverage_factor;
    
    // Apply inversion if necessary
    if (is_inverse) {
        leveraged_return = -leveraged_return;
    }
    
    // Apply daily costs and compound
    double new_price = prev_price * (1.0 + leveraged_return - daily_costs);
    
    // Ensure positive price
    return std::max(0.01, new_price);
}

/// Scale intraday movements proportionally
/// @param base_movement Base asset intraday movement ratio
/// @param leverage_factor Leverage multiplier
/// @param is_inverse Whether instrument is inverse
/// @return Scaled movement ratio
inline double scale_intraday_movement(double base_movement, 
                                     double leverage_factor,
                                     bool is_inverse) {
    double scaled = base_movement * leverage_factor;
    return is_inverse ? -scaled : scaled;
}

/// Calculate volume scaling factor for leverage instruments
/// @param leverage_factor Leverage multiplier
/// @return Volume scaling factor (typically 0.3 for 3x leverage)
inline double calculate_volume_scaling(double leverage_factor) {
    // Higher leverage typically has lower volume due to higher risk
    return std::max(0.1, 1.0 / leverage_factor);
}

} // namespace leverage_data_utils

} // namespace sentio
