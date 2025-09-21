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
    double buy_threshold = 0.6;
    double sell_threshold = 0.4;
    
    // ADAPTIVE THRESHOLD INTEGRATION: Get optimal thresholds if enabled
    if (config_.enable_adaptive_thresholds && adaptive_threshold_manager_) {
        ThresholdPair adaptive_thresholds = adaptive_threshold_manager_->get_current_thresholds(signal, bar);
        buy_threshold = adaptive_thresholds.buy_threshold;
        sell_threshold = adaptive_thresholds.sell_threshold;
        
        utils::log_debug("ADAPTIVE THRESHOLDS: Buy=" + std::to_string(buy_threshold) + 
                        ", Sell=" + std::to_string(sell_threshold) + 
                        " (Signal=" + std::to_string(signal.probability) + ")");
    } else {
        // Fallback to static thresholds from configuration
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
        
        utils::log_debug("STATIC THRESHOLDS: Buy=" + std::to_string(buy_threshold) + 
                        ", Sell=" + std::to_string(sell_threshold) + 
                        " (Signal=" + std::to_string(signal.probability) + ")");
    }

    if (signal.probability > buy_threshold && signal.confidence > 0.5) {
        // Buy signal - aggressive capital allocation for maximum profit
        if (!portfolio_manager_->has_position(order.symbol)) {
            // Check for leverage conflicts BEFORE calculating size
            if (config_.enable_conflict_prevention) {
                auto conflict_reason = leverage_validator_->would_cause_conflict(order.symbol, portfolio_manager_->get_all_positions());
                if (!conflict_reason.empty()) {
                    order.action = TradeAction::HOLD;
                    order.rejection_reason = conflict_reason;
                    utils::log_debug("Trade rejected due to leverage conflict: " + conflict_reason);
                    return order;
                }
            }
            
            double available_capital = portfolio_manager_->get_cash_balance();
            
            // Validate available capital
            if (available_capital < 0.0) {
                utils::log_error("CRITICAL: Negative available_capital=" + std::to_string(available_capital) + 
                                " for symbol=" + order.symbol + ", cannot execute buy");
                std::abort();
            }
            
            double base_position_size = calculate_position_size(signal.probability, available_capital);
            double position_size = base_position_size;
            
            // Apply leverage risk adjustment if leverage trading is enabled
            if (config_.leverage_enabled) {
                position_size = calculate_risk_adjusted_size(order.symbol, base_position_size);
                utils::log_debug("Leverage risk adjustment: base_size=" + std::to_string(base_position_size) + 
                               ", adjusted_size=" + std::to_string(position_size) + " for symbol=" + order.symbol);
            }
            
            // Validate position size calculation
            if (position_size < 0.0 || std::isnan(position_size) || std::isinf(position_size)) {
                utils::log_error("CRITICAL: Invalid position_size=" + std::to_string(position_size) + 
                                " for symbol=" + order.symbol + ", available_capital=" + std::to_string(available_capital));
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
            // CASE 2: EXISTING POSITION - Evaluate scaling, profit-taking, or rebalancing opportunities
            Position existing_pos = portfolio_manager_->get_position(order.symbol);
            
            // Calculate current position metrics
            double current_pnl_percent = (bar.close - existing_pos.avg_price) / existing_pos.avg_price;
            double position_value = existing_pos.quantity * bar.close;
            double total_portfolio_value = portfolio_manager_->get_total_equity();
            double position_weight = (total_portfolio_value > 0) ? position_value / total_portfolio_value : 0.0;
            
            utils::log_debug("EXISTING POSITION ANALYSIS: " + order.symbol + 
                           " | P&L: " + std::to_string(current_pnl_percent * 100) + "%" +
                           " | Weight: " + std::to_string(position_weight * 100) + "%" +
                           " | Signal: " + std::to_string(signal.probability) + 
                           " | Confidence: " + std::to_string(signal.confidence));
            
            // STRATEGY 1: Scale up winning positions with very strong signals
            if (current_pnl_percent > 0.02 &&           // Position is profitable (>2%)
                signal.probability > 0.7 &&             // Very strong signal
                signal.confidence > 0.8 &&              // High confidence
                position_weight < 0.6) {                // Position not too large (<60%)
                
                // Calculate scaling amount (20-40% of existing position)
                double scale_factor = 0.2 + (signal.probability - 0.7) * 0.67; // 0.2 to 0.4
                scale_factor = std::clamp(scale_factor, 0.2, 0.4);
                
                double additional_shares = existing_pos.quantity * scale_factor;
                double scaling_value = additional_shares * bar.close;
                double available_cash = portfolio_manager_->get_cash_balance();
                
                if (available_cash >= scaling_value * 1.1) { // 10% buffer for fees
                    order.quantity = additional_shares;
                    order.trade_value = scaling_value;
                    order.fees = calculate_fees(order.trade_value);
                    order.action = TradeAction::BUY;
                    order.execution_reason = "SCALING UP: Adding " + std::to_string(scale_factor * 100) + 
                                           "% to profitable position (P&L: " + std::to_string(current_pnl_percent * 100) + 
                                           "%, Signal: " + std::to_string(signal.probability) + ")";
                    
                    utils::log_debug("SCALING BUY: " + std::to_string(additional_shares) + 
                                   " shares (" + std::to_string(scale_factor * 100) + "% increase)");
                } else {
                    order.action = TradeAction::HOLD;
                    order.execution_reason = "SCALING BLOCKED: Insufficient cash for position scaling";
                    utils::log_debug("Scaling blocked: need " + std::to_string(scaling_value) + 
                                   ", have " + std::to_string(available_cash));
                }
                
            // STRATEGY 2: Rebalance based on signal strength for breakeven positions
            } else if (signal.probability > 0.65 &&            // Good signal
                      std::abs(current_pnl_percent) < 0.03 &&  // Position near breakeven
                      position_weight < 0.5) {                 // Room to grow
                
                // Add 10-25% to position based on signal strength
                double rebalance_factor = 0.1 + (signal.probability - 0.65) * 0.43; // 0.1 to 0.25
                rebalance_factor = std::clamp(rebalance_factor, 0.1, 0.25);
                
                double additional_shares = existing_pos.quantity * rebalance_factor;
                double rebalance_value = additional_shares * bar.close;
                double available_cash = portfolio_manager_->get_cash_balance();
                
                if (available_cash >= rebalance_value * 1.1) { // 10% buffer for fees
                    order.quantity = additional_shares;
                    order.trade_value = rebalance_value;
                    order.fees = calculate_fees(order.trade_value);
                    order.action = TradeAction::BUY;
                    order.execution_reason = "REBALANCING: Adding " + std::to_string(rebalance_factor * 100) + 
                                           "% based on signal strength " + std::to_string(signal.probability);
                    
                    utils::log_debug("REBALANCE BUY: " + std::to_string(additional_shares) + 
                                   " shares (" + std::to_string(rebalance_factor * 100) + "% increase)");
                } else {
                    order.action = TradeAction::HOLD;
                    order.execution_reason = "REBALANCING BLOCKED: Insufficient cash";
                }
                
            } else {
                // No scaling or rebalancing opportunity
            order.action = TradeAction::HOLD;
                order.execution_reason = "EXISTING POSITION: No scaling opportunity " +
                                       std::string("(P&L: ") + std::to_string(current_pnl_percent * 100) + "%, " +
                                       "Weight: " + std::to_string(position_weight * 100) + "%, " +
                                       "Signal: " + std::to_string(signal.probability) + ")";
                
                utils::log_debug("HOLD: Existing position, no scaling criteria met");
            }
        }
    } else if (signal.probability < sell_threshold) {
        // Sell signal - EMERGENCY FIX: Sell existing positions instead of trading inverse instruments
        // Find and sell any existing positions regardless of the selected trade symbol
        std::string position_to_sell = "";
        
        // Check for existing positions in order of preference (highest leverage first)
        if (portfolio_manager_->has_position("TQQQ")) {
            position_to_sell = "TQQQ";
        } else if (portfolio_manager_->has_position("QQQ")) {
            position_to_sell = "QQQ";
        } else if (portfolio_manager_->has_position("SQQQ")) {
            position_to_sell = "SQQQ";
        } else if (portfolio_manager_->has_position("PSQ")) {
            position_to_sell = "PSQ";
        }
        
        if (!position_to_sell.empty()) {
            // Override the order symbol to sell the actual position we hold
            order.symbol = position_to_sell;
            utils::log_debug("SELL SIGNAL FIX: Overriding trade symbol to " + position_to_sell + " (actual position held)");
        }
        
        if (portfolio_manager_->has_position(order.symbol)) {
            auto position = portfolio_manager_->get_position(order.symbol);
            
            // Validate position data
            if (position.quantity <= 0.0 || std::isnan(position.quantity) || std::isinf(position.quantity)) {
                utils::log_error("CRITICAL: Invalid position.quantity=" + std::to_string(position.quantity) + 
                                " for symbol=" + order.symbol + " during sell signal");
                std::abort();
            }
            
            // Calculate position metrics for intelligent selling
            double current_pnl_percent = (bar.close - position.avg_price) / position.avg_price;
            double position_value = position.quantity * bar.close;
            double total_portfolio_value = portfolio_manager_->get_total_equity();
            double position_weight = (total_portfolio_value > 0) ? position_value / total_portfolio_value : 0.0;
            
            utils::log_debug("SELL ANALYSIS: " + order.symbol + 
                           " | P&L: " + std::to_string(current_pnl_percent * 100) + "%" +
                           " | Weight: " + std::to_string(position_weight * 100) + "%" +
                           " | Signal: " + std::to_string(signal.probability) + 
                           " | Sell Threshold: " + std::to_string(sell_threshold));
            
            // STRATEGY 1: Full liquidation for strong sell signals or significant losses
            if (signal.probability < sell_threshold - 0.05 ||  // Very strong sell signal
                current_pnl_percent < -0.05 ||               // Significant loss (>5%)
                position_weight > 0.7) {                     // Over-concentrated position
                
                order.quantity = position.quantity; // Full liquidation
                order.trade_value = order.quantity * bar.close;
                order.fees = calculate_fees(order.trade_value);
                order.action = TradeAction::SELL;
                order.execution_reason = "FULL LIQUIDATION: Strong sell signal " + std::to_string(signal.probability) + 
                                       " < " + std::to_string(sell_threshold) + 
                                       " (P&L: " + std::to_string(current_pnl_percent * 100) + "%)";
                
                utils::log_debug("FULL SELL: " + std::to_string(order.quantity) + " shares (100% liquidation)");
                
            // STRATEGY 2: Partial profit-taking for profitable positions with weak signals
            } else if (current_pnl_percent > 0.03 &&         // Position is profitable (>3%)
                      signal.probability < sell_threshold + 0.02) { // Moderately weak signal
                
                // Sell 30-60% of position based on signal weakness
                double sell_percentage = 0.3 + (sell_threshold - signal.probability) * 5.0; // 0.3 to 0.6
                sell_percentage = std::clamp(sell_percentage, 0.3, 0.6);
                
                order.quantity = position.quantity * sell_percentage;
                order.trade_value = order.quantity * bar.close;
                order.fees = calculate_fees(order.trade_value);
                order.action = TradeAction::SELL;
                order.execution_reason = "PROFIT TAKING: Selling " + std::to_string(sell_percentage * 100) + 
                                       "% of profitable position (P&L: " + std::to_string(current_pnl_percent * 100) + 
                                       "%, Signal: " + std::to_string(signal.probability) + ")";
                
                utils::log_debug("PARTIAL SELL: " + std::to_string(order.quantity) + 
                               " shares (" + std::to_string(sell_percentage * 100) + "% of position)");
                
            // STRATEGY 3: Risk reduction for breakeven positions with sell signals
            } else if (std::abs(current_pnl_percent) < 0.02) { // Near breakeven
                
                // Sell 25-40% to reduce risk
                double risk_reduction = 0.25 + (sell_threshold - signal.probability) * 3.0; // 0.25 to 0.4
                risk_reduction = std::clamp(risk_reduction, 0.25, 0.4);
                
                order.quantity = position.quantity * risk_reduction;
                order.trade_value = order.quantity * bar.close;
                order.fees = calculate_fees(order.trade_value);
                order.action = TradeAction::SELL;
                order.execution_reason = "RISK REDUCTION: Selling " + std::to_string(risk_reduction * 100) + 
                                       "% to reduce exposure (breakeven position, weak signal)";
                
                utils::log_debug("RISK REDUCTION SELL: " + std::to_string(order.quantity) + 
                               " shares (" + std::to_string(risk_reduction * 100) + "% reduction)");
                
            } else {
                // Default: Full liquidation for other cases
            order.quantity = position.quantity;
            order.trade_value = order.quantity * bar.close;
            order.fees = calculate_fees(order.trade_value);
            order.action = TradeAction::SELL;
                order.execution_reason = "STANDARD SELL: Signal " + std::to_string(signal.probability) + 
                                       " < " + std::to_string(sell_threshold) + " - full liquidation";
                
                utils::log_debug("STANDARD SELL: " + std::to_string(order.quantity) + " shares (full position)");
            }
            
            // Validate trade value calculation
            if (order.trade_value <= 0.0 || std::isnan(order.trade_value) || std::isinf(order.trade_value)) {
                utils::log_error("CRITICAL: Invalid sell trade_value=" + std::to_string(order.trade_value) + 
                                " for symbol=" + signal.symbol + ", quantity=" + std::to_string(order.quantity) + 
                                ", price=" + std::to_string(bar.close));
                std::abort();
            }
            
        } else {
            order.action = TradeAction::HOLD;
            order.execution_reason = "No position to sell";
            utils::log_debug("HOLD: No position to sell in " + order.symbol);
        }
    } else {
        order.action = TradeAction::HOLD;
        order.execution_reason = "Signal in neutral zone";
        utils::log_debug("HOLD: Signal in neutral zone for " + signal.symbol + 
                        ", probability=" + std::to_string(signal.probability));
    }

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
    order.execution_reason = transition.optimal_action + " (" + transition.theoretical_basis + ")";
    
    // Convert PSM state transition to concrete trade action
    if (transition.current_state == transition.target_state) {
        // No state change - HOLD
        order.action = TradeAction::HOLD;
        return order;
    }
    
    // Determine the appropriate symbol and action based on state transition
    std::string target_symbol = determine_target_symbol(transition);
    if (target_symbol != "HOLD") {
        order.symbol = target_symbol;
    }
    
    // Determine action based on transition type
    if (is_buy_transition(transition)) {
        order.action = TradeAction::BUY;
        
        // Calculate position size
        double available_capital = portfolio_manager_->get_cash_balance();
        double position_size = calculate_position_size(signal.probability, available_capital);
        
        if (position_size > 0.0) {
            order.quantity = position_size / bar.close;
            order.trade_value = order.quantity * bar.close;
            order.fees = calculate_fees(order.trade_value);
            
            // Validate we can execute this buy
            if (!portfolio_manager_->can_buy(order.symbol, order.quantity, bar.close, order.fees)) {
                order.action = TradeAction::HOLD;
                order.execution_reason = "Insufficient capital for PSM buy transition";
            }
        } else {
            order.action = TradeAction::HOLD;
            order.execution_reason = "No capital available for PSM buy transition";
        }
        
    } else if (is_sell_transition(transition)) {
        order.action = TradeAction::SELL;
        
        // Find position to sell
        if (portfolio_manager_->has_position(order.symbol)) {
            auto position = portfolio_manager_->get_position(order.symbol);
            
            // For now, sell entire position (can be refined later)
            order.quantity = position.quantity;
            order.trade_value = order.quantity * bar.close;
            order.fees = calculate_fees(order.trade_value);
        } else {
            order.action = TradeAction::HOLD;
            order.execution_reason = "No position to sell for PSM sell transition";
        }
    } else {
        // Default to HOLD for complex transitions
        order.action = TradeAction::HOLD;
    }
    
    return order;
}

std::string BackendComponent::determine_target_symbol(const PositionStateMachine::StateTransition& transition) {
    // Map PSM state transitions to specific symbols
    using State = PositionStateMachine::State;
    
    switch (transition.target_state) {
        case State::CASH_ONLY:
            return "HOLD"; // Liquidation - will be handled by sell logic
        case State::QQQ_ONLY:
            return "QQQ";
        case State::TQQQ_ONLY:
            return "TQQQ";
        case State::PSQ_ONLY:
            return "PSQ";
        case State::SQQQ_ONLY:
            return "SQQQ";
        case State::QQQ_TQQQ:
            // For dual states, choose based on current state
            if (transition.current_state == State::QQQ_ONLY) {
                return "TQQQ"; // Add TQQQ to existing QQQ
            } else if (transition.current_state == State::TQQQ_ONLY) {
                return "QQQ"; // Add QQQ to existing TQQQ
            } else {
                return "QQQ"; // Default to QQQ for other cases
            }
        case State::PSQ_SQQQ:
            // For dual short states
            if (transition.current_state == State::PSQ_ONLY) {
                return "SQQQ"; // Add SQQQ to existing PSQ
            } else if (transition.current_state == State::SQQQ_ONLY) {
                return "PSQ"; // Add PSQ to existing SQQQ
            } else {
                return "PSQ"; // Default to PSQ for other cases
            }
        case State::INVALID:
        default:
            return "HOLD";
    }
}

bool BackendComponent::is_buy_transition(const PositionStateMachine::StateTransition& transition) {
    using State = PositionStateMachine::State;
    
    // Check if transition involves acquiring new positions
    if (transition.current_state == State::CASH_ONLY && transition.target_state != State::CASH_ONLY) {
        return true; // Moving from cash to any position is a buy
    }
    
    // Check for position scaling (adding to existing positions)
    if ((transition.current_state == State::QQQ_ONLY && transition.target_state == State::QQQ_TQQQ) ||
        (transition.current_state == State::TQQQ_ONLY && transition.target_state == State::QQQ_TQQQ) ||
        (transition.current_state == State::PSQ_ONLY && transition.target_state == State::PSQ_SQQQ) ||
        (transition.current_state == State::SQQQ_ONLY && transition.target_state == State::PSQ_SQQQ)) {
        return true; // Scaling up positions
    }
    
    // Check for same-state transitions that involve adding to positions
    if (transition.current_state == transition.target_state &&
        (transition.optimal_action.find("Add") != std::string::npos ||
         transition.optimal_action.find("Scale") != std::string::npos)) {
        return true;
    }
    
    return false;
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
        return true; // De-leveraging
    }
    
    // Check for same-state transitions that involve selling
    if (transition.current_state == transition.target_state &&
        (transition.optimal_action.find("liquidation") != std::string::npos ||
         transition.optimal_action.find("Selling") != std::string::npos ||
         transition.optimal_action.find("Partial") != std::string::npos)) {
        return true;
    }
    
    return false;
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

// --- NEW LEVERAGE IMPLEMENTATIONS ---

// Implements requirement 4.2 - Select optimal leverage instrument
std::string BackendComponent::select_optimal_instrument(double probability, double confidence) const {
    return leverage_selector_->select_optimal_instrument(probability, confidence);
}

// Implements requirement 4.3 - Calculate risk-adjusted position size
double BackendComponent::calculate_risk_adjusted_size(const std::string& symbol, double base_position_size) const {
    return leverage_selector_->calculate_risk_adjusted_size(symbol, base_position_size);
}

} // namespace sentio


