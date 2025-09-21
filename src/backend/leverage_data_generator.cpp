#include "backend/leverage_data_generator.h"
#include "backend/leverage_types.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace sentio {

LeverageDataGenerator::LeverageDataGenerator(double daily_decay_rate, double expense_ratio)
    : daily_decay_rate_(daily_decay_rate),
      daily_expense_rate_(expense_ratio / 252.0) { // Approximate trading days per year
}

std::vector<Bar> LeverageDataGenerator::generate_series(const std::vector<Bar>& qqq_data, 
                                                      const std::string& symbol) {
    if (qqq_data.empty()) {
        throw std::invalid_argument("QQQ data cannot be empty");
    }
    
    // Get leverage specification
    LeverageSpec spec = get_leverage_spec(symbol);
    if (spec.type == InstrumentType::NEUTRAL) {
        throw std::runtime_error("Cannot generate data for unknown symbol: " + symbol);
    }
    
    // Cannot generate data for base instrument (QQQ)
    if (spec.type == InstrumentType::LONG_1X && !spec.is_inverse) {
        throw std::runtime_error("Cannot generate data for base instrument: " + symbol);
    }
    
    std::vector<Bar> leverage_series;
    leverage_series.reserve(qqq_data.size());
    
    // Initialize starting price for the leveraged ETF
    // Use a reasonable starting price (e.g., $100 for TQQQ, $50 for SQQQ)
    double starting_price = 100.0;
    if (spec.is_inverse) {
        starting_price = 50.0; // Inverse ETFs typically start lower
    }
    
    double prev_lev_close = starting_price;
    
    // Process each bar using daily return compounding
    for (size_t i = 1; i < qqq_data.size(); ++i) {
        const auto& prev_qqq_bar = qqq_data[i-1];
        const auto& curr_qqq_bar = qqq_data[i];
        
        // 1. Calculate the daily return of the base asset (QQQ)
        double qqq_daily_return = calculate_daily_return(prev_qqq_bar.close, curr_qqq_bar.close);
        
        // 2. Apply leverage factor to daily return
        double leveraged_return = apply_leverage_factor(qqq_daily_return, spec);
        
        // 3. Apply daily costs (decay + expense ratio)
        double total_daily_costs = calculate_daily_costs();
        
        // 4. Calculate the new closing price for the leveraged ETF
        double current_lev_close = prev_lev_close * (1.0 + leveraged_return - total_daily_costs);
        current_lev_close = validate_price(current_lev_close);
        
        // 5. Generate theoretical OHLC by scaling QQQ's intraday movements
        Bar lev_bar = generate_bar_ohlc(curr_qqq_bar, prev_lev_close, spec);
        lev_bar.close = current_lev_close;
        
        leverage_series.push_back(lev_bar);
        prev_lev_close = current_lev_close;
    }
    
    return leverage_series;
}

std::map<std::string, std::vector<Bar>> LeverageDataGenerator::generate_all_series(
    const std::vector<Bar>& qqq_data) {
    
    std::map<std::string, std::vector<Bar>> all_series;
    
    // Generate series for all supported leverage instruments
    auto all_symbols = LeverageRegistry::instance().get_all_symbols();
    
    for (const auto& symbol : all_symbols) {
        // Skip base instrument (QQQ)
        if (symbol == "QQQ") continue;
        
        try {
            all_series[symbol] = generate_series(qqq_data, symbol);
        } catch (const std::exception& e) {
            // Log error but continue with other symbols
            // In a real implementation, you might want to log this
            continue;
        }
    }
    
    return all_series;
}

LeverageSpec LeverageDataGenerator::get_leverage_spec(const std::string& symbol) const {
    return LeverageRegistry::instance().get_spec(symbol);
}

bool LeverageDataGenerator::is_supported_symbol(const std::string& symbol) const {
    auto spec = get_leverage_spec(symbol);
    return spec.type != InstrumentType::NEUTRAL && symbol != "QQQ";
}

Bar LeverageDataGenerator::generate_bar_ohlc(const Bar& qqq_bar, double prev_lev_close, 
                                           const LeverageSpec& spec) const {
    Bar lev_bar;
    lev_bar.timestamp_ms = qqq_bar.timestamp_ms;
    lev_bar.symbol = spec.symbol;
    
    // Calculate intraday movement ratios from QQQ
    double open_ratio = (qqq_bar.open - qqq_bar.close) / qqq_bar.close;
    double high_ratio = (qqq_bar.high - qqq_bar.close) / qqq_bar.close;
    double low_ratio = (qqq_bar.low - qqq_bar.close) / qqq_bar.close;
    
    // Apply leverage factor to intraday movements
    double lev_open_ratio = leverage_data_utils::scale_intraday_movement(
        open_ratio, spec.leverage_factor, spec.is_inverse);
    double lev_high_ratio = leverage_data_utils::scale_intraday_movement(
        high_ratio, spec.leverage_factor, spec.is_inverse);
    double lev_low_ratio = leverage_data_utils::scale_intraday_movement(
        low_ratio, spec.leverage_factor, spec.is_inverse);
    
    // Calculate leveraged OHLC prices
    double lev_open = prev_lev_close * (1.0 + lev_open_ratio);
    double lev_high = prev_lev_close * (1.0 + lev_high_ratio);
    double lev_low = prev_lev_close * (1.0 + lev_low_ratio);
    
    // For inverse ETFs, high and low are swapped
    if (spec.is_inverse) {
        std::swap(lev_high, lev_low);
    }
    
    // Ensure OHLC relationships are maintained
    lev_bar.open = validate_price(lev_open);
    lev_bar.high = validate_price(std::max({lev_open, lev_high}));
    lev_bar.low = validate_price(std::min({lev_open, lev_low}));
    
    // Volume scaling - leverage instruments typically have lower volume
    double volume_scaling = leverage_data_utils::calculate_volume_scaling(spec.leverage_factor);
    lev_bar.volume = static_cast<double>(qqq_bar.volume) * volume_scaling;
    
    return lev_bar;
}

double LeverageDataGenerator::calculate_daily_return(double prev_close, double curr_close) const {
    if (prev_close <= 0.0) {
        return 0.0; // Avoid division by zero
    }
    return (curr_close / prev_close) - 1.0;
}

double LeverageDataGenerator::apply_leverage_factor(double daily_return, 
                                                   const LeverageSpec& spec) const {
    double leveraged_return = daily_return * spec.leverage_factor;
    
    // Apply inversion if necessary
    if (spec.is_inverse) {
        leveraged_return = -leveraged_return;
    }
    
    return leveraged_return;
}

double LeverageDataGenerator::calculate_daily_costs() const {
    return daily_decay_rate_ + daily_expense_rate_;
}

double LeverageDataGenerator::validate_price(double price) const {
    return std::max(0.01, price); // Ensure positive price
}

} // namespace sentio
