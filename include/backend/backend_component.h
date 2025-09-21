#pragma once

// =============================================================================
// Module: backend/backend_component.h
// Purpose: Aggressive Portfolio Management & Trade Execution Engine
//
// Core Architecture & Recent Enhancements:
// The Backend serves as the critical execution layer that transforms AI-generated
// signals into actual trades using an aggressive capital allocation strategy
// designed for maximum profit potential while maintaining strict risk controls.
//
// Key Design Philosophy:
// - Aggressive capital allocation: Up to 100% capital usage based on signal strength
// - Fail-fast error handling: Immediate program termination on any financial anomalies
// - Zero-commission Alpaca fee model for cost-effective execution
// - Comprehensive audit trails with detailed trade logging
// - Robust portfolio state management with negative cash prevention
//
// Recent Major Improvements:
// - Removed artificial capital allocation limits for maximum profit potential
// - Enhanced fee calculation with detailed error handling and validation
// - Implemented aggressive position sizing based on signal confidence
// - Added comprehensive input validation with crash-on-error philosophy
// - Integrated detailed debug logging for all financial operations
// - Enhanced conflict detection and prevention mechanisms
//
// Financial Safeguards:
// - Hard constraint preventing negative cash balances (immediate abort)
// - Extensive validation of all trade parameters before execution
// - Detailed logging of all financial calculations and state changes
// - Position conflict detection to prevent simultaneous long/short positions
// =============================================================================

#include <vector>
#include <memory>
#include <string>
#include <map>
#include "common/types.h"
#include "strategy/signal_output.h"
#include "backend/leverage_manager.h"
#include "backend/adaptive_trading_mechanism.h"
#include "backend/position_state_machine.h"
#include "strategy/momentum_scalper.h"

namespace sentio {

class BackendComponent {
public:
    struct TradeOrder {
        int64_t timestamp_ms = 0;
        int bar_index = 0;
        std::string symbol;
        TradeAction action = TradeAction::HOLD;
        double quantity = 0.0;
        double price = 0.0;
        double trade_value = 0.0;
        double fees = 0.0;

        PortfolioState before_state;  // snapshot prior to execution
        PortfolioState after_state;   // snapshot after execution

        double signal_probability = 0.0;
        double signal_confidence = 0.0;

        std::string execution_reason;
        std::string rejection_reason;
        bool conflict_check_passed = true;

        // JSONL serialization for file-based trade books
        std::string to_json_line(const std::string& run_id) const;
    };

    /// Configuration for aggressive portfolio management and trade execution
    /// 
    /// This structure defines the core parameters for the trading engine,
    /// emphasizing aggressive capital allocation for maximum profit potential
    /// while maintaining essential risk controls.
    struct BackendConfig {
        /// Starting capital for trading operations (default: $100,000)
        double starting_capital = 100000.0;
        
        /// REMOVED: max_position_size - No artificial limits on capital usage
        /// The system now allows up to 100% capital allocation based on signal
        /// strength to maximize profit potential. Position sizing is determined
        /// dynamically by signal confidence and available capital.
        
        /// Enable position conflict detection to prevent simultaneous long/short positions
        /// This prevents conflicting trades that could create unintended hedged positions
        bool enable_conflict_prevention = true;
        
        /// Fee model for trade execution (default: Alpaca zero-commission model)
        /// ALPACA: Zero commission for cost-effective execution
        /// Other models available for testing: ZERO, FIXED, PERCENTAGE
        CostModel cost_model = CostModel::ALPACA;
        
        /// Strategy-specific thresholds for buy/sell decisions
        /// Keys: "buy_threshold", "sell_threshold" 
        /// Values: Probability thresholds (0.0 to 1.0) for trade execution
        std::map<std::string, double> strategy_thresholds;
        
        // --- NEW LEVERAGE PARAMETERS (Requirement 5.2) ---
        /// Enable leverage trading with intelligent instrument selection
        /// When enabled, the system will automatically select optimal leverage
        /// instruments (QQQ, TQQQ, SQQQ, PSQ) based on signal strength
        bool leverage_enabled = false;
        
        /// Force a specific symbol for trading (overrides automatic selection)
        /// If empty, the system uses automatic instrument selection
        /// Valid values: "QQQ", "TQQQ", "SQQQ", "PSQ", or empty for auto-selection
        std::string target_symbol;
        
        // --- ADAPTIVE THRESHOLD PARAMETERS ---
        /// Enable adaptive threshold optimization for improved trading performance
        /// When enabled, the system learns optimal buy/sell thresholds from market outcomes
        bool enable_adaptive_thresholds = false;
        
        /// Configuration for adaptive threshold learning system
        AdaptiveConfig adaptive_config;
        
        /// Enable high-frequency momentum scalping algorithm
        /// When enabled, uses regime-adaptive momentum scalper for 100+ daily trades
        bool enable_momentum_scalping = false;
        
        /// Configuration for momentum scalping algorithm
        RegimeAdaptiveMomentumScalper::ScalperConfig scalper_config;
    };

    explicit BackendComponent(const BackendConfig& config);
    ~BackendComponent();

    // Main processing: turn signals + bars into executed trades
    std::vector<TradeOrder> process_signals(
        const std::string& signal_file_path,
        const std::string& market_data_path,
        const BackendConfig& config
    );

    // (DB export removed) Use process_to_jsonl instead

    // File-based trade book writer (JSONL). Writes only executed trades.
    bool process_to_jsonl(
        const std::string& signal_file_path,
        const std::string& market_data_path,
        const std::string& out_path,
        const std::string& run_id,
        size_t start_index = 0,
        size_t max_count = static_cast<size_t>(-1)
    );

private:
    BackendConfig config_;
    std::unique_ptr<class PortfolioManager> portfolio_manager_;
    std::unique_ptr<class StaticPositionManager> position_manager_;
    std::unique_ptr<LeveragePositionValidator> leverage_validator_; // <-- NEW
    std::unique_ptr<LeverageSelector> leverage_selector_; // <-- NEW
    std::unique_ptr<class AdaptivePortfolioManager> adaptive_portfolio_manager_; // <-- PHANTOM ORDER FIX
    std::unique_ptr<AdaptiveThresholdManager> adaptive_threshold_manager_; // <-- ADAPTIVE THRESHOLDS
    std::unique_ptr<PositionStateMachine> position_state_machine_; // <-- POSITION STATE MACHINE
    std::unique_ptr<RegimeAdaptiveMomentumScalper> momentum_scalper_; // <-- MOMENTUM SCALPER

    TradeOrder evaluate_signal(const SignalOutput& signal, const Bar& bar);
    TradeOrder convert_psm_transition_to_order(const PositionStateMachine::StateTransition& transition, 
                                               const SignalOutput& signal, const Bar& bar);
    std::string determine_target_symbol(const PositionStateMachine::StateTransition& transition);
    bool is_buy_transition(const PositionStateMachine::StateTransition& transition);
    bool is_sell_transition(const PositionStateMachine::StateTransition& transition);
    bool check_conflicts(const TradeOrder& order);
    double calculate_fees(double trade_value);
    double calculate_position_size(double signal_probability, double available_capital);
    
    // --- NEW LEVERAGE METHODS (Requirement 4) ---
    /// Select the optimal leverage instrument based on signal characteristics
    /// @param probability Signal probability (0.0 to 1.0)
    /// @param confidence Signal confidence (0.0 to 1.0)
    /// @return Recommended symbol for the trade, or "HOLD" if no recommendation
    std::string select_optimal_instrument(double probability, double confidence) const;
    
    /// Calculate risk-adjusted position size based on leverage factor
    /// @param symbol The leverage instrument symbol
    /// @param base_position_size Base position size for 1x leverage
    /// @return Risk-adjusted position size
    double calculate_risk_adjusted_size(const std::string& symbol, double base_position_size) const;
};

} // namespace sentio


