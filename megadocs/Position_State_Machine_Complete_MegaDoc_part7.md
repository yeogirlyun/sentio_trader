# Position State Machine Complete Implementation

**Part 7 of 8**

**Generated**: 2025-09-22 01:45:57
**Source Directory**: /Users/yeogirlyun/C++/sentio_trader
**Description**: Comprehensive requirements document and source code analysis for implementing a theoretically sound Position State Machine to replace ad-hoc trading logic with systematic state-based decision making. Includes complete requirements specification, current system analysis, and integration architecture.

**Part 7 Files**: See file count below

---

## 📋 **TABLE OF CONTENTS**

31. [src/backend/adaptive_portfolio_manager.cpp](#file-31)
32. [src/backend/adaptive_trading_mechanism.cpp](#file-32)
33. [src/backend/audit_component.cpp](#file-33)
34. [src/backend/backend_component.cpp](#file-34)
35. [src/backend/backend_component_fixed.cpp](#file-35)

---

## 📄 **FILE 31 of 39**: src/backend/adaptive_portfolio_manager.cpp

**File Information**:
- **Path**: `src/backend/adaptive_portfolio_manager.cpp`

- **Size**: 753 lines
- **Modified**: 2025-09-21 22:02:21

- **Type**: .cpp

```text
// =============================================================================
// Module: backend/adaptive_portfolio_manager.cpp
// Purpose: Implementation of the Adaptive Portfolio Manager that completely
//          eliminates phantom sell orders and provides intelligent trading.
//
// CRITICAL BUG FIX: This implementation prevents the phantom sell order bug
// by validating all sell orders before execution.
// =============================================================================

#include "backend/adaptive_portfolio_manager.h"
#include "common/utils.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace sentio {

// ===================================================================
// POSITION VALIDATOR IMPLEMENTATION
// ===================================================================

PositionValidator::ValidationResult PositionValidator::validate_sell_order(
    const std::string& symbol, 
    double requested_quantity,
    const std::map<std::string, Position>& positions) const {
    
    ValidationResult result;
    
    // CRITICAL CHECK 1: Position existence
    auto it = positions.find(symbol);
    if (it == positions.end()) {
        result.error_message = "Position does not exist for symbol: " + symbol;
        utils::log_warning("PHANTOM ORDER PREVENTED: " + result.error_message);
        return result;
    }
    
    const Position& position = it->second;
    
    // CRITICAL CHECK 2: Position has actual quantity
    if (std::abs(position.quantity) < 1e-6) {
        result.error_message = "Position exists but has zero quantity for symbol: " + symbol;
        utils::log_warning("PHANTOM ORDER PREVENTED: " + result.error_message);
        return result;
    }
    
    // CRITICAL CHECK 3: Sufficient quantity available
    if (requested_quantity > position.quantity + 1e-6) {
        result.error_message = "Insufficient quantity. Available: " + 
                             std::to_string(position.quantity) + 
                             ", Requested: " + std::to_string(requested_quantity);
        utils::log_warning("PHANTOM ORDER PREVENTED: " + result.error_message);
        return result;
    }
    
    // All validations passed - this is a legitimate sell order
    result.is_valid = true;
    result.validated_quantity = std::min(requested_quantity, position.quantity);
    utils::log_debug("SELL ORDER VALIDATED: Can sell " + std::to_string(result.validated_quantity) + 
                    " shares of " + symbol);
    
    return result;
}

bool PositionValidator::validate_buy_order(const std::string& symbol, 
                                         double quantity, 
                                         double price, 
                                         double available_cash,
                                         double fees) const {
    if (quantity <= 0) {
        utils::log_warning("Invalid buy quantity: " + std::to_string(quantity));
        return false;
    }
    
    if (price <= 0) {
        utils::log_warning("Invalid buy price: " + std::to_string(price));
        return false;
    }
    
    double required_cash = quantity * price + fees;
    if (required_cash > available_cash + 1e-6) {
        utils::log_warning("Insufficient cash. Required: " + std::to_string(required_cash) + 
                         ", Available: " + std::to_string(available_cash));
        return false;
    }
    
    return true;
}

// ===================================================================
// CONFLICT RESOLUTION ENGINE IMPLEMENTATION
// ===================================================================

ConflictResolutionEngine::ConflictAnalysis ConflictResolutionEngine::analyze_conflicts(
    const std::string& proposed_symbol,
    const std::map<std::string, Position>& current_positions,
    double current_price) const {
    
    ConflictAnalysis analysis;
    
    // Define conflicting instrument pairs based on leverage trading requirements
    static const std::map<std::string, std::vector<std::string>> conflicts = {
        {"TQQQ", {"SQQQ", "PSQ"}},  // 3x long conflicts with shorts
        {"SQQQ", {"TQQQ", "QQQ"}},  // 3x short conflicts with longs
        {"PSQ",  {"TQQQ", "QQQ"}},  // 1x short conflicts with longs
        {"QQQ",  {"SQQQ", "PSQ"}}   // 1x long conflicts with shorts
    };
    
    auto conflict_it = conflicts.find(proposed_symbol);
    if (conflict_it == conflicts.end()) {
        return analysis; // No known conflicts for this symbol
    }
    
    // Check for existing conflicting positions
    for (const std::string& conflicting_symbol : conflict_it->second) {
        auto pos_it = current_positions.find(conflicting_symbol);
        if (pos_it != current_positions.end() && std::abs(pos_it->second.quantity) > 1e-6) {
            analysis.has_conflicts = true;
            analysis.conflicting_symbols.push_back(conflicting_symbol);
            
            // Create liquidation order
            TradeOrder liquidation_order;
            liquidation_order.action = TradeAction::SELL;
            liquidation_order.symbol = conflicting_symbol;
            liquidation_order.quantity = pos_it->second.quantity;
            liquidation_order.price = current_price;
            liquidation_order.trade_value = liquidation_order.quantity * current_price;
            liquidation_order.execution_reason = "Conflict resolution: Liquidating " + 
                                               conflicting_symbol + " for " + proposed_symbol;
            
            analysis.liquidation_orders.push_back(liquidation_order);
        }
    }
    
    if (analysis.has_conflicts) {
        analysis.resolution_strategy = "AUTO_LIQUIDATE_CONFLICTS";
        utils::log_info("CONFLICT DETECTED: " + proposed_symbol + " conflicts with " + 
                       std::to_string(analysis.conflicting_symbols.size()) + " existing positions");
    }
    
    return analysis;
}

std::vector<TradeOrder> ConflictResolutionEngine::resolve_conflicts_automatically(
    const ConflictAnalysis& analysis) const {
    
    if (!analysis.has_conflicts) {
        return {};
    }
    
    utils::log_info("AUTO-RESOLVING CONFLICTS: Liquidating " + 
                   std::to_string(analysis.liquidation_orders.size()) + " conflicting positions");
    
    return analysis.liquidation_orders;
}

bool ConflictResolutionEngine::would_conflict(const std::string& proposed, const std::string& existing) const {
    static const std::map<std::string, std::vector<std::string>> conflicts = {
        {"TQQQ", {"SQQQ", "PSQ"}},
        {"SQQQ", {"TQQQ", "QQQ"}},
        {"PSQ",  {"TQQQ", "QQQ"}},
        {"QQQ",  {"SQQQ", "PSQ"}}
    };
    
    auto it = conflicts.find(proposed);
    if (it != conflicts.end()) {
        return std::find(it->second.begin(), it->second.end(), existing) != it->second.end();
    }
    
    return false;
}

// ===================================================================
// CASH BALANCE PROTECTOR IMPLEMENTATION
// ===================================================================

CashBalanceProtector::CashBalanceProtector(double min_reserve) 
    : minimum_cash_reserve_(min_reserve) {}

CashBalanceProtector::CashValidationResult CashBalanceProtector::validate_transaction(
    const TradeOrder& order, 
    double current_cash,
    double fee_rate) const {
    
    CashValidationResult result;
    
    if (order.action == TradeAction::SELL) {
        // Selling adds cash, always valid from cash perspective
        double fees = order.trade_value * fee_rate;
        result.projected_cash = current_cash + order.trade_value - fees;
        result.is_valid = true;
        return result;
    }
    
    if (order.action == TradeAction::BUY) {
        double fees = order.trade_value * fee_rate;
        double total_cost = order.trade_value + fees;
        
        result.projected_cash = current_cash - total_cost;
        
        // Check if we maintain minimum reserve
        if (result.projected_cash < minimum_cash_reserve_) {
            result.error_message = "Transaction would violate minimum cash reserve. " +
                                 std::string("Projected cash: ") + std::to_string(result.projected_cash) + 
                                 ", Minimum reserve: " + std::to_string(minimum_cash_reserve_);
            
            // Calculate maximum affordable quantity
            double available_for_trading = current_cash - minimum_cash_reserve_;
            if (available_for_trading > 0 && order.price > 0) {
                result.max_affordable_quantity = available_for_trading / (order.price * (1.0 + fee_rate));
            }
            
            utils::log_warning("CASH VALIDATION FAILED: " + result.error_message);
            return result;
        }
        
        result.is_valid = true;
        utils::log_debug("CASH VALIDATION PASSED: Projected cash balance: " + 
                       std::to_string(result.projected_cash));
    }
    
    return result;
}

TradeOrder CashBalanceProtector::adjust_order_for_cash_constraints(
    const TradeOrder& original_order,
    double current_cash,
    double fee_rate) const {
    
    auto validation = validate_transaction(original_order, current_cash, fee_rate);
    
    if (validation.is_valid) {
        return original_order;
    }
    
    if (validation.max_affordable_quantity > 1e-6) {
        TradeOrder adjusted_order = original_order;
        adjusted_order.quantity = validation.max_affordable_quantity;
        adjusted_order.trade_value = adjusted_order.quantity * adjusted_order.price;
        adjusted_order.execution_reason = "Adjusted for cash constraints: " + original_order.execution_reason;
        
        utils::log_info("ORDER ADJUSTED: Reduced quantity from " + 
                       std::to_string(original_order.quantity) + " to " + 
                       std::to_string(adjusted_order.quantity) + " due to cash constraints");
        
        return adjusted_order;
    }
    
    // Cannot afford any quantity, return HOLD order
    TradeOrder hold_order;
    hold_order.action = TradeAction::HOLD;
    hold_order.symbol = original_order.symbol;
    hold_order.execution_reason = "Insufficient cash for trade: " + validation.error_message;
    
    return hold_order;
}

// ===================================================================
// PROFIT MAXIMIZATION ENGINE IMPLEMENTATION
// ===================================================================

std::string ProfitMaximizationEngine::select_optimal_instrument(
    const SignalOutput& signal, 
    const std::map<std::string, Position>& current_positions) const {
    
    std::vector<InstrumentAnalysis> candidates;
    
    // Analyze all available instruments based on signal direction
    if (signal.probability > 0.5) {
        // Bullish signal - consider long instruments
        candidates.push_back(analyze_instrument("QQQ", signal, 1.0));    // 1x long
        candidates.push_back(analyze_instrument("TQQQ", signal, 3.0));   // 3x long
    } else {
        // Bearish signal - consider short instruments
        candidates.push_back(analyze_instrument("PSQ", signal, -1.0));   // 1x short
        candidates.push_back(analyze_instrument("SQQQ", signal, -3.0));  // 3x short
    }
    
    // Filter out instruments that would create conflicts
    candidates = filter_conflicting_instruments(candidates, current_positions);
    
    if (candidates.empty()) {
        utils::log_warning("No suitable instruments available after conflict filtering");
        return "HOLD";
    }
    
    // Select instrument with highest profit potential
    auto best_instrument = std::max_element(candidates.begin(), candidates.end(),
        [](const InstrumentAnalysis& a, const InstrumentAnalysis& b) {
            return a.final_score < b.final_score;
        });
    
    utils::log_info("OPTIMAL INSTRUMENT SELECTED: " + best_instrument->symbol + 
                   " (Score: " + std::to_string(best_instrument->final_score) + ")");
    
    return best_instrument->symbol;
}

ProfitMaximizationEngine::InstrumentAnalysis ProfitMaximizationEngine::analyze_instrument(
    const std::string& symbol, 
    const SignalOutput& signal,
    double leverage_factor) const {
    
    InstrumentAnalysis analysis;
    analysis.symbol = symbol;
    analysis.leverage_factor = leverage_factor;
    
    // Calculate profit potential based on signal strength and leverage
    double signal_strength = std::abs(signal.probability - 0.5) * 2.0; // 0 to 1
    analysis.profit_potential = signal_strength * std::abs(leverage_factor) * signal.confidence;
    
    // Calculate risk score (higher leverage = higher risk)
    analysis.risk_score = std::abs(leverage_factor) * 0.1; // Base risk
    analysis.risk_score += (1.0 - signal.confidence) * 0.5; // Confidence risk
    
    // Confidence adjustment
    analysis.confidence_adjustment = signal.confidence;
    
    // Final score balances profit potential against risk
    analysis.final_score = analysis.profit_potential * analysis.confidence_adjustment - 
                          analysis.risk_score * 0.5;
    
    return analysis;
}

std::vector<ProfitMaximizationEngine::InstrumentAnalysis> 
ProfitMaximizationEngine::filter_conflicting_instruments(
    const std::vector<InstrumentAnalysis>& candidates,
    const std::map<std::string, Position>& current_positions) const {
    
    std::vector<InstrumentAnalysis> filtered;
    
    for (const auto& candidate : candidates) {
        bool has_conflict = false;
        
        // Check for conflicts with existing positions
        for (const auto& [symbol, position] : current_positions) {
            if (std::abs(position.quantity) > 1e-6 && would_conflict(candidate.symbol, symbol)) {
                has_conflict = true;
                break;
            }
        }
        
        if (!has_conflict) {
            filtered.push_back(candidate);
        }
    }
    
    return filtered;
}

bool ProfitMaximizationEngine::would_conflict(const std::string& proposed, const std::string& existing) const {
    static const std::map<std::string, std::vector<std::string>> conflicts = {
        {"TQQQ", {"SQQQ", "PSQ"}},
        {"SQQQ", {"TQQQ", "QQQ"}},
        {"PSQ",  {"TQQQ", "QQQ"}},
        {"QQQ",  {"SQQQ", "PSQ"}}
    };
    
    auto it = conflicts.find(proposed);
    if (it != conflicts.end()) {
        return std::find(it->second.begin(), it->second.end(), existing) != it->second.end();
    }
    
    return false;
}

// ===================================================================
// RISK MANAGER IMPLEMENTATION
// ===================================================================

RiskManager::RiskManager(double max_pos_size, double max_portfolio_risk, double vol_adj)
    : max_position_size_(max_pos_size), max_portfolio_risk_(max_portfolio_risk), 
      volatility_adjustment_factor_(vol_adj) {}

RiskManager::RiskAnalysis RiskManager::calculate_optimal_position_size(
    const std::string& symbol,
    const SignalOutput& signal,
    double available_capital,
    double current_price,
    const std::map<std::string, Position>& positions) const {
    
    RiskAnalysis analysis;
    
    // Base position size based on signal confidence
    double base_size = signal.confidence * max_position_size_;
    
    // Adjust for leverage factor
    double leverage_factor = get_leverage_factor(symbol);
    double leverage_adjustment = 1.0 / std::sqrt(std::abs(leverage_factor));
    
    // Adjust for signal strength
    double signal_strength = std::abs(signal.probability - 0.5) * 2.0;
    double strength_adjustment = 0.5 + (signal_strength * 0.5);
    
    // Calculate recommended position size
    analysis.recommended_position_size = base_size * leverage_adjustment * strength_adjustment;
    
    // Ensure we don't exceed maximum position size
    analysis.recommended_position_size = std::min(analysis.recommended_position_size, max_position_size_);
    
    // Calculate maximum safe quantity
    double position_value = available_capital * analysis.recommended_position_size;
    analysis.max_safe_quantity = position_value / current_price;
    
    // Calculate risk score
    analysis.risk_score = calculate_portfolio_risk_score(symbol, analysis.max_safe_quantity, 
                                                       current_price, positions);
    
    // Determine risk level and warnings
    if (analysis.risk_score < 0.1) {
        analysis.risk_level = "LOW";
    } else if (analysis.risk_score < 0.25) {
        analysis.risk_level = "MEDIUM";
    } else if (analysis.risk_score < 0.5) {
        analysis.risk_level = "HIGH";
        analysis.risk_warnings.push_back("High portfolio risk detected");
    } else {
        analysis.risk_level = "EXTREME";
        analysis.risk_warnings.push_back("Extreme portfolio risk - position size reduced");
        analysis.recommended_position_size *= 0.5; // Reduce by 50%
        analysis.max_safe_quantity *= 0.5;
    }
    
    return analysis;
}

double RiskManager::get_leverage_factor(const std::string& symbol) const {
    static const std::map<std::string, double> leverage_factors = {
        {"QQQ", 1.0}, {"TQQQ", 3.0}, {"PSQ", -1.0}, {"SQQQ", -3.0}
    };
    
    auto it = leverage_factors.find(symbol);
    return (it != leverage_factors.end()) ? it->second : 1.0;
}

double RiskManager::calculate_portfolio_risk_score(
    const std::string& new_symbol,
    double new_quantity,
    double new_price,
    const std::map<std::string, Position>& positions) const {
    
    // Calculate value-weighted portfolio risk
    double total_portfolio_value = new_quantity * new_price;
    double total_leveraged_exposure = std::abs(get_leverage_factor(new_symbol)) * 
                                    new_quantity * new_price;
    
    for (const auto& [symbol, position] : positions) {
        double position_value = position.quantity * position.current_price;
        total_portfolio_value += position_value;
        total_leveraged_exposure += std::abs(get_leverage_factor(symbol)) * position_value;
    }
    
    if (total_portfolio_value < 1e-6) return 0.0;
    
    return total_leveraged_exposure / total_portfolio_value;
}

// ===================================================================
// ADAPTIVE PORTFOLIO MANAGER IMPLEMENTATION
// ===================================================================

AdaptivePortfolioManager::AdaptivePortfolioManager(double initial_cash) 
    : cash_balance_(initial_cash), total_portfolio_value_(initial_cash) {
    
    // Initialize all components
    position_validator_ = std::make_unique<PositionValidator>();
    conflict_resolver_ = std::make_unique<ConflictResolutionEngine>();
    cash_protector_ = std::make_unique<CashBalanceProtector>(config_.minimum_cash_reserve);
    profit_optimizer_ = std::make_unique<ProfitMaximizationEngine>();
    risk_manager_ = std::make_unique<RiskManager>();
    
    utils::log_info("ADAPTIVE PORTFOLIO MANAGER INITIALIZED with $" + 
                   std::to_string(initial_cash) + " starting capital");
}

std::vector<TradeOrder> AdaptivePortfolioManager::execute_adaptive_trade(
    const SignalOutput& signal, const Bar& bar) {
    
    std::vector<TradeOrder> orders;
    
    utils::log_info("EXECUTING ADAPTIVE TRADE: Signal=" + std::to_string(signal.probability) + 
                   ", Confidence=" + std::to_string(signal.confidence) + 
                   ", Symbol=" + signal.symbol);
    
    // 1. Validate inputs
    if (!validate_inputs(signal, bar)) {
        orders.push_back(create_hold_order("Invalid signal or market data"));
        return orders;
    }
    
    // 2. Determine trade action based on signal
    TradeAction intended_action = determine_trade_action(signal);
    
    if (intended_action == TradeAction::HOLD) {
        orders.push_back(create_hold_order("Signal in neutral zone"));
        return orders;
    }
    
    // 3. Select optimal instrument
    std::string optimal_symbol = profit_optimizer_->select_optimal_instrument(signal, positions_);
    
    if (optimal_symbol == "HOLD") {
        orders.push_back(create_hold_order("No suitable instruments available"));
        return orders;
    }
    
    // 4. Check for and resolve conflicts
    if (config_.enable_auto_conflict_resolution) {
        auto conflict_analysis = conflict_resolver_->analyze_conflicts(optimal_symbol, positions_, bar.close);
        
        if (conflict_analysis.has_conflicts) {
            auto liquidation_orders = conflict_resolver_->resolve_conflicts_automatically(conflict_analysis);
            orders.insert(orders.end(), liquidation_orders.begin(), liquidation_orders.end());
            
            // Update cash balance from liquidations (simplified)
            for (const auto& liquidation_order : liquidation_orders) {
                cash_balance_ += liquidation_order.trade_value * (1.0 - config_.fee_rate);
                positions_.erase(liquidation_order.symbol);
            }
        }
    }
    
    // 5. Create main trade order
    TradeOrder main_order = create_main_order(intended_action, optimal_symbol, signal, bar);
    
    // 6. Apply risk management
    if (config_.enable_risk_management && intended_action == TradeAction::BUY) {
        main_order = apply_risk_management(main_order, signal, bar);
    }
    
    // 7. Validate cash constraints
    main_order = cash_protector_->adjust_order_for_cash_constraints(main_order, cash_balance_, config_.fee_rate);
    
    // 8. CRITICAL: Final validation for sell orders to prevent phantom orders
    if (main_order.action == TradeAction::SELL) {
        auto validation = position_validator_->validate_sell_order(main_order.symbol, 
                                                                 main_order.quantity, positions_);
        if (!validation.is_valid) {
            main_order = create_hold_order("PHANTOM ORDER PREVENTED: " + validation.error_message);
        } else {
            main_order.quantity = validation.validated_quantity;
            main_order.trade_value = main_order.quantity * main_order.price;
        }
    }
    
    orders.push_back(main_order);
    
    utils::log_info("ADAPTIVE TRADE EXECUTION COMPLETE: Generated " + 
                   std::to_string(orders.size()) + " orders");
    
    return orders;
}

bool AdaptivePortfolioManager::execute_order(const TradeOrder& order) {
    if (order.action == TradeAction::HOLD) {
        return true; // Nothing to execute
    }
    
    if (order.action == TradeAction::BUY) {
        return execute_buy_order(order);
    } else if (order.action == TradeAction::SELL) {
        return execute_sell_order(order);
    }
    
    return false;
}

double AdaptivePortfolioManager::get_total_portfolio_value() const {
    return calculate_total_portfolio_value();
}

bool AdaptivePortfolioManager::has_position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    return (it != positions_.end() && std::abs(it->second.quantity) > 1e-6);
}

Position AdaptivePortfolioManager::get_position(const std::string& symbol) const {
    auto it = positions_.find(symbol);
    return (it != positions_.end()) ? it->second : Position{};
}

// Private helper methods implementation continues...
bool AdaptivePortfolioManager::validate_inputs(const SignalOutput& signal, const Bar& bar) const {
    if (signal.confidence < 0.0 || signal.confidence > 1.0) {
        utils::log_error("Invalid signal confidence: " + std::to_string(signal.confidence));
        return false;
    }
    
    if (signal.probability < 0.0 || signal.probability > 1.0) {
        utils::log_error("Invalid signal probability: " + std::to_string(signal.probability));
        return false;
    }
    
    if (bar.close <= 0.0) {
        utils::log_error("Invalid bar close price: " + std::to_string(bar.close));
        return false;
    }
    
    return true;
}

TradeAction AdaptivePortfolioManager::determine_trade_action(const SignalOutput& signal) const {
    if (signal.probability > config_.buy_threshold) {
        return TradeAction::BUY;
    } else if (signal.probability < config_.sell_threshold) {
        return TradeAction::SELL;
    } else {
        return TradeAction::HOLD;
    }
}

TradeOrder AdaptivePortfolioManager::create_main_order(
    TradeAction action, const std::string& symbol, 
    const SignalOutput& signal, const Bar& bar) const {
    
    TradeOrder order;
    order.action = action;
    order.symbol = symbol;
    order.price = bar.close;
    order.confidence = signal.confidence;
    
    if (action == TradeAction::BUY) {
        // Calculate quantity based on available cash and risk management
        double available_cash = cash_balance_ - config_.minimum_cash_reserve;
        order.quantity = (available_cash * 0.95) / bar.close; // Use 95% of available cash
        order.trade_value = order.quantity * order.price;
        order.execution_reason = "Buy signal above threshold (Prob: " + 
                               std::to_string(signal.probability) + ")";
    } else if (action == TradeAction::SELL) {
        // For sell orders, quantity will be validated later
        auto pos_it = positions_.find(symbol);
        if (pos_it != positions_.end()) {
            order.quantity = pos_it->second.quantity;
            order.trade_value = order.quantity * order.price;
            order.execution_reason = "Sell signal below threshold (Prob: " + 
                                   std::to_string(signal.probability) + ")";
        }
    }
    
    return order;
}

TradeOrder AdaptivePortfolioManager::apply_risk_management(
    const TradeOrder& order, const SignalOutput& signal, const Bar& bar) const {
    
    auto risk_analysis = risk_manager_->calculate_optimal_position_size(
        order.symbol, signal, cash_balance_, bar.close, positions_);
    
    TradeOrder risk_adjusted_order = order;
    
    if (risk_analysis.max_safe_quantity < order.quantity) {
        risk_adjusted_order.quantity = risk_analysis.max_safe_quantity;
        risk_adjusted_order.trade_value = risk_adjusted_order.quantity * risk_adjusted_order.price;
        risk_adjusted_order.execution_reason = "Risk-adjusted: " + order.execution_reason + 
                                             " (Risk level: " + risk_analysis.risk_level + ")";
        
        utils::log_info("RISK MANAGEMENT: Reduced position size from " + 
                       std::to_string(order.quantity) + " to " + 
                       std::to_string(risk_adjusted_order.quantity) + " due to " + risk_analysis.risk_level + " risk");
    }
    
    return risk_adjusted_order;
}

TradeOrder AdaptivePortfolioManager::create_hold_order(const std::string& reason) const {
    TradeOrder hold_order;
    hold_order.action = TradeAction::HOLD;
    hold_order.execution_reason = reason;
    return hold_order;
}

bool AdaptivePortfolioManager::execute_buy_order(const TradeOrder& order) {
    double total_cost = order.trade_value + (order.trade_value * config_.fee_rate);
    
    if (total_cost > cash_balance_) {
        utils::log_error("Insufficient cash for buy order");
        return false;
    }
    
    // Update cash balance
    cash_balance_ -= total_cost;
    
    // Update or create position
    auto it = positions_.find(order.symbol);
    if (it != positions_.end()) {
        // Update existing position (average cost basis)
        Position& pos = it->second;
        double total_quantity = pos.quantity + order.quantity;
        double total_cost_basis = (pos.quantity * pos.avg_price) + order.trade_value;
        pos.avg_price = total_cost_basis / total_quantity;
        pos.quantity = total_quantity;
        pos.current_price = order.price;
    } else {
        // Create new position
        Position pos;
        pos.symbol = order.symbol;
        pos.quantity = order.quantity;
        pos.avg_price = order.price;
        pos.current_price = order.price;
        pos.unrealized_pnl = 0.0;
        pos.realized_pnl = 0.0;
        positions_[order.symbol] = pos;
    }
    
    utils::log_info("BUY ORDER EXECUTED: " + std::to_string(order.quantity) + 
                   " shares of " + order.symbol + " at $" + std::to_string(order.price));
    
    return true;
}

bool AdaptivePortfolioManager::execute_sell_order(const TradeOrder& order) {
    auto it = positions_.find(order.symbol);
    if (it == positions_.end()) {
        utils::log_error("PHANTOM ORDER PREVENTED: Cannot sell non-existent position: " + order.symbol);
        return false;
    }
    
    Position& pos = it->second;
    if (pos.quantity < order.quantity) {
        utils::log_error("PHANTOM ORDER PREVENTED: Insufficient quantity to sell");
        return false;
    }
    
    // Update cash balance (subtract fees)
    double proceeds = order.trade_value - (order.trade_value * config_.fee_rate);
    cash_balance_ += proceeds;
    
    // Update position
    pos.quantity -= order.quantity;
    pos.current_price = order.price;
    
    // Remove position if fully liquidated
    if (pos.quantity < 1e-6) {
        positions_.erase(it);
    }
    
    utils::log_info("SELL ORDER EXECUTED: " + std::to_string(order.quantity) + 
                   " shares of " + order.symbol + " at $" + std::to_string(order.price));
    
    return true;
}

double AdaptivePortfolioManager::calculate_total_portfolio_value() const {
    double total_value = cash_balance_;
    
    for (const auto& [symbol, position] : positions_) {
        total_value += position.quantity * position.current_price;
    }
    
    return total_value;
}

} // namespace sentio

```

## 📄 **FILE 32 of 39**: src/backend/adaptive_trading_mechanism.cpp

**File Information**:
- **Path**: `src/backend/adaptive_trading_mechanism.cpp`

- **Size**: 702 lines
- **Modified**: 2025-09-21 23:54:35

- **Type**: .cpp

```text
#include "backend/adaptive_trading_mechanism.h"
#include "common/utils.h"
#include <numeric>
#include <filesystem>

namespace sentio {

// ===================================================================
// MARKET REGIME DETECTOR IMPLEMENTATION
// ===================================================================

MarketState MarketRegimeDetector::analyze_market_state(const Bar& current_bar, 
                                                      const std::vector<Bar>& recent_history,
                                                      const SignalOutput& signal) {
    MarketState state;
    
    // Update price history
    price_history_.push_back(current_bar.close);
    if (price_history_.size() > LOOKBACK_PERIOD) {
        price_history_.erase(price_history_.begin());
    }
    
    // Update volume history
    volume_history_.push_back(current_bar.volume);
    if (volume_history_.size() > LOOKBACK_PERIOD) {
        volume_history_.erase(volume_history_.begin());
    }
    
    // Calculate market metrics
    state.current_price = current_bar.close;
    state.volatility = calculate_volatility();
    state.trend_strength = calculate_trend_strength();
    state.volume_ratio = calculate_volume_ratio();
    state.regime = classify_market_regime(state.volatility, state.trend_strength);
    
    // Signal statistics
    state.avg_signal_strength = std::abs(signal.probability - 0.5) * 2.0;
    
    utils::log_debug("Market Analysis: Price=" + std::to_string(state.current_price) + 
                    ", Vol=" + std::to_string(state.volatility) + 
                    ", Trend=" + std::to_string(state.trend_strength) + 
                    ", Regime=" + std::to_string(static_cast<int>(state.regime)));
    
    return state;
}

double MarketRegimeDetector::calculate_volatility() {
    if (price_history_.size() < 2) return 0.1; // Default volatility
    
    std::vector<double> returns;
    for (size_t i = 1; i < price_history_.size(); ++i) {
        double ret = std::log(price_history_[i] / price_history_[i-1]);
        returns.push_back(ret);
    }
    
    // Calculate standard deviation of returns
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double sq_sum = 0.0;
    for (double ret : returns) {
        sq_sum += (ret - mean) * (ret - mean);
    }
    
    return std::sqrt(sq_sum / returns.size()) * std::sqrt(252); // Annualized
}

double MarketRegimeDetector::calculate_trend_strength() {
    if (price_history_.size() < 10) return 0.0;
    
    // Linear regression slope over recent prices
    double n = static_cast<double>(price_history_.size());
    double sum_x = n * (n - 1) / 2;
    double sum_y = std::accumulate(price_history_.begin(), price_history_.end(), 0.0);
    double sum_xy = 0.0;
    double sum_x2 = n * (n - 1) * (2 * n - 1) / 6;
    
    for (size_t i = 0; i < price_history_.size(); ++i) {
        sum_xy += static_cast<double>(i) * price_history_[i];
    }
    
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    // Normalize slope to [-1, 1] range
    double price_range = *std::max_element(price_history_.begin(), price_history_.end()) -
                        *std::min_element(price_history_.begin(), price_history_.end());
    
    if (price_range > 0) {
        return std::clamp(slope / price_range * 100, -1.0, 1.0);
    }
    
    return 0.0;
}

double MarketRegimeDetector::calculate_volume_ratio() {
    if (volume_history_.empty()) return 1.0;
    
    double current_volume = volume_history_.back();
    double avg_volume = std::accumulate(volume_history_.begin(), volume_history_.end(), 0.0) / volume_history_.size();
    
    return (avg_volume > 0) ? current_volume / avg_volume : 1.0;
}

MarketRegime MarketRegimeDetector::classify_market_regime(double volatility, double trend_strength) {
    bool high_vol = volatility > 0.25; // 25% annualized volatility threshold
    
    if (trend_strength > 0.3) {
        return high_vol ? MarketRegime::BULL_HIGH_VOL : MarketRegime::BULL_LOW_VOL;
    } else if (trend_strength < -0.3) {
        return high_vol ? MarketRegime::BEAR_HIGH_VOL : MarketRegime::BEAR_LOW_VOL;
    } else {
        return high_vol ? MarketRegime::SIDEWAYS_HIGH_VOL : MarketRegime::SIDEWAYS_LOW_VOL;
    }
}

// ===================================================================
// PERFORMANCE EVALUATOR IMPLEMENTATION
// ===================================================================

void PerformanceEvaluator::add_trade_outcome(const TradeOutcome& outcome) {
    trade_history_.push_back(outcome);
    
    // Maintain rolling window
    if (trade_history_.size() > MAX_HISTORY) {
        trade_history_.erase(trade_history_.begin());
    }
    
    utils::log_debug("Trade outcome added: PnL=" + std::to_string(outcome.actual_pnl) + 
                    ", Profitable=" + (outcome.was_profitable ? "YES" : "NO"));
}

void PerformanceEvaluator::add_portfolio_value(double value) {
    portfolio_values_.push_back(value);
    
    if (portfolio_values_.size() > MAX_HISTORY) {
        portfolio_values_.erase(portfolio_values_.begin());
    }
}

PerformanceMetrics PerformanceEvaluator::calculate_performance_metrics() {
    PerformanceMetrics metrics;
    
    if (trade_history_.empty()) {
        return metrics;
    }
    
    // Get recent trades for analysis
    size_t start_idx = trade_history_.size() > PERFORMANCE_WINDOW ? 
                      trade_history_.size() - PERFORMANCE_WINDOW : 0;
    
    std::vector<TradeOutcome> recent_trades(
        trade_history_.begin() + start_idx, trade_history_.end());
    
    // Calculate basic metrics
    metrics.total_trades = static_cast<int>(recent_trades.size());
    metrics.winning_trades = 0;
    metrics.losing_trades = 0;
    metrics.gross_profit = 0.0;
    metrics.gross_loss = 0.0;
    
    for (const auto& trade : recent_trades) {
        if (trade.was_profitable) {
            metrics.winning_trades++;
            metrics.gross_profit += trade.actual_pnl;
        } else {
            metrics.losing_trades++;
            metrics.gross_loss += std::abs(trade.actual_pnl);
        }
        
        metrics.returns.push_back(trade.pnl_percentage);
    }
    
    // Calculate derived metrics
    metrics.win_rate = metrics.total_trades > 0 ? 
                      static_cast<double>(metrics.winning_trades) / metrics.total_trades : 0.0;
    
    metrics.profit_factor = metrics.gross_loss > 0 ? 
                           metrics.gross_profit / metrics.gross_loss : 1.0;
    
    metrics.sharpe_ratio = calculate_sharpe_ratio(metrics.returns);
    metrics.max_drawdown = calculate_max_drawdown();
    metrics.capital_efficiency = calculate_capital_efficiency();
    
    return metrics;
}

double PerformanceEvaluator::calculate_reward_signal(const PerformanceMetrics& metrics) {
    // Multi-objective reward function
    double profit_component = metrics.gross_profit - metrics.gross_loss;
    double risk_component = metrics.sharpe_ratio * 0.5;
    double drawdown_penalty = metrics.max_drawdown * -2.0;
    double overtrading_penalty = std::max(0.0, metrics.trade_frequency - 10.0) * -0.1;
    
    double total_reward = profit_component + risk_component + drawdown_penalty + overtrading_penalty;
    
    utils::log_debug("Reward calculation: Profit=" + std::to_string(profit_component) + 
                    ", Risk=" + std::to_string(risk_component) + 
                    ", Drawdown=" + std::to_string(drawdown_penalty) + 
                    ", Total=" + std::to_string(total_reward));
    
    return total_reward;
}

double PerformanceEvaluator::calculate_sharpe_ratio(const std::vector<double>& returns) {
    if (returns.size() < 2) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= returns.size();
    
    double std_dev = std::sqrt(variance);
    return std_dev > 0 ? mean_return / std_dev : 0.0;
}

double PerformanceEvaluator::calculate_max_drawdown() {
    if (portfolio_values_.size() < 2) return 0.0;
    
    double peak = portfolio_values_[0];
    double max_dd = 0.0;
    
    for (double value : portfolio_values_) {
        if (value > peak) {
            peak = value;
        }
        
        double drawdown = (peak - value) / peak;
        max_dd = std::max(max_dd, drawdown);
    }
    
    return max_dd;
}

double PerformanceEvaluator::calculate_capital_efficiency() {
    if (portfolio_values_.size() < 2) return 0.0;
    
    double initial_value = portfolio_values_.front();
    double final_value = portfolio_values_.back();
    
    return initial_value > 0 ? (final_value - initial_value) / initial_value : 0.0;
}

// ===================================================================
// Q-LEARNING THRESHOLD OPTIMIZER IMPLEMENTATION
// ===================================================================

QLearningThresholdOptimizer::QLearningThresholdOptimizer() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    utils::log_info("Q-Learning Threshold Optimizer initialized with learning_rate=" + 
                   std::to_string(learning_rate_) + ", exploration_rate=" + std::to_string(exploration_rate_));
}

ThresholdAction QLearningThresholdOptimizer::select_action(const MarketState& state, 
                                                          const ThresholdPair& current_thresholds,
                                                          const PerformanceMetrics& performance) {
    int state_hash = discretize_state(state, current_thresholds, performance);
    
    // Epsilon-greedy action selection
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    if (dis(rng_) < exploration_rate_) {
        // Explore: random action
        std::uniform_int_distribution<int> action_dis(0, static_cast<int>(ThresholdAction::COUNT) - 1);
        ThresholdAction action = static_cast<ThresholdAction>(action_dis(rng_));
        utils::log_debug("Q-Learning: EXPLORE action=" + std::to_string(static_cast<int>(action)));
        return action;
    } else {
        // Exploit: best known action
        ThresholdAction action = get_best_action(state_hash);
        utils::log_debug("Q-Learning: EXPLOIT action=" + std::to_string(static_cast<int>(action)));
        return action;
    }
}

void QLearningThresholdOptimizer::update_q_value(const MarketState& prev_state,
                                                 const ThresholdPair& prev_thresholds,
                                                 const PerformanceMetrics& prev_performance,
                                                 ThresholdAction action,
                                                 double reward,
                                                 const MarketState& new_state,
                                                 const ThresholdPair& new_thresholds,
                                                 const PerformanceMetrics& new_performance) {
    
    int prev_state_hash = discretize_state(prev_state, prev_thresholds, prev_performance);
    int new_state_hash = discretize_state(new_state, new_thresholds, new_performance);
    
    StateActionPair sa_pair{prev_state_hash, static_cast<int>(action)};
    
    // Get current Q-value
    double current_q = get_q_value(sa_pair);
    
    // Get maximum Q-value for next state
    double max_next_q = get_max_q_value(new_state_hash);
    
    // Q-learning update
    double target = reward + discount_factor_ * max_next_q;
    double new_q = current_q + learning_rate_ * (target - current_q);
    
    q_table_[sa_pair] = new_q;
    state_visit_count_[prev_state_hash]++;
    
    // Decay exploration rate
    exploration_rate_ = std::max(min_exploration_, exploration_rate_ * exploration_decay_);
    
    utils::log_debug("Q-Learning update: State=" + std::to_string(prev_state_hash) + 
                    ", Action=" + std::to_string(static_cast<int>(action)) + 
                    ", Reward=" + std::to_string(reward) + 
                    ", Q_old=" + std::to_string(current_q) + 
                    ", Q_new=" + std::to_string(new_q));
}

ThresholdPair QLearningThresholdOptimizer::apply_action(const ThresholdPair& current_thresholds, ThresholdAction action) {
    ThresholdPair new_thresholds = current_thresholds;
    
    switch (action) {
        case ThresholdAction::INCREASE_BUY_SMALL:
            new_thresholds.buy_threshold += 0.01;
            break;
        case ThresholdAction::INCREASE_BUY_MEDIUM:
            new_thresholds.buy_threshold += 0.03;
            break;
        case ThresholdAction::DECREASE_BUY_SMALL:
            new_thresholds.buy_threshold -= 0.01;
            break;
        case ThresholdAction::DECREASE_BUY_MEDIUM:
            new_thresholds.buy_threshold -= 0.03;
            break;
        case ThresholdAction::INCREASE_SELL_SMALL:
            new_thresholds.sell_threshold += 0.01;
            break;
        case ThresholdAction::INCREASE_SELL_MEDIUM:
            new_thresholds.sell_threshold += 0.03;
            break;
        case ThresholdAction::DECREASE_SELL_SMALL:
            new_thresholds.sell_threshold -= 0.01;
            break;
        case ThresholdAction::DECREASE_SELL_MEDIUM:
            new_thresholds.sell_threshold -= 0.03;
            break;
        case ThresholdAction::MAINTAIN_THRESHOLDS:
        default:
            // No change
            break;
    }
    
    // Ensure thresholds remain valid
    new_thresholds.buy_threshold = std::clamp(new_thresholds.buy_threshold, 0.51, 0.90);
    new_thresholds.sell_threshold = std::clamp(new_thresholds.sell_threshold, 0.10, 0.49);
    
    // Ensure minimum gap
    if (new_thresholds.buy_threshold - new_thresholds.sell_threshold < 0.05) {
        new_thresholds.buy_threshold = new_thresholds.sell_threshold + 0.05;
        new_thresholds.buy_threshold = std::min(new_thresholds.buy_threshold, 0.90);
    }
    
    return new_thresholds;
}

double QLearningThresholdOptimizer::get_learning_progress() const {
    return 1.0 - exploration_rate_;
}

int QLearningThresholdOptimizer::discretize_state(const MarketState& state, 
                                                 const ThresholdPair& thresholds,
                                                 const PerformanceMetrics& performance) {
    // Create a hash of the discretized state
    int buy_bin = static_cast<int>((thresholds.buy_threshold - 0.5) / 0.4 * THRESHOLD_BINS);
    int sell_bin = static_cast<int>((thresholds.sell_threshold - 0.1) / 0.4 * THRESHOLD_BINS);
    int vol_bin = static_cast<int>(std::min(state.volatility / 0.5, 1.0) * 5);
    int trend_bin = static_cast<int>((state.trend_strength + 1.0) / 2.0 * 5);
    int perf_bin = static_cast<int>(std::clamp(performance.win_rate, 0.0, 1.0) * PERFORMANCE_BINS);
    
    // Combine bins into a single hash
    return buy_bin * 10000 + sell_bin * 1000 + vol_bin * 100 + trend_bin * 10 + perf_bin;
}

double QLearningThresholdOptimizer::get_q_value(const StateActionPair& sa_pair) {
    auto it = q_table_.find(sa_pair);
    return (it != q_table_.end()) ? it->second : 0.0; // Optimistic initialization
}

double QLearningThresholdOptimizer::get_max_q_value(int state_hash) {
    double max_q = 0.0;
    
    for (int action = 0; action < static_cast<int>(ThresholdAction::COUNT); ++action) {
        StateActionPair sa_pair{state_hash, action};
        max_q = std::max(max_q, get_q_value(sa_pair));
    }
    
    return max_q;
}

ThresholdAction QLearningThresholdOptimizer::get_best_action(int state_hash) {
    ThresholdAction best_action = ThresholdAction::MAINTAIN_THRESHOLDS;
    double best_q = get_q_value({state_hash, static_cast<int>(best_action)});
    
    for (int action = 0; action < static_cast<int>(ThresholdAction::COUNT); ++action) {
        StateActionPair sa_pair{state_hash, action};
        double q_val = get_q_value(sa_pair);
        
        if (q_val > best_q) {
            best_q = q_val;
            best_action = static_cast<ThresholdAction>(action);
        }
    }
    
    return best_action;
}

// ===================================================================
// MULTI-ARMED BANDIT OPTIMIZER IMPLEMENTATION
// ===================================================================

MultiArmedBanditOptimizer::MultiArmedBanditOptimizer() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    initialize_arms();
    utils::log_info("Multi-Armed Bandit Optimizer initialized with " + std::to_string(arms_.size()) + " arms");
}

ThresholdPair MultiArmedBanditOptimizer::select_thresholds() {
    if (arms_.empty()) {
        return ThresholdPair(); // Default thresholds
    }
    
    // UCB1 algorithm
    update_confidence_bounds();
    
    auto best_arm = std::max_element(arms_.begin(), arms_.end(),
        [](const BanditArm& a, const BanditArm& b) {
            return (a.estimated_reward + a.confidence_bound) < 
                   (b.estimated_reward + b.confidence_bound);
        });
    
    utils::log_debug("Bandit selected: Buy=" + std::to_string(best_arm->thresholds.buy_threshold) + 
                    ", Sell=" + std::to_string(best_arm->thresholds.sell_threshold) + 
                    ", UCB=" + std::to_string(best_arm->estimated_reward + best_arm->confidence_bound));
    
    return best_arm->thresholds;
}

void MultiArmedBanditOptimizer::update_reward(const ThresholdPair& thresholds, double reward) {
    // Find the arm that was pulled
    auto arm_it = std::find_if(arms_.begin(), arms_.end(),
        [&thresholds](const BanditArm& arm) {
            return std::abs(arm.thresholds.buy_threshold - thresholds.buy_threshold) < 0.005 &&
                   std::abs(arm.thresholds.sell_threshold - thresholds.sell_threshold) < 0.005;
        });
    
    if (arm_it != arms_.end()) {
        // Update arm's estimated reward using incremental mean
        arm_it->pull_count++;
        total_pulls_++;
        
        double old_estimate = arm_it->estimated_reward;
        arm_it->estimated_reward = old_estimate + (reward - old_estimate) / arm_it->pull_count;
        
        utils::log_debug("Bandit reward update: Buy=" + std::to_string(thresholds.buy_threshold) + 
                        ", Sell=" + std::to_string(thresholds.sell_threshold) + 
                        ", Reward=" + std::to_string(reward) + 
                        ", New_Est=" + std::to_string(arm_it->estimated_reward));
    }
}

void MultiArmedBanditOptimizer::initialize_arms() {
    // Create a grid of threshold combinations
    for (double buy = 0.55; buy <= 0.85; buy += 0.05) {
        for (double sell = 0.15; sell <= 0.45; sell += 0.05) {
            if (buy > sell + 0.05) { // Ensure minimum gap
                arms_.emplace_back(ThresholdPair(buy, sell));
            }
        }
    }
}

void MultiArmedBanditOptimizer::update_confidence_bounds() {
    for (auto& arm : arms_) {
        if (arm.pull_count == 0) {
            arm.confidence_bound = std::numeric_limits<double>::max();
        } else {
            arm.confidence_bound = std::sqrt(2.0 * std::log(total_pulls_) / arm.pull_count);
        }
    }
}

// ===================================================================
// ADAPTIVE THRESHOLD MANAGER IMPLEMENTATION
// ===================================================================

AdaptiveThresholdManager::AdaptiveThresholdManager(const AdaptiveConfig& config) 
    : config_(config), current_thresholds_(0.55, 0.45) {
    
    // Initialize components
    q_learner_ = std::make_unique<QLearningThresholdOptimizer>();
    bandit_optimizer_ = std::make_unique<MultiArmedBanditOptimizer>();
    regime_detector_ = std::make_unique<MarketRegimeDetector>();
    performance_evaluator_ = std::make_unique<PerformanceEvaluator>();
    
    // Initialize regime-specific thresholds
    initialize_regime_thresholds();
    
    utils::log_info("AdaptiveThresholdManager initialized: Algorithm=" + 
                   std::to_string(static_cast<int>(config_.algorithm)) + 
                   ", LearningRate=" + std::to_string(config_.learning_rate) + 
                   ", ConservativeMode=" + (config_.conservative_mode ? "YES" : "NO"));
}

ThresholdPair AdaptiveThresholdManager::get_current_thresholds(const SignalOutput& signal, const Bar& bar) {
    // Update market state
    current_market_state_ = regime_detector_->analyze_market_state(bar, recent_bars_, signal);
    recent_bars_.push_back(bar);
    if (recent_bars_.size() > 100) {
        recent_bars_.erase(recent_bars_.begin());
    }
    
    // Check circuit breaker
    if (circuit_breaker_active_) {
        utils::log_warning("Circuit breaker active - using conservative thresholds");
        return get_conservative_thresholds();
    }
    
    // Update performance and potentially adjust thresholds
    update_performance_and_learn();
    
    // Get regime-adapted thresholds if enabled
    if (config_.enable_regime_adaptation) {
        return get_regime_adapted_thresholds();
    }
    
    return current_thresholds_;
}

void AdaptiveThresholdManager::process_trade_outcome(const std::string& symbol, TradeAction action, 
                                                    double quantity, double price, double trade_value, double fees,
                                                    double actual_pnl, double pnl_percentage, bool was_profitable) {
    TradeOutcome outcome;
    outcome.symbol = symbol;
    outcome.action = action;
    outcome.quantity = quantity;
    outcome.price = price;
    outcome.trade_value = trade_value;
    outcome.fees = fees;
    outcome.actual_pnl = actual_pnl;
    outcome.pnl_percentage = pnl_percentage;
    outcome.was_profitable = was_profitable;
    outcome.outcome_timestamp = std::chrono::system_clock::now();
    
    performance_evaluator_->add_trade_outcome(outcome);
    
    // Update learning algorithms with reward feedback
    if (learning_enabled_) {
        current_performance_ = performance_evaluator_->calculate_performance_metrics();
        double reward = performance_evaluator_->calculate_reward_signal(current_performance_);
        
        // Update based on algorithm type
        switch (config_.algorithm) {
            case LearningAlgorithm::Q_LEARNING:
                // Q-learning update will happen in next call to update_performance_and_learn()
                break;
                
            case LearningAlgorithm::MULTI_ARMED_BANDIT:
                bandit_optimizer_->update_reward(current_thresholds_, reward);
                break;
                
            case LearningAlgorithm::ENSEMBLE:
                // Update both algorithms
                bandit_optimizer_->update_reward(current_thresholds_, reward);
                break;
        }
    }
    
    // Check for circuit breaker conditions
    check_circuit_breaker();
}

void AdaptiveThresholdManager::update_portfolio_value(double value) {
    performance_evaluator_->add_portfolio_value(value);
}

double AdaptiveThresholdManager::get_learning_progress() const {
    return q_learner_->get_learning_progress();
}

std::string AdaptiveThresholdManager::generate_performance_report() const {
    std::ostringstream report;
    
    report << "=== ADAPTIVE TRADING PERFORMANCE REPORT ===\n";
    report << "Current Thresholds: Buy=" << std::fixed << std::setprecision(3) << current_thresholds_.buy_threshold 
           << ", Sell=" << current_thresholds_.sell_threshold << "\n";
    report << "Market Regime: " << static_cast<int>(current_market_state_.regime) << "\n";
    report << "Total Trades: " << current_performance_.total_trades << "\n";
    report << "Win Rate: " << std::fixed << std::setprecision(1) << (current_performance_.win_rate * 100) << "%\n";
    report << "Profit Factor: " << std::fixed << std::setprecision(2) << current_performance_.profit_factor << "\n";
    report << "Sharpe Ratio: " << std::fixed << std::setprecision(2) << current_performance_.sharpe_ratio << "\n";
    report << "Max Drawdown: " << std::fixed << std::setprecision(1) << (current_performance_.max_drawdown * 100) << "%\n";
    report << "Learning Progress: " << std::fixed << std::setprecision(1) << (get_learning_progress() * 100) << "%\n";
    report << "Circuit Breaker: " << (circuit_breaker_active_ ? "ACTIVE" : "INACTIVE") << "\n";
    
    return report.str();
}

void AdaptiveThresholdManager::initialize_regime_thresholds() {
    // Conservative thresholds for volatile markets
    regime_thresholds_[MarketRegime::BULL_HIGH_VOL] = ThresholdPair(0.65, 0.35);
    regime_thresholds_[MarketRegime::BEAR_HIGH_VOL] = ThresholdPair(0.70, 0.30);
    regime_thresholds_[MarketRegime::SIDEWAYS_HIGH_VOL] = ThresholdPair(0.68, 0.32);
    
    // More aggressive thresholds for stable markets
    regime_thresholds_[MarketRegime::BULL_LOW_VOL] = ThresholdPair(0.58, 0.42);
    regime_thresholds_[MarketRegime::BEAR_LOW_VOL] = ThresholdPair(0.62, 0.38);
    regime_thresholds_[MarketRegime::SIDEWAYS_LOW_VOL] = ThresholdPair(0.60, 0.40);
}

void AdaptiveThresholdManager::update_performance_and_learn() {
    if (!learning_enabled_ || circuit_breaker_active_) {
        return;
    }
    
    // Update performance metrics
    PerformanceMetrics new_performance = performance_evaluator_->calculate_performance_metrics();
    
    // Only learn if we have enough data
    if (new_performance.total_trades < config_.performance_window / 2) {
        return;
    }
    
    // Q-Learning update
    if (config_.algorithm == LearningAlgorithm::Q_LEARNING || 
        config_.algorithm == LearningAlgorithm::ENSEMBLE) {
        
        double reward = performance_evaluator_->calculate_reward_signal(new_performance);
        
        // Select and apply action
        ThresholdAction action = q_learner_->select_action(
            current_market_state_, current_thresholds_, current_performance_);
        
        ThresholdPair new_thresholds = q_learner_->apply_action(current_thresholds_, action);
        
        // Update Q-values if we have previous state
        if (current_performance_.total_trades > 0) {
            q_learner_->update_q_value(
                current_market_state_, current_thresholds_, current_performance_,
                action, reward,
                current_market_state_, new_thresholds, new_performance);
        }
        
        current_thresholds_ = new_thresholds;
    }
    
    // Multi-Armed Bandit update
    if (config_.algorithm == LearningAlgorithm::MULTI_ARMED_BANDIT || 
        config_.algorithm == LearningAlgorithm::ENSEMBLE) {
        
        current_thresholds_ = bandit_optimizer_->select_thresholds();
    }
    
    current_performance_ = new_performance;
}

ThresholdPair AdaptiveThresholdManager::get_regime_adapted_thresholds() {
    auto regime_it = regime_thresholds_.find(current_market_state_.regime);
    if (regime_it != regime_thresholds_.end()) {
        // Blend learned thresholds with regime-specific ones
        ThresholdPair regime_thresholds = regime_it->second;
        double blend_factor = config_.conservative_mode ? 0.7 : 0.3;
        
        return ThresholdPair(
            current_thresholds_.buy_threshold * (1.0 - blend_factor) + 
            regime_thresholds.buy_threshold * blend_factor,
            current_thresholds_.sell_threshold * (1.0 - blend_factor) + 
            regime_thresholds.sell_threshold * blend_factor
        );
    }
    
    return current_thresholds_;
}

ThresholdPair AdaptiveThresholdManager::get_conservative_thresholds() {
    // Return very conservative thresholds during circuit breaker
    return ThresholdPair(0.75, 0.25);
}

void AdaptiveThresholdManager::check_circuit_breaker() {
    // Only activate circuit breaker if we have sufficient trading history
    if (current_performance_.total_trades < 10) {
        return; // Not enough data to make circuit breaker decisions
    }
    
    if (current_performance_.max_drawdown > config_.max_drawdown_limit ||
        current_performance_.win_rate < 0.3 ||
        (current_performance_.total_trades > 20 && current_performance_.profit_factor < 0.8)) {
        
        circuit_breaker_active_ = true;
        learning_enabled_ = false;
        
        utils::log_error("CIRCUIT BREAKER ACTIVATED: Drawdown=" + std::to_string(current_performance_.max_drawdown) + 
                        ", WinRate=" + std::to_string(current_performance_.win_rate) + 
                        ", ProfitFactor=" + std::to_string(current_performance_.profit_factor));
    }
}

} // namespace sentio

```

## 📄 **FILE 33 of 39**: src/backend/audit_component.cpp

**File Information**:
- **Path**: `src/backend/audit_component.cpp`

- **Size**: 28 lines
- **Modified**: 2025-09-21 00:30:30

- **Type**: .cpp

```text
#include "backend/audit_component.h"
#include "common/utils.h"

namespace sentio {

TradeSummary AuditComponent::analyze_equity_curve(const std::vector<double>& equity_curve) const {
    TradeSummary s;
    if (equity_curve.size() < 2) return s;

    // Approximate returns from equity curve deltas
    std::vector<double> returns;
    returns.reserve(equity_curve.size() - 1);
    for (size_t i = 1; i < equity_curve.size(); ++i) {
        double prev = equity_curve[i - 1];
        double curr = equity_curve[i];
        if (prev != 0.0) {
            returns.push_back((curr - prev) / prev);
        }
    }

    s.sharpe = utils::calculate_sharpe_ratio(returns);
    s.max_drawdown = utils::calculate_max_drawdown(equity_curve);
    return s;
}

} // namespace sentio



```

## 📄 **FILE 34 of 39**: src/backend/backend_component.cpp

**File Information**:
- **Path**: `src/backend/backend_component.cpp`

- **Size**: 725 lines
- **Modified**: 2025-09-22 01:37:54

- **Type**: .cpp

```text
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
    
    utils::log_debug("BACKEND COMPONENT INITIALIZED with Adaptive Portfolio Manager - Phantom orders ELIMINATED");
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

    // --- SIMPLIFIED LEVERAGE LOGIC ---
    std::string trade_symbol = signal.symbol;
    if (config_.leverage_enabled) {
        // Leverage enabled: Use automatic instrument selection
        trade_symbol = select_optimal_instrument(signal.probability, signal.confidence);
        utils::log_debug("Leverage trading: Auto-selected symbol=" + trade_symbol + 
                       " for probability=" + std::to_string(signal.probability) + 
                       ", confidence=" + std::to_string(signal.confidence));
    } else {
        // No leverage: Only trade QQQ
        trade_symbol = "QQQ";
        utils::log_debug("Standard trading: Using QQQ only");
    }
    
    // If HOLD is selected, skip the trade
    if (trade_symbol == "HOLD") {
        order.action = TradeAction::HOLD;
        order.execution_reason = "Signal in neutral zone or selector chose HOLD";
        return order;
    }
    
    order.symbol = trade_symbol; // Update order to use the selected symbol
    
    utils::log_debug("Evaluating signal: symbol=" + signal.symbol + 
                    ", probability=" + std::to_string(signal.probability) + 
                    ", confidence=" + std::to_string(signal.confidence) + 
                    ", price=" + std::to_string(bar.close));

    // Decision logic based on probability thresholds with validation
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
            utils::log_debug("SELL SIGNAL FIX: Overriding trade symbol from " + trade_symbol + 
                           " to " + position_to_sell + " (actual position held)");
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



```

## 📄 **FILE 35 of 39**: src/backend/backend_component_fixed.cpp

**File Information**:
- **Path**: `src/backend/backend_component_fixed.cpp`

- **Size**: 74 lines
- **Modified**: 2025-09-21 22:02:21

- **Type**: .cpp

```text
// FIXED evaluate_signal method - replaces the buggy version
// This method eliminates phantom sell orders by using the Adaptive Portfolio Manager

BackendComponent::TradeOrder BackendComponent::evaluate_signal(
    const SignalOutput& signal, const Bar& bar) {

    // CRITICAL BUG FIX: Use Adaptive Portfolio Manager to eliminate phantom sell orders
    utils::log_debug("EVALUATING SIGNAL with Adaptive Portfolio Manager: Symbol=" + signal.symbol + 
                    ", Probability=" + std::to_string(signal.probability) + 
                    ", Confidence=" + std::to_string(signal.confidence));
    
    // Use the Adaptive Portfolio Manager for intelligent trade decisions
    auto orders = adaptive_portfolio_manager_->execute_adaptive_trade(signal, bar);
    
    if (orders.empty()) {
        utils::log_warning("No orders generated by Adaptive Portfolio Manager");
        TradeOrder hold_order;
        hold_order.action = TradeAction::HOLD;
        hold_order.symbol = signal.symbol;
        hold_order.timestamp_ms = signal.timestamp_ms;
        hold_order.bar_index = signal.bar_index;
        hold_order.execution_reason = "No valid trade opportunities";
        hold_order.before_state = portfolio_manager_->get_state();
        hold_order.after_state = portfolio_manager_->get_state();
        return hold_order;
    }
    
    // For backward compatibility, return the first (main) order
    // The adaptive manager handles conflict resolution internally
    TradeOrder order = orders[0];
    
    // Set backward compatibility fields
    order.timestamp_ms = signal.timestamp_ms;
    order.bar_index = signal.bar_index;
    order.signal_probability = signal.probability;
    order.signal_confidence = signal.confidence;
    order.before_state = portfolio_manager_->get_state();
    
    // Execute any additional orders (e.g., conflict resolution orders)
    for (size_t i = 1; i < orders.size(); ++i) {
        bool executed = adaptive_portfolio_manager_->execute_order(orders[i]);
        if (executed) {
            utils::log_info("EXECUTED AUXILIARY ORDER: " + orders[i].execution_reason);
        } else {
            utils::log_error("FAILED TO EXECUTE AUXILIARY ORDER: " + orders[i].execution_reason);
        }
    }
    
    // Add fees to the main order
    if (order.action != TradeAction::HOLD) {
        order.fees = calculate_fees(order.trade_value);
    }
    
    // Execute the main order through the adaptive manager
    if (order.action != TradeAction::HOLD) {
        bool executed = adaptive_portfolio_manager_->execute_order(order);
        if (!executed) {
            utils::log_error("FAILED TO EXECUTE MAIN ORDER - Converting to HOLD");
            order.action = TradeAction::HOLD;
            order.execution_reason = "Order execution failed - PHANTOM ORDER PREVENTED";
        }
    }
    
    order.after_state = portfolio_manager_->get_state();
    
    utils::log_info("SIGNAL EVALUATION COMPLETE: Action=" + 
                   (order.action == TradeAction::BUY ? "BUY" : 
                    order.action == TradeAction::SELL ? "SELL" : "HOLD") + 
                   ", Symbol=" + order.symbol + 
                   ", Quantity=" + std::to_string(order.quantity) + 
                   ", Reason=" + order.execution_reason);
    
    return order;
}

```

