#include "backend/backend_component.h"
#include "backend/portfolio_manager.h"
#include "backend/adaptive_portfolio_manager.h"
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
    leverage_validator_ = std::make_unique<LeveragePositionValidator>(); // <-- NEW
    leverage_selector_ = std::make_unique<LeverageSelector>(); // <-- NEW
    
    // Initialize adaptive threshold manager if enabled
    if (config.enable_adaptive_thresholds) {
        adaptive_threshold_manager_ = std::make_unique<AdaptiveThresholdManager>(config.adaptive_config);
        utils::log_info("Adaptive threshold optimization ENABLED - learning algorithm: " + 
                       std::to_string(static_cast<int>(config.adaptive_config.algorithm)));
    } else {
        utils::log_info("Adaptive threshold optimization DISABLED - using static thresholds");
    }
    
    // CRITICAL: Initialize Adaptive Portfolio Manager to fix phantom sell orders
    adaptive_portfolio_manager_ = std::make_unique<AdaptivePortfolioManager>(config.starting_capital);
    
    // Initialize Position State Machine for systematic decision making
    position_state_machine_ = std::make_unique<PositionStateMachine>();
    
    // Initialize Momentum Scalper if enabled
    if (config.enable_momentum_scalping) {
        momentum_scalper_ = std::make_unique<RegimeAdaptiveMomentumScalper>(config.scalper_config);
        utils::log_info("MOMENTUM SCALPER ENABLED - High-frequency regime-adaptive trading");
        utils::log_info("Target: 100+ daily trades, ~10% monthly returns, trend-following");
    } else {
        utils::log_info("Momentum scalping DISABLED - using standard PSM logic");
    }
    
    utils::log_info("BACKEND COMPONENT INITIALIZED with Position State Machine - Systematic trading enabled");
    utils::log_debug("PSM replaces ad-hoc logic with 32 theoretically-sound state transitions");
}

BackendComponent::~BackendComponent() {
    // No-op (file-based trade books)
}

std::vector<BackendComponent::TradeOrder> BackendComponent::process_signals(
    const std::string& signal_file_path,
    const std::string& market_data_path,
    const BackendConfig& /*config*/,
    size_t start_index,
    size_t end_index) {

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

    // Determine actual processing range
    size_t actual_end = std::min({end_index, signals.size(), bars.size()});
    
    utils::log_info("Processing signals from index " + std::to_string(start_index) + 
                   " to " + std::to_string(actual_end) + " (fresh $" + 
                   std::to_string(config_.starting_capital) + " capital)");

    // Process each signal with corresponding bar in the specified range
    for (size_t i = start_index; i < actual_end; ++i) {
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
            // Store pre-execution portfolio value for adaptive learning
            double pre_execution_value = portfolio_manager_->get_total_equity();
            
            if (order.action == TradeAction::BUY) {
                portfolio_manager_->execute_buy(order.symbol, order.quantity, order.price, order.fees);
                position_manager_->add_position(order.symbol);
                position_manager_->update_direction(StaticPositionManager::Direction::LONG);
            } else if (order.action == TradeAction::SELL) {
                portfolio_manager_->execute_sell(order.symbol, order.quantity, order.price, order.fees);
                position_manager_->remove_position(order.symbol);
            }
            
            // Provide feedback to adaptive threshold manager
            if (config_.enable_adaptive_thresholds && adaptive_threshold_manager_) {
                double post_execution_value = portfolio_manager_->get_total_equity();
                double immediate_pnl = post_execution_value - pre_execution_value;
                bool was_profitable = immediate_pnl > 0.0;
                double pnl_percentage = (pre_execution_value > 0) ? (immediate_pnl / pre_execution_value) : 0.0;
                
                adaptive_threshold_manager_->process_trade_outcome(order.symbol, order.action, order.quantity, 
                                                                  order.price, order.trade_value, order.fees,
                                                                  immediate_pnl, pnl_percentage, was_profitable);
                adaptive_threshold_manager_->update_portfolio_value(post_execution_value);
                
                utils::log_debug("ADAPTIVE FEEDBACK: PnL=" + std::to_string(immediate_pnl) + 
                               ", Profitable=" + (was_profitable ? "YES" : "NO") + 
                               ", Portfolio=" + std::to_string(post_execution_value));
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

    // --- TRADING ALGORITHM SELECTION ---
    // Choose between momentum scalper and standard PSM based on configuration
    
    PositionStateMachine::StateTransition psm_transition;
    
    if (config_.enable_momentum_scalping && momentum_scalper_) {
        // Use high-frequency momentum scalper
        PortfolioState current_portfolio = portfolio_manager_->get_state();
        psm_transition = momentum_scalper_->process_bar(bar, signal, current_portfolio);
        
        utils::log_info("MOMENTUM SCALPER: " + 
                       PositionStateMachine::state_to_string(psm_transition.current_state) + 
                       " -> " + PositionStateMachine::state_to_string(psm_transition.target_state) + 
                       " | Regime: " + (momentum_scalper_->get_current_regime() == 
                                       RegimeAdaptiveMomentumScalper::MarketRegime::UPTREND ? "UPTREND" :
                                       momentum_scalper_->get_current_regime() == 
                                       RegimeAdaptiveMomentumScalper::MarketRegime::DOWNTREND ? "DOWNTREND" : "NEUTRAL"));
        utils::log_info("SCALPER ACTION: " + psm_transition.optimal_action + " (" + psm_transition.theoretical_basis + ")");
        
    } else {
        // Use standard Position State Machine
        PortfolioState current_portfolio = portfolio_manager_->get_state();
        
        // Create market conditions (simplified for now)
        MarketState market_conditions;
        market_conditions.volatility = 0.2; // Default volatility
        market_conditions.trend_strength = (signal.probability - 0.5) * 2.0; // Convert signal to trend
        market_conditions.volume_ratio = 1.0; // Default volume ratio
        
        // Get optimal state transition from PSM
        psm_transition = position_state_machine_->get_optimal_transition(
            current_portfolio, signal, market_conditions
        );
        
        utils::log_info("PSM TRANSITION: " + PositionStateMachine::state_to_string(psm_transition.current_state) + 
                       " + " + PositionStateMachine::signal_type_to_string(psm_transition.signal_type) + 
                       " -> " + PositionStateMachine::state_to_string(psm_transition.target_state));
        utils::log_info("PSM ACTION: " + psm_transition.optimal_action + " (" + psm_transition.theoretical_basis + ")");
    }
    
    // Convert PSM transition to trade order
    order = convert_psm_transition_to_order(psm_transition, signal, bar);
    
    utils::log_debug("PSM-based signal evaluation: symbol=" + signal.symbol + 
                    ", probability=" + std::to_string(signal.probability) + 
                    ", confidence=" + std::to_string(signal.confidence) + 
                    ", price=" + std::to_string(bar.close));

    return order;
}

BackendComponent::TradeOrder BackendComponent::convert_psm_transition_to_order(
    const PositionStateMachine::StateTransition& transition,
    const SignalOutput& signal,
    const Bar& bar) {
    
    TradeOrder order;
    order.timestamp_ms = signal.timestamp_ms;
    order.bar_index = signal.bar_index;
    order.symbol = signal.symbol; // Default to signal symbol
    order.signal_probability = signal.probability;
    order.signal_confidence = signal.confidence;
    order.price = bar.close;
    order.before_state = portfolio_manager_->get_state();
    
    // Determine the target symbol based on PSM transition
    std::string target_symbol = determine_target_symbol(transition);
    if (!target_symbol.empty() && target_symbol != "HOLD") {
        order.symbol = target_symbol;
    }
    
    // Determine action based on transition type
    if (is_buy_transition(transition)) {
        order.action = TradeAction::BUY;
        
        // Calculate position size
        double available_capital = portfolio_manager_->get_cash_balance();
        double position_size = calculate_position_size(signal.probability, available_capital);
        
        // Apply leverage risk adjustment if needed
        if (config_.leverage_enabled) {
            position_size = calculate_risk_adjusted_size(order.symbol, position_size);
        }
        
        order.quantity = position_size / bar.close;
        order.trade_value = position_size;
        order.fees = calculate_fees(order.trade_value);
        order.execution_reason = "PSM BUY: " + transition.optimal_action + " (" + transition.theoretical_basis + ")";
        
    } else if (is_sell_transition(transition)) {
        order.action = TradeAction::SELL;
        
        // Get position to sell
        if (portfolio_manager_->has_position(order.symbol)) {
            auto position = portfolio_manager_->get_position(order.symbol);
            order.quantity = position.quantity; // Full liquidation for now
            order.trade_value = order.quantity * bar.close;
            order.fees = calculate_fees(order.trade_value);
            order.execution_reason = "PSM SELL: " + transition.optimal_action + " (" + transition.theoretical_basis + ")";
        } else {
            // No position to sell - hold
            order.action = TradeAction::HOLD;
            order.execution_reason = "PSM HOLD: No position to sell";
        }
        
    } else {
        order.action = TradeAction::HOLD;
        order.execution_reason = "PSM HOLD: " + transition.optimal_action + " (" + transition.theoretical_basis + ")";
    }
    
    return order;
}

std::string BackendComponent::determine_target_symbol(const PositionStateMachine::StateTransition& transition) {
    using State = PositionStateMachine::State;
    
    switch (transition.target_state) {
        case State::CASH_ONLY:
            return determine_liquidation_symbol(transition.current_state);
        case State::QQQ_ONLY:
            return "QQQ";
        case State::TQQQ_ONLY:
            return config_.leverage_enabled ? "TQQQ" : "QQQ";
        case State::PSQ_ONLY:
            return config_.leverage_enabled ? "PSQ" : "QQQ";
        case State::SQQQ_ONLY:
            return config_.leverage_enabled ? "SQQQ" : "QQQ";
        case State::QQQ_TQQQ:
            return determine_dual_long_symbol(transition.current_state);
        case State::PSQ_SQQQ:
            return determine_dual_short_symbol(transition.current_state);
        default:
            return "HOLD";
    }
}

std::string BackendComponent::determine_liquidation_symbol(PositionStateMachine::State current_state) {
    using State = PositionStateMachine::State;
    
    switch (current_state) {
        case State::QQQ_ONLY:
            return "QQQ";
        case State::TQQQ_ONLY:
            return config_.leverage_enabled ? "TQQQ" : "QQQ";
        case State::PSQ_ONLY:
            return config_.leverage_enabled ? "PSQ" : "QQQ";
        case State::SQQQ_ONLY:
            return config_.leverage_enabled ? "SQQQ" : "QQQ";
        case State::QQQ_TQQQ:
            // For dual positions, sell the leveraged one first (higher risk)
            return config_.leverage_enabled ? "TQQQ" : "QQQ";
        case State::PSQ_SQQQ:
            // For dual short positions, sell the leveraged one first
            return config_.leverage_enabled ? "SQQQ" : "QQQ";
        default:
            return "HOLD"; // Already in cash or unknown state
    }
}

std::string BackendComponent::determine_dual_long_symbol(PositionStateMachine::State current_state) {
    using State = PositionStateMachine::State;
    
    if (!config_.leverage_enabled) {
        return "QQQ"; // No leverage - only trade QQQ
    }
    
    // With leverage enabled, determine which symbol to trade based on current state
    if (current_state == State::QQQ_ONLY) {
        return "TQQQ"; // Add TQQQ to existing QQQ
    } else if (current_state == State::TQQQ_ONLY) {
        return "QQQ"; // Add QQQ to existing TQQQ
    } else {
        return "QQQ"; // Default to QQQ for dual state
    }
}

std::string BackendComponent::determine_dual_short_symbol(PositionStateMachine::State current_state) {
    using State = PositionStateMachine::State;
    
    if (!config_.leverage_enabled) {
        return "QQQ"; // No leverage - only trade QQQ
    }
    
    // With leverage enabled
    if (current_state == State::PSQ_ONLY) {
        return "SQQQ"; // Add SQQQ to existing PSQ
    } else if (current_state == State::SQQQ_ONLY) {
        return "PSQ"; // Add PSQ to existing SQQQ
    } else {
        return "PSQ"; // Default to PSQ for dual short state
    }
}

bool BackendComponent::is_buy_transition(const PositionStateMachine::StateTransition& transition) {
    using State = PositionStateMachine::State;
    
    // Check if transition involves acquiring new positions from cash
    if (is_cash_to_position_transition(transition)) {
        return true;
    }
    
    // Check for position scaling (adding to existing positions)
    if (is_position_scaling_transition(transition)) {
        return true;
    }
    
    return false;
}

bool BackendComponent::is_cash_to_position_transition(const PositionStateMachine::StateTransition& transition) {
    using State = PositionStateMachine::State;
    return transition.current_state == State::CASH_ONLY && transition.target_state != State::CASH_ONLY;
}

bool BackendComponent::is_position_scaling_transition(const PositionStateMachine::StateTransition& transition) {
    using State = PositionStateMachine::State;
    
    // Check for long position scaling
    if (is_long_scaling_transition(transition)) {
        return true;
    }
    
    // Check for short position scaling
    if (is_short_scaling_transition(transition)) {
        return true;
    }
    
    return false;
}

bool BackendComponent::is_long_scaling_transition(const PositionStateMachine::StateTransition& transition) {
    using State = PositionStateMachine::State;
    
    return (transition.current_state == State::QQQ_ONLY && transition.target_state == State::QQQ_TQQQ) ||
           (transition.current_state == State::TQQQ_ONLY && transition.target_state == State::QQQ_TQQQ);
}

bool BackendComponent::is_short_scaling_transition(const PositionStateMachine::StateTransition& transition) {
    using State = PositionStateMachine::State;
    
    return (transition.current_state == State::PSQ_ONLY && transition.target_state == State::PSQ_SQQQ) ||
           (transition.current_state == State::SQQQ_ONLY && transition.target_state == State::PSQ_SQQQ);
}

bool BackendComponent::is_sell_transition(const PositionStateMachine::StateTransition& transition) {
    using State = PositionStateMachine::State;
    
    // Check if transition involves liquidating positions
    if (transition.target_state == State::CASH_ONLY && transition.current_state != State::CASH_ONLY) {
        return true; // Moving to cash from any position is a sell
    }
    
    // Check for position reduction (dual to single states)
    if ((transition.current_state == State::QQQ_TQQQ && 
         (transition.target_state == State::QQQ_ONLY || transition.target_state == State::TQQQ_ONLY)) ||
        (transition.current_state == State::PSQ_SQQQ &&
         (transition.target_state == State::PSQ_ONLY || transition.target_state == State::SQQQ_ONLY))) {
        return true; // Reducing dual position to single
    }
    
    return false;
}

bool BackendComponent::check_conflicts(const TradeOrder& order) {
    return position_manager_->would_cause_conflict(order.symbol, order.action);
}

double BackendComponent::calculate_fees(double trade_value) {
    // Validate trade value
    if (trade_value < 0.0 || std::isnan(trade_value) || std::isinf(trade_value)) {
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
    
    return fees;
}

double BackendComponent::calculate_position_size(double signal_probability, double available_capital) {
    // Validate inputs
    if (signal_probability < 0.0 || signal_probability > 1.0) {
        utils::log_error("CRITICAL: Invalid signal_probability=" + std::to_string(signal_probability) + 
                        " (must be 0.0-1.0) for available_capital=" + std::to_string(available_capital));
        std::abort();
    }
    
    if (available_capital < 0.0 || std::isnan(available_capital) || std::isinf(available_capital)) {
        utils::log_error("CRITICAL: Invalid available_capital=" + std::to_string(available_capital) + 
                        " (negative, NaN, or Inf) for signal_probability=" + std::to_string(signal_probability));
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

bool BackendComponent::process_to_jsonl(const std::string& signal_file_path, 
                                       const std::string& market_data_path, 
                                       const std::string& output_file_path, 
                                       const std::string& run_id,
                                       size_t start_index, 
                                       size_t end_index) {
    utils::log_info("Processing signals to JSONL: " + signal_file_path + " -> " + output_file_path);
    
    auto trades = process_signals(signal_file_path, market_data_path, config_);
    
    std::ofstream out(output_file_path);
    if (!out.is_open()) {
        utils::log_error("Cannot open output file: " + output_file_path);
        return false;
    }
    
    int processed = 0;
    int executed = 0;
    
    for (const auto& trade : trades) {
        if (processed >= (int)start_index && processed < (int)end_index) {
            out << trade.to_json_line(run_id) << "\n";
        }
        processed++;
        if (trade.action != TradeAction::HOLD) {
            executed++;
        }
    }
    
    std::cout << "Completed. Total signals: " << processed << ", executed trades: " << executed << std::endl;
    return true;
}

// --- LEVERAGE IMPLEMENTATIONS ---

// Implements requirement 4.2 - Select optimal instrument based on signal strength
std::string BackendComponent::select_optimal_instrument(double probability, double confidence) const {
    return leverage_selector_->select_optimal_instrument(probability, confidence);
}

// Implements requirement 4.3 - Calculate risk-adjusted position size
double BackendComponent::calculate_risk_adjusted_size(const std::string& symbol, double base_position_size) const {
    return leverage_selector_->calculate_risk_adjusted_size(symbol, base_position_size);
}

} // namespace sentio
