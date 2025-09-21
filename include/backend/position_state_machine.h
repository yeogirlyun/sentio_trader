#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include "common/types.h"
#include "strategy/signal_output.h"

namespace sentio {

// Forward declaration - MarketState is defined in adaptive_trading_mechanism.h
struct MarketState;

/**
 * @class PositionStateMachine
 * @brief Implements a systematic, state-based approach to portfolio management.
 *
 * This class replaces ad-hoc trading logic with a theoretically sound state machine
 * that maps all possible portfolio states and signal scenarios to optimal transitions,
 * as defined in the PSM Requirements Document (PSM-REQ-001).
 * 
 * Core Features:
 * - Handles 8 portfolio states (CASH_ONLY, QQQ_ONLY, TQQQ_ONLY, PSQ_ONLY, SQQQ_ONLY, QQQ_TQQQ, PSQ_SQQQ, INVALID)
 * - Processes 4 signal types (STRONG_BUY, WEAK_BUY, WEAK_SELL, STRONG_SELL)
 * - Provides 32 complete state transition scenarios
 * - Integrates with existing AdaptivePortfolioManager and AdaptiveThresholdManager
 * - Based on theoretical optimization framework (Kelly Criterion, Risk Parity)
 */
class PositionStateMachine {
public:
    // REQ-PSM-001: Defines the complete state space for all position combinations.
    enum class State {
        CASH_ONLY,      // No positions held (100% cash)
        QQQ_ONLY,       // Only QQQ position held
        TQQQ_ONLY,      // Only TQQQ position held  
        PSQ_ONLY,       // Only PSQ position held
        SQQQ_ONLY,      // Only SQQQ position held
        QQQ_TQQQ,       // Both QQQ and TQQQ positions (amplified long exposure)
        PSQ_SQQQ,       // Both PSQ and SQQQ positions (amplified short exposure)
        INVALID         // Any conflicting combination (e.g., QQQ + PSQ)
    };

    // Defines the four possible signal scenarios plus neutral.
    enum class SignalType {
        STRONG_BUY,     // Signal > (buy_threshold + margin)
        WEAK_BUY,       // Signal > buy_threshold but < (buy_threshold + margin)
        WEAK_SELL,      // Signal < sell_threshold but > (sell_threshold - margin)
        STRONG_SELL,    // Signal < (sell_threshold - margin)
        NEUTRAL         // Signal within neutral zone
    };

    // Represents a recommended transition from a current state to a target state.
    struct StateTransition {
        State current_state;
        SignalType signal_type;
        State target_state;
        std::string optimal_action;
        std::string theoretical_basis;
        double expected_return = 0.0;      // Expected return from this transition
        double risk_score = 0.0;           // Risk assessment score
        double confidence = 0.0;           // Confidence in the transition
    };

    // Constructor initializes the state machine and its components.
    PositionStateMachine();

    /**
     * @brief Determines the optimal state transition based on the current portfolio and signal.
     * @param current_portfolio The current state of the portfolio (positions held).
     * @param signal The latest trading signal from the strategy.
     * @param market_conditions Current market state (e.g., volatility).
     * @return The recommended StateTransition object.
     * 
     * REQ-PSM-005: Provides real-time state transition recommendations in <1ms.
     */
    StateTransition get_optimal_transition(
        const PortfolioState& current_portfolio,
        const SignalOutput& signal,
        const MarketState& market_conditions
    );

    /**
     * @brief Gets state-aware thresholds for adaptive threshold management.
     * @param base_buy_threshold Base buy threshold from adaptive system.
     * @param base_sell_threshold Base sell threshold from adaptive system.
     * @param current_state Current portfolio state.
     * @return Adjusted thresholds based on portfolio state.
     * 
     * REQ-PSM-004: Integrates with adaptive threshold mechanism.
     */
    std::pair<double, double> get_state_aware_thresholds(
        double base_buy_threshold,
        double base_sell_threshold,
        State current_state
    ) const;

    /**
     * @brief Validates if a proposed transition is safe and optimal.
     * @param transition The proposed state transition.
     * @param current_portfolio Current portfolio state.
     * @param available_capital Available capital for trading.
     * @return True if transition is valid and safe.
     * 
     * REQ-PSM-003: Implements theoretical optimization framework.
     */
    bool validate_transition(
        const StateTransition& transition,
        const PortfolioState& current_portfolio,
        double available_capital
    ) const;

    // Helper functions for state and signal type conversion
    static std::string state_to_string(State s);
    static std::string signal_type_to_string(SignalType st);
    
    // Get current state from portfolio (public for testing)
    State determine_current_state(const PortfolioState& portfolio) const;

private:
    /**
     * @brief Classifies a raw signal probability into a SignalType enum.
     * @param signal The signal output from the strategy.
     * @param buy_threshold Dynamic buy threshold.
     * @param sell_threshold Dynamic sell threshold.
     * @return The corresponding SignalType enum value.
     */
    SignalType classify_signal(
        const SignalOutput& signal,
        double buy_threshold,
        double sell_threshold
    ) const;
    
    // Populates the 32 required state transitions.
    void initialize_transition_matrix();

    // Apply state-specific risk adjustments
    double apply_state_risk_adjustment(State state, double base_value) const;

    // Calculate optimal position size using Kelly Criterion
    double calculate_kelly_position_size(
        double signal_probability,
        double expected_return,
        double risk_estimate,
        double available_capital
    ) const;

    // The core transition logic mapping (State, Signal) -> Transition.
    std::map<std::pair<State, SignalType>, StateTransition> transition_matrix_;

    // Placeholder components for future optimization and risk management
    // std::unique_ptr<class OptimizationEngine> optimization_engine_;
    // std::unique_ptr<class RiskManager> risk_manager_;

    // Configuration constants
    static constexpr double DEFAULT_BUY_THRESHOLD = 0.55;
    static constexpr double DEFAULT_SELL_THRESHOLD = 0.45;
    static constexpr double STRONG_MARGIN = 0.15;
    
    // Risk management constants
    static constexpr double MAX_LEVERAGE_EXPOSURE = 0.8;    // Max 80% in leveraged instruments
    static constexpr double MAX_POSITION_SIZE = 0.6;       // Max 60% in single position
    static constexpr double MIN_CASH_BUFFER = 0.1;         // Min 10% cash buffer
};

// Integration typedef for existing system compatibility
using PSM = PositionStateMachine;

} // namespace sentio
