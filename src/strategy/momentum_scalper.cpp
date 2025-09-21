#include "strategy/momentum_scalper.h"
#include "common/utils.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace sentio {

RegimeAdaptiveMomentumScalper::RegimeAdaptiveMomentumScalper(const ScalperConfig& config)
    : config_(config) {
    
    // Initialize PSM for systematic position management
    psm_ = std::make_unique<PositionStateMachine>();
    
    // Configure adaptive threshold manager for high-frequency scalping
    AdaptiveConfig adaptive_config;
    adaptive_config.algorithm = LearningAlgorithm::Q_LEARNING;
    adaptive_config.learning_rate = 0.2;        // Fast learning for scalping
    adaptive_config.exploration_rate = 0.3;     // High exploration for frequent signals
    adaptive_config.performance_window = 50;    // Shorter window for rapid adaptation
    adaptive_config.conservative_mode = false;  // Aggressive mode for scalping
    
    adaptive_manager_ = std::make_unique<AdaptiveThresholdManager>(adaptive_config);
    
    // Initialize thresholds
    current_buy_threshold_ = config_.base_buy_threshold;
    current_sell_threshold_ = config_.base_sell_threshold;
    
    utils::log_info("RegimeAdaptiveMomentumScalper initialized for high-frequency trading");
    utils::log_info("Target: 100+ daily trades, ~10% monthly returns, trend-following");
    utils::log_debug("SMA periods: " + std::to_string(config_.fast_sma_period) + "/" + 
                    std::to_string(config_.slow_sma_period));
    utils::log_debug("Base thresholds: buy=" + std::to_string(config_.base_buy_threshold) + 
                    ", sell=" + std::to_string(config_.base_sell_threshold));
}

PositionStateMachine::StateTransition RegimeAdaptiveMomentumScalper::process_bar(
    const Bar& bar,
    const SignalOutput& signal,
    const PortfolioState& current_portfolio) {
    
    // 1. Update price history and SMA values
    update_sma_values(bar.close);
    
    // 2. Detect current market regime
    MarketRegime previous_regime = current_regime_;
    current_regime_ = detect_market_regime();
    
    if (previous_regime != current_regime_) {
        std::string regime_name = (current_regime_ == MarketRegime::UPTREND ? "UPTREND" :
                                  current_regime_ == MarketRegime::DOWNTREND ? "DOWNTREND" : "NEUTRAL");
        utils::log_info("REGIME CHANGE: " + regime_name);
    }
    
    // 3. Update regime-adjusted thresholds
    update_regime_thresholds();
    
    // 4. Create market conditions for PSM
    MarketState market_conditions;
    market_conditions.volatility = 0.25; // Higher volatility for scalping
    market_conditions.trend_strength = trend_strength_;
    market_conditions.volume_ratio = 1.0;
    
    // 5. Get optimal transition from PSM
    auto psm_transition = psm_->get_optimal_transition(current_portfolio, signal, market_conditions);
    
    // 6. Apply trend filter - only allow regime-aligned transitions
    if (!is_transition_allowed(psm_transition)) {
        utils::log_debug("TREND FILTER: Blocking " + 
                        PositionStateMachine::state_to_string(psm_transition.target_state) + 
                        " transition in " + 
                        (current_regime_ == MarketRegime::UPTREND ? "UPTREND" : 
                         current_regime_ == MarketRegime::DOWNTREND ? "DOWNTREND" : "NEUTRAL"));
        
        // Return HOLD transition
        return {psm_transition.current_state, psm_transition.signal_type, 
                psm_transition.current_state, "HOLD - Trend filter block", 
                "Regime-adaptive momentum scalper", 0.0, 0.0, 0.5};
    }
    
    // 7. Enhance transition with optimal instrument selection
    if (psm_transition.target_state != psm_transition.current_state) {
        std::string optimal_symbol = select_optimal_instrument(signal, current_regime_);
        if (optimal_symbol != "HOLD") {
            // Update transition with optimal instrument
            psm_transition.optimal_action = "Scalp " + optimal_symbol + " (" + psm_transition.optimal_action + ")";
            psm_transition.theoretical_basis = "Regime-adaptive momentum scalping";
        }
    }
    
    std::string regime_str = (current_regime_ == MarketRegime::UPTREND ? "UP" : 
                             current_regime_ == MarketRegime::DOWNTREND ? "DOWN" : "NEUTRAL");
    utils::log_debug("SCALPER DECISION: " + 
                    PositionStateMachine::state_to_string(psm_transition.current_state) + 
                    " -> " + PositionStateMachine::state_to_string(psm_transition.target_state) + 
                    " | Regime: " + regime_str +
                    " | Trend: " + std::to_string(trend_strength_));
    
    return psm_transition;
}

void RegimeAdaptiveMomentumScalper::update_trade_outcome(const TradeOutcome& outcome) {
    // Update daily statistics
    trades_today_++;
    daily_pnl_ += outcome.actual_pnl;
    last_trade_time_ = outcome.outcome_timestamp;
    
    // Provide feedback to adaptive threshold manager
    if (adaptive_manager_) {
        adaptive_manager_->process_trade_outcome(
            outcome.symbol, outcome.action, outcome.quantity, outcome.price,
            outcome.trade_value, outcome.fees, outcome.actual_pnl,
            outcome.pnl_percentage, outcome.was_profitable
        );
    }
    
    utils::log_debug("SCALPER UPDATE: Trade #" + std::to_string(trades_today_) + 
                    " | Daily P&L: $" + std::to_string(daily_pnl_) + 
                    " | Last P&L: $" + std::to_string(outcome.actual_pnl));
}

std::pair<double, double> RegimeAdaptiveMomentumScalper::get_regime_thresholds() const {
    return {current_buy_threshold_, current_sell_threshold_};
}

double RegimeAdaptiveMomentumScalper::get_trend_strength() const {
    return trend_strength_;
}

bool RegimeAdaptiveMomentumScalper::is_transition_allowed(
    const PositionStateMachine::StateTransition& transition) const {
    
    if (!config_.enforce_trend_alignment) {
        return true; // Allow all transitions if trend alignment is disabled
    }
    
    switch (current_regime_) {
        case MarketRegime::UPTREND:
            // Only allow long positions and cash
            return !is_short_transition(transition);
            
        case MarketRegime::DOWNTREND:
            // Only allow short positions and cash
            return !is_long_transition(transition);
            
        case MarketRegime::NEUTRAL:
            // Allow all transitions in neutral regime
            return true;
            
        default:
            return true;
    }
}

void RegimeAdaptiveMomentumScalper::update_sma_values(double price) {
    // Add new price to history
    price_history_.push_back(price);
    maintain_history_size();
    
    // Calculate SMAs if we have enough data
    if (price_history_.size() >= static_cast<size_t>(config_.slow_sma_period)) {
        fast_sma_ = calculate_sma(price_history_, config_.fast_sma_period);
        slow_sma_ = calculate_sma(price_history_, config_.slow_sma_period);
        
        // Calculate trend strength (-1.0 to +1.0)
        if (slow_sma_ > 0) {
            trend_strength_ = std::clamp((fast_sma_ - slow_sma_) / slow_sma_, -1.0, 1.0);
        }
    }
}

RegimeAdaptiveMomentumScalper::MarketRegime RegimeAdaptiveMomentumScalper::detect_market_regime() {
    // Need sufficient data for regime detection
    if (price_history_.size() < static_cast<size_t>(config_.min_bars_for_trend)) {
        return MarketRegime::NEUTRAL;
    }
    
    // Simple SMA crossover for regime detection
    if (fast_sma_ > slow_sma_) {
        return MarketRegime::UPTREND;
    } else if (fast_sma_ < slow_sma_) {
        return MarketRegime::DOWNTREND;
    } else {
        return MarketRegime::NEUTRAL;
    }
}

void RegimeAdaptiveMomentumScalper::update_regime_thresholds() {
    if (!config_.enable_regime_adaptation) {
        return; // Use base thresholds
    }
    
    // Get adaptive thresholds as base
    double base_buy = config_.base_buy_threshold;
    double base_sell = config_.base_sell_threshold;
    
    // Apply regime bias for more aggressive scalping
    switch (current_regime_) {
        case MarketRegime::UPTREND:
            // More aggressive long entries, less aggressive short entries
            current_buy_threshold_ = base_buy - config_.uptrend_bias;
            current_sell_threshold_ = base_sell + config_.uptrend_bias;
            break;
            
        case MarketRegime::DOWNTREND:
            // More aggressive short entries, less aggressive long entries
            current_buy_threshold_ = base_buy + config_.downtrend_bias;
            current_sell_threshold_ = base_sell - config_.downtrend_bias;
            break;
            
        case MarketRegime::NEUTRAL:
        default:
            // Use base thresholds
            current_buy_threshold_ = base_buy;
            current_sell_threshold_ = base_sell;
            break;
    }
    
    // Ensure minimum gap between thresholds
    if (current_buy_threshold_ - current_sell_threshold_ < config_.min_threshold_gap) {
        double midpoint = (current_buy_threshold_ + current_sell_threshold_) / 2.0;
        current_buy_threshold_ = midpoint + config_.min_threshold_gap / 2.0;
        current_sell_threshold_ = midpoint - config_.min_threshold_gap / 2.0;
    }
    
    // Clamp to reasonable bounds
    current_buy_threshold_ = std::clamp(current_buy_threshold_, 0.51, 0.90);
    current_sell_threshold_ = std::clamp(current_sell_threshold_, 0.10, 0.49);
}

bool RegimeAdaptiveMomentumScalper::is_long_transition(
    const PositionStateMachine::StateTransition& transition) const {
    
    using State = PositionStateMachine::State;
    
    // Check if transition involves long positions
    return (transition.target_state == State::QQQ_ONLY ||
            transition.target_state == State::TQQQ_ONLY ||
            transition.target_state == State::QQQ_TQQQ);
}

bool RegimeAdaptiveMomentumScalper::is_short_transition(
    const PositionStateMachine::StateTransition& transition) const {
    
    using State = PositionStateMachine::State;
    
    // Check if transition involves short positions
    return (transition.target_state == State::PSQ_ONLY ||
            transition.target_state == State::SQQQ_ONLY ||
            transition.target_state == State::PSQ_SQQQ);
}

std::string RegimeAdaptiveMomentumScalper::select_optimal_instrument(
    const SignalOutput& signal, MarketRegime regime) const {
    
    if (!config_.enable_leveraged_scalping) {
        return (regime == MarketRegime::UPTREND) ? "QQQ" : 
               (regime == MarketRegime::DOWNTREND) ? "PSQ" : "QQQ";
    }
    
    // Calculate signal strength for leveraged instrument selection
    double signal_strength = std::abs(signal.probability - 0.5) * 2.0;
    bool strong_signal = signal_strength > config_.strong_signal_threshold;
    
    switch (regime) {
        case MarketRegime::UPTREND:
            if (signal.probability > current_buy_threshold_) {
                return strong_signal ? "TQQQ" : "QQQ"; // Use leverage for strong signals
            }
            break;
            
        case MarketRegime::DOWNTREND:
            if (signal.probability < current_sell_threshold_) {
                return strong_signal ? "SQQQ" : "PSQ"; // Use inverse leverage for strong signals
            }
            break;
            
        case MarketRegime::NEUTRAL:
        default:
            // In neutral regime, use moderate instruments
            return (signal.probability > 0.5) ? "QQQ" : "PSQ";
    }
    
    return "HOLD";
}

double RegimeAdaptiveMomentumScalper::calculate_sma(const std::deque<double>& data, int period) const {
    if (data.size() < static_cast<size_t>(period)) {
        return 0.0;
    }
    
    // Calculate SMA over the last 'period' values
    auto start_it = data.end() - period;
    double sum = std::accumulate(start_it, data.end(), 0.0);
    return sum / period;
}

void RegimeAdaptiveMomentumScalper::maintain_history_size() {
    // Keep only the data we need (slow SMA period + some buffer)
    size_t max_history = static_cast<size_t>(config_.slow_sma_period * 2);
    
    while (price_history_.size() > max_history) {
        price_history_.pop_front();
    }
}

} // namespace sentio
