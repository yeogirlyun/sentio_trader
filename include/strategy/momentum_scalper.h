#pragma once

#include <vector>
#include <deque>
#include <memory>
#include "common/types.h"
#include "strategy/signal_output.h"
#include "backend/position_state_machine.h"
#include "backend/adaptive_trading_mechanism.h"

namespace sentio {

/**
 * @class RegimeAdaptiveMomentumScalper
 * @brief High-frequency trend-following algorithm targeting 100+ daily trades and ~10% monthly returns
 * 
 * Core Features:
 * - SMA-based trend detection (10/30 crossover) to filter trade direction
 * - Adaptive thresholds with narrow neutral zone for high-frequency scalping
 * - PSM integration with trend-aware state transition filtering
 * - Leveraged instrument selection (TQQQ/SQQQ) for maximum profit potential
 * - Systematic risk management with trend alignment enforcement
 * 
 * Algorithm Philosophy:
 * - Only trade in the direction of the short-term trend
 * - Use tight, adaptive thresholds to generate frequent entry/exit signals
 * - Leverage 3x ETFs for high-conviction signals
 * - Maintain systematic position management via PSM
 */
class RegimeAdaptiveMomentumScalper {
public:
    /**
     * @brief Market regime based on SMA crossover
     */
    enum class MarketRegime {
        UPTREND,    // SMA10 > SMA30 - only long positions allowed
        DOWNTREND,  // SMA10 < SMA30 - only short positions allowed
        NEUTRAL     // Transition period or insufficient data
    };

    /**
     * @brief Configuration for the momentum scalper
     */
    struct ScalperConfig {
        // SMA parameters
        int fast_sma_period;      // Fast SMA for trend detection
        int slow_sma_period;      // Slow SMA for trend detection
        
        // Adaptive threshold configuration for high-frequency trading
        double base_buy_threshold;   // Narrow neutral zone
        double base_sell_threshold;  // for frequent signals
        double min_threshold_gap;    // Minimum gap between thresholds
        
        // Scalping parameters
        bool enable_leveraged_scalping;  // Use TQQQ/SQQQ for strong signals
        double strong_signal_threshold; // Threshold for leveraged instruments
        int min_bars_for_trend;           // Minimum bars needed for trend detection
        
        // Risk management
        double max_position_weight;      // Maximum portfolio allocation per position
        bool enforce_trend_alignment;   // Strict trend following
        
        // Performance optimization
        bool enable_regime_adaptation;  // Adapt thresholds based on regime
        double uptrend_bias;           // Bias towards longs in uptrend
        double downtrend_bias;         // Bias towards shorts in downtrend
        
        // Constructor with default values
        ScalperConfig() 
            : fast_sma_period(10)
            , slow_sma_period(30)
            , base_buy_threshold(0.52)
            , base_sell_threshold(0.48)
            , min_threshold_gap(0.02)
            , enable_leveraged_scalping(true)
            , strong_signal_threshold(0.15)
            , min_bars_for_trend(30)
            , max_position_weight(0.8)
            , enforce_trend_alignment(true)
            , enable_regime_adaptation(true)
            , uptrend_bias(0.02)
            , downtrend_bias(0.02)
        {}
    };

    /**
     * @brief Constructor
     * @param config Configuration for the scalper
     */
    explicit RegimeAdaptiveMomentumScalper(const ScalperConfig& config = ScalperConfig());

    /**
     * @brief Process a new bar and generate trading decision
     * @param bar Current market bar
     * @param signal Strategy signal output
     * @param current_portfolio Current portfolio state
     * @return Filtered PSM transition based on trend regime
     */
    PositionStateMachine::StateTransition process_bar(
        const Bar& bar,
        const SignalOutput& signal,
        const PortfolioState& current_portfolio
    );

    /**
     * @brief Update the scalper with trade outcome for learning
     * @param outcome Trade outcome for adaptive learning
     */
    void update_trade_outcome(const TradeOutcome& outcome);

    /**
     * @brief Get current market regime
     * @return Current detected market regime
     */
    MarketRegime get_current_regime() const { return current_regime_; }

    /**
     * @brief Get regime-adjusted thresholds
     * @return Current buy/sell thresholds adjusted for market regime
     */
    std::pair<double, double> get_regime_thresholds() const;

    /**
     * @brief Get trend strength indicator (-1.0 to +1.0)
     * @return Trend strength (positive = uptrend, negative = downtrend)
     */
    double get_trend_strength() const;

    /**
     * @brief Check if a PSM transition is allowed in current regime
     * @param transition Proposed state transition
     * @return True if transition aligns with current market regime
     */
    bool is_transition_allowed(const PositionStateMachine::StateTransition& transition) const;

private:
    ScalperConfig config_;
    MarketRegime current_regime_ = MarketRegime::NEUTRAL;
    
    // Price history for SMA calculation
    std::deque<double> price_history_;
    
    // SMA values
    double fast_sma_ = 0.0;
    double slow_sma_ = 0.0;
    double trend_strength_ = 0.0;
    
    // Regime-adjusted thresholds
    double current_buy_threshold_ = 0.52;
    double current_sell_threshold_ = 0.48;
    
    // Performance tracking
    int trades_today_ = 0;
    double daily_pnl_ = 0.0;
    std::chrono::system_clock::time_point last_trade_time_;
    
    // Components
    std::unique_ptr<PositionStateMachine> psm_;
    std::unique_ptr<AdaptiveThresholdManager> adaptive_manager_;
    
    // Helper methods
    void update_sma_values(double price);
    MarketRegime detect_market_regime();
    void update_regime_thresholds();
    bool is_long_transition(const PositionStateMachine::StateTransition& transition) const;
    bool is_short_transition(const PositionStateMachine::StateTransition& transition) const;
    std::string select_optimal_instrument(const SignalOutput& signal, MarketRegime regime) const;
    
    // SMA calculation utilities
    double calculate_sma(const std::deque<double>& data, int period) const;
    void maintain_history_size();
};

} // namespace sentio
