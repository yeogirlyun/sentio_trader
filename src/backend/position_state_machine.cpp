#include "backend/position_state_machine.h"
#include "common/utils.h"
#include <vector>
#include <string>
#include <set>
#include <cmath>
#include <algorithm>

// Constructor for the PositionStateMachine.
sentio::PositionStateMachine::PositionStateMachine() {
    initialize_transition_matrix();
    utils::log_info("PositionStateMachine initialized with 32 state transitions");
    // In a full implementation, you would also initialize:
    // optimization_engine_ = std::make_unique<OptimizationEngine>();
    // risk_manager_ = std::make_unique<RiskManager>();
}

// REQ-PSM-002: Maps all signal scenarios to optimal state transitions.
void sentio::PositionStateMachine::initialize_transition_matrix() {
    utils::log_debug("Initializing Position State Machine transition matrix with 32 scenarios");
    
    // This function populates all 32 decision scenarios from the requirements document.
    
    // 1. CASH_ONLY State Transitions (4 scenarios)
    transition_matrix_[{State::CASH_ONLY, SignalType::STRONG_BUY}] = {
        State::CASH_ONLY, SignalType::STRONG_BUY, State::TQQQ_ONLY, 
        "Initiate TQQQ position", "Maximize leverage on strong signal",
        0.15, 0.8, 0.9  // expected_return, risk_score, confidence
    };
    transition_matrix_[{State::CASH_ONLY, SignalType::WEAK_BUY}] = {
        State::CASH_ONLY, SignalType::WEAK_BUY, State::QQQ_ONLY, 
        "Initiate QQQ position", "Conservative entry",
        0.08, 0.4, 0.7
    };
    transition_matrix_[{State::CASH_ONLY, SignalType::WEAK_SELL}] = {
        State::CASH_ONLY, SignalType::WEAK_SELL, State::PSQ_ONLY, 
        "Initiate PSQ position", "Conservative short entry",
        0.06, 0.4, 0.6
    };
    transition_matrix_[{State::CASH_ONLY, SignalType::STRONG_SELL}] = {
        State::CASH_ONLY, SignalType::STRONG_SELL, State::SQQQ_ONLY, 
        "Initiate SQQQ position", "Maximize short leverage",
        0.12, 0.8, 0.85
    };

    // 2. QQQ_ONLY State Transitions (4 scenarios)
    transition_matrix_[{State::QQQ_ONLY, SignalType::STRONG_BUY}] = {
        State::QQQ_ONLY, SignalType::STRONG_BUY, State::QQQ_TQQQ, 
        "Scale up with TQQQ", "Leverage profitable position",
        0.18, 0.6, 0.85
    };
    transition_matrix_[{State::QQQ_ONLY, SignalType::WEAK_BUY}] = {
        State::QQQ_ONLY, SignalType::WEAK_BUY, State::QQQ_ONLY, 
        "Add to QQQ position", "Conservative scaling",
        0.05, 0.3, 0.6
    };
    transition_matrix_[{State::QQQ_ONLY, SignalType::WEAK_SELL}] = {
        State::QQQ_ONLY, SignalType::WEAK_SELL, State::QQQ_ONLY, 
        "Partial QQQ liquidation", "Risk reduction",
        0.02, 0.2, 0.5
    };
    transition_matrix_[{State::QQQ_ONLY, SignalType::STRONG_SELL}] = {
        State::QQQ_ONLY, SignalType::STRONG_SELL, State::CASH_ONLY, 
        "Full QQQ liquidation", "Capital preservation",
        0.0, 0.1, 0.9
    };

    // 3. TQQQ_ONLY State Transitions (4 scenarios)
    transition_matrix_[{State::TQQQ_ONLY, SignalType::STRONG_BUY}] = {
        State::TQQQ_ONLY, SignalType::STRONG_BUY, State::QQQ_TQQQ, 
        "Add QQQ for stability", "Diversify leverage risk",
        0.12, 0.5, 0.8
    };
    transition_matrix_[{State::TQQQ_ONLY, SignalType::WEAK_BUY}] = {
        State::TQQQ_ONLY, SignalType::WEAK_BUY, State::TQQQ_ONLY, 
        "Scale up TQQQ", "Maintain leverage",
        0.08, 0.7, 0.6
    };
    transition_matrix_[{State::TQQQ_ONLY, SignalType::WEAK_SELL}] = {
        State::TQQQ_ONLY, SignalType::WEAK_SELL, State::QQQ_ONLY, 
        "Partial TQQQ -> QQQ", "De-leverage gradually",
        0.03, 0.3, 0.7
    };
    transition_matrix_[{State::TQQQ_ONLY, SignalType::STRONG_SELL}] = {
        State::TQQQ_ONLY, SignalType::STRONG_SELL, State::CASH_ONLY, 
        "Full TQQQ liquidation", "Rapid de-risking",
        0.0, 0.1, 0.95
    };

    // 4. PSQ_ONLY State Transitions (4 scenarios)
    transition_matrix_[{State::PSQ_ONLY, SignalType::STRONG_BUY}] = {
        State::PSQ_ONLY, SignalType::STRONG_BUY, State::CASH_ONLY, 
        "Full PSQ liquidation", "Directional reversal",
        0.0, 0.2, 0.9
    };
    transition_matrix_[{State::PSQ_ONLY, SignalType::WEAK_BUY}] = {
        State::PSQ_ONLY, SignalType::WEAK_BUY, State::PSQ_ONLY, 
        "Partial PSQ liquidation", "Gradual unwinding",
        0.02, 0.3, 0.6
    };
    transition_matrix_[{State::PSQ_ONLY, SignalType::WEAK_SELL}] = {
        State::PSQ_ONLY, SignalType::WEAK_SELL, State::PSQ_ONLY, 
        "Add to PSQ position", "Reinforce position",
        0.04, 0.4, 0.6
    };
    transition_matrix_[{State::PSQ_ONLY, SignalType::STRONG_SELL}] = {
        State::PSQ_ONLY, SignalType::STRONG_SELL, State::PSQ_SQQQ, 
        "Scale up with SQQQ", "Amplify short exposure",
        0.15, 0.7, 0.8
    };

    // 5. SQQQ_ONLY State Transitions (4 scenarios)
    transition_matrix_[{State::SQQQ_ONLY, SignalType::STRONG_BUY}] = {
        State::SQQQ_ONLY, SignalType::STRONG_BUY, State::CASH_ONLY, 
        "Full SQQQ liquidation", "Rapid directional reversal",
        0.0, 0.1, 0.95
    };
    transition_matrix_[{State::SQQQ_ONLY, SignalType::WEAK_BUY}] = {
        State::SQQQ_ONLY, SignalType::WEAK_BUY, State::PSQ_ONLY, 
        "Partial SQQQ -> PSQ", "Gradual de-leveraging",
        0.02, 0.4, 0.7
    };
    transition_matrix_[{State::SQQQ_ONLY, SignalType::WEAK_SELL}] = {
        State::SQQQ_ONLY, SignalType::WEAK_SELL, State::SQQQ_ONLY, 
        "Scale up SQQQ", "Maintain leverage",
        0.06, 0.8, 0.6
    };
    transition_matrix_[{State::SQQQ_ONLY, SignalType::STRONG_SELL}] = {
        State::SQQQ_ONLY, SignalType::STRONG_SELL, State::PSQ_SQQQ, 
        "Add PSQ for stability", "Diversify short risk",
        0.10, 0.6, 0.8
    };

    // 6. QQQ_TQQQ State Transitions (4 scenarios)
    transition_matrix_[{State::QQQ_TQQQ, SignalType::STRONG_BUY}] = {
        State::QQQ_TQQQ, SignalType::STRONG_BUY, State::QQQ_TQQQ, 
        "Scale both positions", "Amplify winning strategy",
        0.20, 0.8, 0.9
    };
    transition_matrix_[{State::QQQ_TQQQ, SignalType::WEAK_BUY}] = {
        State::QQQ_TQQQ, SignalType::WEAK_BUY, State::QQQ_TQQQ, 
        "Add to QQQ only", "Conservative scaling",
        0.06, 0.4, 0.6
    };
    transition_matrix_[{State::QQQ_TQQQ, SignalType::WEAK_SELL}] = {
        State::QQQ_TQQQ, SignalType::WEAK_SELL, State::QQQ_ONLY, 
        "Liquidate TQQQ first", "De-leverage gradually",
        0.02, 0.3, 0.7
    };
    transition_matrix_[{State::QQQ_TQQQ, SignalType::STRONG_SELL}] = {
        State::QQQ_TQQQ, SignalType::STRONG_SELL, State::CASH_ONLY, 
        "Full liquidation", "Rapid risk reduction",
        0.0, 0.1, 0.95
    };

    // 7. PSQ_SQQQ State Transitions (4 scenarios)
    transition_matrix_[{State::PSQ_SQQQ, SignalType::STRONG_BUY}] = {
        State::PSQ_SQQQ, SignalType::STRONG_BUY, State::CASH_ONLY, 
        "Full liquidation", "Complete directional reversal",
        0.0, 0.1, 0.95
    };
    transition_matrix_[{State::PSQ_SQQQ, SignalType::WEAK_BUY}] = {
        State::PSQ_SQQQ, SignalType::WEAK_BUY, State::PSQ_ONLY, 
        "Liquidate SQQQ first", "Gradual de-leveraging",
        0.02, 0.4, 0.7
    };
    transition_matrix_[{State::PSQ_SQQQ, SignalType::WEAK_SELL}] = {
        State::PSQ_SQQQ, SignalType::WEAK_SELL, State::PSQ_SQQQ, 
        "Add to PSQ only", "Conservative scaling",
        0.05, 0.5, 0.6
    };
    transition_matrix_[{State::PSQ_SQQQ, SignalType::STRONG_SELL}] = {
        State::PSQ_SQQQ, SignalType::STRONG_SELL, State::PSQ_SQQQ, 
        "Scale both positions", "Amplify short strategy",
        0.18, 0.8, 0.85
    };
    
    utils::log_info("Position State Machine transition matrix initialized with " + 
                   std::to_string(transition_matrix_.size()) + " transitions");
}

// REQ-PSM-005: Provide real-time state transition recommendations.
sentio::PositionStateMachine::StateTransition sentio::PositionStateMachine::get_optimal_transition(
    const PortfolioState& current_portfolio,
    const SignalOutput& signal,
    const MarketState& market_conditions) {
    
    // 1. Determine the current state from the portfolio.
    State current_state = determine_current_state(current_portfolio);

    // Handle the INVALID state immediately.
    if (current_state == State::INVALID) {
        utils::log_warning("INVALID portfolio state detected - triggering emergency liquidation");
        return {State::INVALID, SignalType::NEUTRAL, State::CASH_ONLY, 
                "Emergency liquidation", "Risk containment", 0.0, 0.0, 1.0};
    }

    // 2. Classify the incoming signal using dynamic thresholds
    SignalType signal_type = classify_signal(signal, DEFAULT_BUY_THRESHOLD, DEFAULT_SELL_THRESHOLD);

    // Handle NEUTRAL signal (no action).
    if (signal_type == SignalType::NEUTRAL) {
        utils::log_debug("NEUTRAL signal (" + std::to_string(signal.probability) + 
                        ") - maintaining current state: " + state_to_string(current_state));
        return {current_state, signal_type, current_state, 
                "Hold position", "Signal in neutral zone", 0.0, 0.0, 0.5};
    }
    
    // 3. Look up the transition in the matrix.
    auto it = transition_matrix_.find({current_state, signal_type});

    if (it != transition_matrix_.end()) {
        StateTransition transition = it->second;
        
        // Apply market condition adjustments
        transition.risk_score = apply_state_risk_adjustment(current_state, transition.risk_score);
        
        // REQ-PSM-003 & REQ-PSM-004: Here you would integrate the more complex modules.
        // optimization_engine_->optimize_transition(transition, ...);
        // if (!risk_manager_->validate_transition(transition, ...)) { ... }
        
        utils::log_debug("PSM Transition: " + state_to_string(current_state) + 
                        " + " + signal_type_to_string(signal_type) + 
                        " -> " + state_to_string(transition.target_state) + 
                        " (" + transition.optimal_action + ")");
        
        return transition;
    }

    // Fallback if a transition is not defined (should not happen with a complete matrix).
    utils::log_error("Undefined transition for state=" + state_to_string(current_state) + 
                     ", signal=" + signal_type_to_string(signal_type));
    return {current_state, signal_type, current_state, 
            "Hold (Undefined Transition)", "No valid action defined for this state/signal pair",
            0.0, 1.0, 0.0};
}

// REQ-PSM-004: Integration with adaptive threshold mechanism
std::pair<double, double> sentio::PositionStateMachine::get_state_aware_thresholds(
    double base_buy_threshold,
    double base_sell_threshold,
    State current_state
) const {
    
    double buy_adjustment = 1.0;
    double sell_adjustment = 1.0;
    
    // State-specific threshold adjustments based on risk profile
    switch (current_state) {
        case State::QQQ_TQQQ:
        case State::PSQ_SQQQ:
            // More conservative thresholds for leveraged positions
            buy_adjustment = 0.95;  // Slightly higher buy threshold
            sell_adjustment = 1.05; // Slightly lower sell threshold
            break;
            
        case State::TQQQ_ONLY:
        case State::SQQQ_ONLY:
            // Very conservative for high-leverage single positions
            buy_adjustment = 0.90;
            sell_adjustment = 1.10;
            break;
            
        case State::CASH_ONLY:
            // More aggressive thresholds for cash deployment
            buy_adjustment = 1.05;  // Slightly lower buy threshold
            sell_adjustment = 0.95; // Slightly higher sell threshold
            break;
            
        case State::QQQ_ONLY:
        case State::PSQ_ONLY:
            // Standard adjustments for single unleveraged positions
            buy_adjustment = 1.0;
            sell_adjustment = 1.0;
            break;
            
        case State::INVALID:
            // Emergency conservative thresholds
            buy_adjustment = 0.80;
            sell_adjustment = 1.20;
            break;
    }
    
    double adjusted_buy = base_buy_threshold * buy_adjustment;
    double adjusted_sell = base_sell_threshold * sell_adjustment;
    
    // Ensure thresholds maintain minimum gap
    if (adjusted_buy - adjusted_sell < 0.05) {
        double gap = 0.05;
        double midpoint = (adjusted_buy + adjusted_sell) / 2.0;
        adjusted_buy = midpoint + gap / 2.0;
        adjusted_sell = midpoint - gap / 2.0;
    }
    
    // Clamp to reasonable bounds
    adjusted_buy = std::clamp(adjusted_buy, 0.51, 0.90);
    adjusted_sell = std::clamp(adjusted_sell, 0.10, 0.49);
    
    utils::log_debug("State-aware thresholds for " + state_to_string(current_state) + 
                    ": buy=" + std::to_string(adjusted_buy) + 
                    ", sell=" + std::to_string(adjusted_sell));
    
    return {adjusted_buy, adjusted_sell};
}

// REQ-PSM-003: Theoretical optimization framework validation
bool sentio::PositionStateMachine::validate_transition(
    const StateTransition& transition,
    const PortfolioState& current_portfolio,
    double available_capital
) const {
    
    // Basic validation checks
    if (transition.risk_score > 0.9) {
        utils::log_warning("High risk transition rejected: risk_score=" + 
                          std::to_string(transition.risk_score));
        return false;
    }
    
    if (transition.confidence < 0.3) {
        utils::log_warning("Low confidence transition rejected: confidence=" + 
                          std::to_string(transition.confidence));
        return false;
    }
    
    // Check capital requirements (simplified)
    if (available_capital < MIN_CASH_BUFFER * 100000) { // Assuming $100k base capital
        utils::log_warning("Insufficient capital for transition: available=" + 
                          std::to_string(available_capital));
        return false;
    }
    
    // Validate state transition logic
    if (transition.current_state == State::INVALID && 
        transition.target_state != State::CASH_ONLY) {
        utils::log_error("Invalid state must transition to CASH_ONLY");
        return false;
    }
    
    utils::log_debug("Transition validation passed for " + 
                    state_to_string(transition.current_state) + " -> " + 
                    state_to_string(transition.target_state));
    
    return true;
}

sentio::PositionStateMachine::State sentio::PositionStateMachine::determine_current_state(const PortfolioState& portfolio) const {
    std::set<std::string> symbols;
    for (const auto& [symbol, pos] : portfolio.positions) {
        if (pos.quantity > 1e-6) { // Consider only positions with meaningful quantity
            symbols.insert(symbol);
        }
    }

    if (symbols.empty()) {
        return State::CASH_ONLY;
    }
    
    bool has_qqq = symbols.count("QQQ");
    bool has_tqqq = symbols.count("TQQQ");
    bool has_psq = symbols.count("PSQ");
    bool has_sqqq = symbols.count("SQQQ");

    // Single Instrument States
    if (symbols.size() == 1) {
        if (has_qqq) return State::QQQ_ONLY;
        if (has_tqqq) return State::TQQQ_ONLY;
        if (has_psq) return State::PSQ_ONLY;
        if (has_sqqq) return State::SQQQ_ONLY;
    }

    // Dual Instrument States (valid combinations only)
    if (symbols.size() == 2) {
        if (has_qqq && has_tqqq) return State::QQQ_TQQQ;
        if (has_psq && has_sqqq) return State::PSQ_SQQQ;
    }

    // Any other combination is considered invalid (e.g., QQQ + PSQ, TQQQ + SQQQ)
    utils::log_warning("Invalid portfolio state detected with symbols: " + 
                      [&symbols]() {
                          std::string result;
                          for (const auto& s : symbols) {
                              if (!result.empty()) result += ", ";
                              result += s;
                          }
                          return result;
                      }());
    return State::INVALID;
}

sentio::PositionStateMachine::SignalType sentio::PositionStateMachine::classify_signal(
    const SignalOutput& signal,
    double buy_threshold,
    double sell_threshold
) const {
    double p = signal.probability;
    
    if (p > (buy_threshold + STRONG_MARGIN)) return SignalType::STRONG_BUY;
    if (p > buy_threshold) return SignalType::WEAK_BUY;
    if (p < (sell_threshold - STRONG_MARGIN)) return SignalType::STRONG_SELL;
    if (p < sell_threshold) return SignalType::WEAK_SELL;
    
    return SignalType::NEUTRAL;
}

double sentio::PositionStateMachine::apply_state_risk_adjustment(State state, double base_risk) const {
    double adjustment = 1.0;
    
    switch (state) {
        case State::TQQQ_ONLY:
        case State::SQQQ_ONLY:
            adjustment = 1.3; // Higher risk for leveraged single positions
            break;
        case State::QQQ_TQQQ:
        case State::PSQ_SQQQ:
            adjustment = 1.2; // Moderate increase for dual positions
            break;
        case State::CASH_ONLY:
            adjustment = 0.5; // Lower risk for cash positions
            break;
        default:
            adjustment = 1.0; // No adjustment for standard positions
            break;
    }
    
    return std::clamp(base_risk * adjustment, 0.0, 1.0);
}

double sentio::PositionStateMachine::calculate_kelly_position_size(
    double signal_probability,
    double expected_return,
    double risk_estimate,
    double available_capital
) const {
    // Kelly Criterion: f* = (bp - q) / b
    // where b = odds, p = win probability, q = loss probability
    
    if (risk_estimate <= 0.0 || expected_return <= 0.0) {
        return 0.0;
    }
    
    double win_prob = std::clamp(signal_probability, 0.1, 0.9);
    double loss_prob = 1.0 - win_prob;
    double odds = expected_return / risk_estimate;
    
    double kelly_fraction = (odds * win_prob - loss_prob) / odds;
    kelly_fraction = std::clamp(kelly_fraction, 0.0, MAX_POSITION_SIZE);
    
    return available_capital * kelly_fraction;
}

// Helper function to convert State enum to string for logging and debugging.
std::string sentio::PositionStateMachine::state_to_string(State s) {
    switch (s) {
        case State::CASH_ONLY: return "CASH_ONLY";
        case State::QQQ_ONLY: return "QQQ_ONLY";
        case State::TQQQ_ONLY: return "TQQQ_ONLY";
        case State::PSQ_ONLY: return "PSQ_ONLY";
        case State::SQQQ_ONLY: return "SQQQ_ONLY";
        case State::QQQ_TQQQ: return "QQQ_TQQQ";
        case State::PSQ_SQQQ: return "PSQ_SQQQ";
        case State::INVALID: return "INVALID";
        default: return "UNKNOWN_STATE";
    }
}

// Helper function to convert SignalType enum to string for logging and debugging.
std::string sentio::PositionStateMachine::signal_type_to_string(SignalType st) {
    switch (st) {
        case SignalType::STRONG_BUY: return "STRONG_BUY";
        case SignalType::WEAK_BUY: return "WEAK_BUY";
        case SignalType::WEAK_SELL: return "WEAK_SELL";
        case SignalType::STRONG_SELL: return "STRONG_SELL";
        case SignalType::NEUTRAL: return "NEUTRAL";
        default: return "UNKNOWN_SIGNAL";
    }
}

