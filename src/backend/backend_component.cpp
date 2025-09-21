#include "backend/backend_component.h"
#include "backend/portfolio_manager.h"
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
            if (order.action == TradeAction::BUY) {
                portfolio_manager_->execute_buy(order.symbol, order.quantity, order.price, order.fees);
                position_manager_->add_position(order.symbol);
                position_manager_->update_direction(StaticPositionManager::Direction::LONG);
            } else if (order.action == TradeAction::SELL) {
                portfolio_manager_->execute_sell(order.symbol, order.quantity, order.price, order.fees);
                position_manager_->remove_position(order.symbol);
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
    
    utils::log_debug("Evaluating signal: symbol=" + signal.symbol + 
                    ", probability=" + std::to_string(signal.probability) + 
                    ", confidence=" + std::to_string(signal.confidence) + 
                    ", price=" + std::to_string(bar.close));

    // Decision logic based on probability thresholds with validation
    double buy_threshold = 0.6;
    double sell_threshold = 0.4;
    
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

    if (signal.probability > buy_threshold && signal.confidence > 0.5) {
        // Buy signal - aggressive capital allocation for maximum profit
        if (!portfolio_manager_->has_position(signal.symbol)) {
            double available_capital = portfolio_manager_->get_cash_balance();
            
            // Validate available capital
            if (available_capital < 0.0) {
                utils::log_error("CRITICAL: Negative available_capital=" + std::to_string(available_capital) + 
                                " for symbol=" + signal.symbol + ", cannot execute buy");
                std::abort();
            }
            
            double position_size = calculate_position_size(signal.probability, available_capital);
            
            // Validate position size calculation
            if (position_size < 0.0 || std::isnan(position_size) || std::isinf(position_size)) {
                utils::log_error("CRITICAL: Invalid position_size=" + std::to_string(position_size) + 
                                " for symbol=" + signal.symbol + ", available_capital=" + std::to_string(available_capital));
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
            order.action = TradeAction::HOLD;
            order.execution_reason = "Already has position";
            utils::log_debug("HOLD: Already have position in " + signal.symbol);
        }
    } else if (signal.probability < sell_threshold) {
        // Sell signal - liquidate position for profit protection
        if (portfolio_manager_->has_position(signal.symbol)) {
            auto position = portfolio_manager_->get_position(signal.symbol);
            
            // Validate position data
            if (position.quantity <= 0.0 || std::isnan(position.quantity) || std::isinf(position.quantity)) {
                utils::log_error("CRITICAL: Invalid position.quantity=" + std::to_string(position.quantity) + 
                                " for symbol=" + signal.symbol + " during sell signal");
                std::abort();
            }
            
            order.quantity = position.quantity;
            order.trade_value = order.quantity * bar.close;
            
            // Validate trade value calculation
            if (order.trade_value <= 0.0 || std::isnan(order.trade_value) || std::isinf(order.trade_value)) {
                utils::log_error("CRITICAL: Invalid sell trade_value=" + std::to_string(order.trade_value) + 
                                " for symbol=" + signal.symbol + ", quantity=" + std::to_string(order.quantity) + 
                                ", price=" + std::to_string(bar.close));
                std::abort();
            }
            
            order.fees = calculate_fees(order.trade_value);
            order.action = TradeAction::SELL;
            order.execution_reason = "Sell signal below threshold - full liquidation";
            
            utils::log_debug("SELL ORDER: symbol=" + signal.symbol + 
                            ", quantity=" + std::to_string(order.quantity) + 
                            ", value=" + std::to_string(order.trade_value) + 
                            ", fees=" + std::to_string(order.fees));
        } else {
            order.action = TradeAction::HOLD;
            order.execution_reason = "No position to sell";
            utils::log_debug("HOLD: No position to sell in " + signal.symbol);
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
            if (order.action == TradeAction::BUY) {
                portfolio_manager_->execute_buy(order.symbol, order.quantity, order.price, order.fees);
                position_manager_->add_position(order.symbol);
                position_manager_->update_direction(StaticPositionManager::Direction::LONG);
            } else if (order.action == TradeAction::SELL) {
                portfolio_manager_->execute_sell(order.symbol, order.quantity, order.price, order.fees);
                position_manager_->remove_position(order.symbol);
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
// DB helpers removed

} // namespace sentio


