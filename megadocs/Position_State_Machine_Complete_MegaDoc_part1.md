# Position State Machine Complete Implementation

**Part 1 of 8**

**Generated**: 2025-09-22 01:45:56
**Source Directory**: /Users/yeogirlyun/C++/sentio_trader
**Description**: Comprehensive requirements document and source code analysis for implementing a theoretically sound Position State Machine to replace ad-hoc trading logic with systematic state-based decision making. Includes complete requirements specification, current system analysis, and integration architecture.

**Part 1 Files**: See file count below

---

## 🐛 **BUG REPORT**

# Position State Machine Requirements Document

**Document ID**: PSM-REQ-001  
**Version**: 1.0  
**Date**: September 22, 2025  
**Status**: DRAFT - Requirements Definition  
**Priority**: P0 - Critical System Architecture  

---

## 📋 **Executive Summary**

### **Current Problem**
The existing trading system uses ad-hoc position management logic that leads to:
- **Inconsistent decision making** across different market scenarios
- **Suboptimal position transitions** (e.g., emergency sell fix overrides)
- **No systematic approach** to multi-instrument portfolio management
- **Poor integration** between leverage selection and position management

### **Proposed Solution**
Implement a **comprehensive Position State Machine (PSM)** that:
1. **Defines all possible portfolio states** (QQQ, TQQQ, PSQ, SQQQ combinations)
2. **Maps signal scenarios** to optimal state transitions
3. **Integrates with adaptive thresholds** for dynamic optimization
4. **Provides theoretical foundation** for position management decisions
5. **Ensures consistent behavior** across all market conditions

### **Expected Benefits**
- **Systematic decision making** based on portfolio theory
- **Optimal position transitions** for all signal scenarios
- **Improved risk management** through state-aware decisions
- **Better capital efficiency** through intelligent rebalancing
- **Enhanced performance** through theoretically sound approach

---

## 🎯 **Requirements Overview**

### **1. Core Requirements**
- **REQ-PSM-001**: Define complete state space for all position combinations
- **REQ-PSM-002**: Map all signal scenarios to optimal state transitions
- **REQ-PSM-003**: Implement theoretical optimization framework
- **REQ-PSM-004**: Integrate with adaptive threshold mechanism
- **REQ-PSM-005**: Provide real-time state transition recommendations

### **2. Performance Requirements**
- **REQ-PERF-001**: State transition decisions in <1ms
- **REQ-PERF-002**: Support for 1000+ state transitions per second
- **REQ-PERF-003**: Memory usage <10MB for state machine
- **REQ-PERF-004**: Integration with existing portfolio manager

### **3. Integration Requirements**
- **REQ-INT-001**: Seamless integration with `AdaptivePortfolioManager`
- **REQ-INT-002**: Compatible with `AdaptiveThresholdManager`
- **REQ-INT-003**: Backward compatibility with existing trade execution
- **REQ-INT-004**: Configurable risk parameters and constraints

---

## 🏗️ **State Space Definition**

### **Portfolio States**
The system must handle all possible combinations of positions:

#### **Single Instrument States (4 states)**
1. **QQQ_ONLY**: Only QQQ position held
2. **TQQQ_ONLY**: Only TQQQ position held  
3. **PSQ_ONLY**: Only PSQ position held
4. **SQQQ_ONLY**: Only SQQQ position held

#### **Dual Instrument States (2 valid combinations)**
5. **QQQ_TQQQ**: Both QQQ and TQQQ positions (amplified long exposure)
6. **PSQ_SQQQ**: Both PSQ and SQQQ positions (amplified short exposure)

#### **Special States (2 states)**
7. **CASH_ONLY**: No positions held (100% cash)
8. **INVALID**: Any conflicting combination (e.g., QQQ + PSQ)

**Total State Space**: 8 defined states

### **Signal Scenarios**
For each portfolio state, the system must handle 4 signal scenarios:

1. **STRONG_BUY**: Signal > (buy_threshold + margin)
2. **WEAK_BUY**: Signal > buy_threshold but < (buy_threshold + margin)
3. **WEAK_SELL**: Signal < sell_threshold but > (sell_threshold - margin)
4. **STRONG_SELL**: Signal < (sell_threshold - margin)

**Total Scenarios**: 8 states × 4 signals = **32 decision scenarios**

---

## 📊 **Complete State Transition Matrix**

### **1. QQQ_ONLY State Transitions**

| Current State | Signal Type | Optimal Action | Target State | Theoretical Basis |
|---------------|-------------|----------------|--------------|-------------------|
| QQQ_ONLY | STRONG_BUY | Scale up with TQQQ | QQQ_TQQQ | Leverage profitable position |
| QQQ_ONLY | WEAK_BUY | Add to QQQ position | QQQ_ONLY | Conservative scaling |
| QQQ_ONLY | WEAK_SELL | Partial QQQ liquidation | QQQ_ONLY | Risk reduction |
| QQQ_ONLY | STRONG_SELL | Full QQQ liquidation | CASH_ONLY | Capital preservation |

**Theoretical Framework**: 
- **Strong signals**: Use leverage to amplify exposure
- **Weak signals**: Conservative position adjustments
- **Risk management**: Gradual position reduction on weak sells

### **2. TQQQ_ONLY State Transitions**

| Current State | Signal Type | Optimal Action | Target State | Theoretical Basis |
|---------------|-------------|----------------|--------------|-------------------|
| TQQQ_ONLY | STRONG_BUY | Add QQQ for stability | QQQ_TQQQ | Diversify leverage risk |
| TQQQ_ONLY | WEAK_BUY | Scale up TQQQ | TQQQ_ONLY | Maintain leverage |
| TQQQ_ONLY | WEAK_SELL | Partial TQQQ → QQQ | QQQ_ONLY | De-leverage gradually |
| TQQQ_ONLY | STRONG_SELL | Full TQQQ liquidation | CASH_ONLY | Rapid de-risking |

**Theoretical Framework**:
- **Leverage management**: Balance risk vs. return
- **Volatility considerations**: TQQQ requires more careful management
- **Transition strategy**: Gradual de-leveraging preferred

### **3. PSQ_ONLY State Transitions**

| Current State | Signal Type | Optimal Action | Target State | Theoretical Basis |
|---------------|-------------|----------------|--------------|-------------------|
| PSQ_ONLY | STRONG_BUY | Full PSQ liquidation | CASH_ONLY | Directional reversal |
| PSQ_ONLY | WEAK_BUY | Partial PSQ liquidation | PSQ_ONLY | Gradual unwinding |
| PSQ_ONLY | WEAK_SELL | Add to PSQ position | PSQ_ONLY | Reinforce position |
| PSQ_ONLY | STRONG_SELL | Scale up with SQQQ | PSQ_SQQQ | Amplify short exposure |

**Theoretical Framework**:
- **Directional consistency**: Maintain short bias
- **Signal strength**: Strong opposing signals trigger reversals
- **Risk scaling**: Use SQQQ for amplified short exposure

### **4. SQQQ_ONLY State Transitions**

| Current State | Signal Type | Optimal Action | Target State | Theoretical Basis |
|---------------|-------------|----------------|--------------|-------------------|
| SQQQ_ONLY | STRONG_BUY | Full SQQQ liquidation | CASH_ONLY | Rapid directional reversal |
| SQQQ_ONLY | WEAK_BUY | Partial SQQQ → PSQ | PSQ_ONLY | Gradual de-leveraging |
| SQQQ_ONLY | WEAK_SELL | Scale up SQQQ | SQQQ_ONLY | Maintain leverage |
| SQQQ_ONLY | STRONG_SELL | Add PSQ for stability | PSQ_SQQQ | Diversify short risk |

**Theoretical Framework**:
- **Leverage de-risking**: Transition to PSQ on weak opposing signals
- **Short amplification**: Use dual positions for strong short signals
- **Risk management**: Rapid liquidation on strong reversals

### **5. QQQ_TQQQ State Transitions**

| Current State | Signal Type | Optimal Action | Target State | Theoretical Basis |
|---------------|-------------|----------------|--------------|-------------------|
| QQQ_TQQQ | STRONG_BUY | Scale both positions | QQQ_TQQQ | Amplify winning strategy |
| QQQ_TQQQ | WEAK_BUY | Add to QQQ only | QQQ_TQQQ | Conservative scaling |
| QQQ_TQQQ | WEAK_SELL | Liquidate TQQQ first | QQQ_ONLY | De-leverage gradually |
| QQQ_TQQQ | STRONG_SELL | Full liquidation | CASH_ONLY | Rapid risk reduction |

**Theoretical Framework**:
- **Leverage hierarchy**: TQQQ first out on sells, first in on strong buys
- **Risk management**: Gradual de-leveraging preferred
- **Capital efficiency**: Scale stable positions (QQQ) on weak signals

### **6. PSQ_SQQQ State Transitions**

| Current State | Signal Type | Optimal Action | Target State | Theoretical Basis |
|---------------|-------------|----------------|--------------|-------------------|
| PSQ_SQQQ | STRONG_BUY | Full liquidation | CASH_ONLY | Complete directional reversal |
| PSQ_SQQQ | WEAK_BUY | Liquidate SQQQ first | PSQ_ONLY | Gradual de-leveraging |
| PSQ_SQQQ | WEAK_SELL | Add to PSQ only | PSQ_SQQQ | Conservative scaling |
| PSQ_SQQQ | STRONG_SELL | Scale both positions | PSQ_SQQQ | Amplify short strategy |

**Theoretical Framework**:
- **Short leverage management**: SQQQ first out on opposing signals
- **Directional consistency**: Maintain short bias until strong reversal
- **Risk scaling**: Conservative additions on weak signals

### **7. CASH_ONLY State Transitions**

| Current State | Signal Type | Optimal Action | Target State | Theoretical Basis |
|---------------|-------------|----------------|--------------|-------------------|
| CASH_ONLY | STRONG_BUY | Initiate TQQQ position | TQQQ_ONLY | Maximize leverage on strong signal |
| CASH_ONLY | WEAK_BUY | Initiate QQQ position | QQQ_ONLY | Conservative entry |
| CASH_ONLY | WEAK_SELL | Initiate PSQ position | PSQ_ONLY | Conservative short entry |
| CASH_ONLY | STRONG_SELL | Initiate SQQQ position | SQQQ_ONLY | Maximize short leverage |

**Theoretical Framework**:
- **Signal strength determines leverage**: Strong signals → leveraged instruments
- **Risk-adjusted entry**: Match instrument risk to signal confidence
- **Capital deployment**: Efficient use of available capital

### **8. INVALID State Handling**

| Current State | Signal Type | Optimal Action | Target State | Theoretical Basis |
|---------------|-------------|----------------|--------------|-------------------|
| INVALID | ANY | Emergency liquidation | CASH_ONLY | Risk containment |

**Theoretical Framework**:
- **Conflict resolution**: Immediate liquidation of conflicting positions
- **Risk management**: Prevent directional conflicts
- **System integrity**: Maintain valid state space

---

## 🧮 **Theoretical Optimization Framework**

### **1. Position Sizing Theory**

#### **Kelly Criterion Application**
For each state transition, optimal position size based on:

```
f* = (bp - q) / b
where:
- f* = fraction of capital to allocate
- b = odds received (expected return / risk)
- p = probability of success (signal confidence)
- q = probability of failure (1 - p)
```

#### **Risk Parity Considerations**
- **Equal risk contribution**: Balance risk across instruments
- **Volatility adjustment**: Scale positions by inverse volatility
- **Correlation effects**: Account for QQQ-TQQQ correlation

#### **Leverage Optimization**
```
Optimal_Leverage = Signal_Strength × Risk_Tolerance × (1 / Volatility)
```

### **2. State Transition Costs**

#### **Transaction Cost Model**
```
Total_Cost = Fixed_Fees + (Trade_Value × Fee_Rate) + Market_Impact + Opportunity_Cost
```

#### **Market Impact Estimation**
```
Market_Impact = α × (Trade_Size / Average_Volume)^β
where α, β are instrument-specific parameters
```

#### **Opportunity Cost Calculation**
```
Opportunity_Cost = |Expected_Return| × Delay_Time × Position_Size
```

### **3. Risk Management Framework**

#### **Value at Risk (VaR) Constraints**
```
Portfolio_VaR ≤ Max_VaR_Limit
where VaR = Portfolio_Value × Volatility × Z_score × √Time_Horizon
```

#### **Maximum Drawdown Control**
```
Max_Position_Size = Available_Capital × (Max_DD_Limit / Expected_Volatility)
```

#### **Correlation Risk Management**
```
Correlation_Adjustment = 1 + (ρ × Leverage_Factor)
where ρ = correlation between instruments
```

---

## 🔧 **Implementation Architecture**

### **1. Core Components**

#### **PositionStateMachine Class**
```cpp
class PositionStateMachine {
public:
    enum class State {
        CASH_ONLY,
        QQQ_ONLY,
        TQQQ_ONLY,
        PSQ_ONLY,
        SQQQ_ONLY,
        QQQ_TQQQ,
        PSQ_SQQQ,
        INVALID
    };
    
    enum class SignalType {
        STRONG_BUY,
        WEAK_BUY,
        WEAK_SELL,
        STRONG_SELL
    };
    
    struct StateTransition {
        State current_state;
        SignalType signal;
        State target_state;
        std::vector<TradeAction> actions;
        double expected_return;
        double risk_score;
        std::string rationale;
    };
    
    StateTransition get_optimal_transition(
        const PortfolioState& current_portfolio,
        const SignalOutput& signal,
        const MarketState& market_conditions
    );
    
private:
    std::map<std::pair<State, SignalType>, StateTransition> transition_matrix_;
    OptimizationEngine optimization_engine_;
    RiskManager risk_manager_;
};
```

#### **OptimizationEngine Class**
```cpp
class OptimizationEngine {
public:
    struct OptimizationResult {
        double optimal_position_size;
        double expected_return;
        double risk_score;
        double kelly_fraction;
        double var_estimate;
    };
    
    OptimizationResult optimize_transition(
        const StateTransition& transition,
        const PortfolioState& current_state,
        const MarketState& market_conditions
    );
    
private:
    double calculate_kelly_fraction(double win_prob, double expected_return, double risk);
    double estimate_market_impact(const std::string& symbol, double trade_size);
    double calculate_opportunity_cost(double expected_return, double delay);
};
```

#### **RiskManager Class**
```cpp
class RiskManager {
public:
    struct RiskConstraints {
        double max_position_size;
        double max_leverage;
        double max_var;
        double max_drawdown;
        double max_correlation_exposure;
    };
    
    bool validate_transition(
        const StateTransition& transition,
        const PortfolioState& current_state,
        const RiskConstraints& constraints
    );
    
    RiskConstraints get_dynamic_constraints(
        const MarketState& market_conditions,
        const PerformanceMetrics& recent_performance
    );
    
private:
    double calculate_portfolio_var(const PortfolioState& state);
    double estimate_max_drawdown(const StateTransition& transition);
    double calculate_correlation_risk(const std::vector<std::string>& symbols);
};
```

### **2. Integration Points**

#### **With AdaptivePortfolioManager**
```cpp
class AdaptivePortfolioManager {
private:
    std::unique_ptr<PositionStateMachine> state_machine_;
    
public:
    std::vector<TradeOrder> generate_orders(
        const SignalOutput& signal,
        const Bar& current_bar,
        const PortfolioState& current_state
    ) override {
        
        // Get optimal state transition
        auto transition = state_machine_->get_optimal_transition(
            current_state, signal, market_state_
        );
        
        // Convert transition to trade orders
        return convert_transition_to_orders(transition, current_bar);
    }
};
```

#### **With AdaptiveThresholdManager**
```cpp
class AdaptiveThresholdManager {
public:
    ThresholdPair get_state_aware_thresholds(
        const SignalOutput& signal,
        const Bar& bar,
        const PositionStateMachine::State& current_state
    ) {
        // Adjust thresholds based on current portfolio state
        // Different states may require different threshold sensitivities
        
        ThresholdPair base_thresholds = get_current_thresholds(signal, bar);
        
        // State-specific adjustments
        switch (current_state) {
            case PositionStateMachine::State::QQQ_TQQQ:
                // More conservative thresholds for leveraged positions
                return adjust_for_leverage(base_thresholds, 0.9);
                
            case PositionStateMachine::State::CASH_ONLY:
                // More aggressive thresholds for cash deployment
                return adjust_for_cash_deployment(base_thresholds, 1.1);
                
            // ... other state-specific adjustments
        }
        
        return base_thresholds;
    }
};
```

---

## 📈 **Performance Optimization Strategies**

### **1. State Transition Optimization**

#### **Transition Cost Minimization**
- **Batch similar transitions**: Group related trades to reduce costs
- **Timing optimization**: Execute during high liquidity periods
- **Partial transitions**: Gradual position changes to minimize market impact

#### **Capital Efficiency Maximization**
- **Optimal rebalancing**: Maintain target allocations efficiently
- **Cash drag minimization**: Keep cash levels at optimal levels
- **Leverage utilization**: Use leverage when risk-adjusted returns justify it

### **2. Risk-Adjusted Performance**

#### **Sharpe Ratio Optimization**
```
Target_Sharpe = (Expected_Return - Risk_Free_Rate) / Portfolio_Volatility
Optimize: max(Target_Sharpe) subject to risk constraints
```

#### **Information Ratio Maximization**
```
Information_Ratio = (Portfolio_Return - Benchmark_Return) / Tracking_Error
Focus on generating alpha while controlling tracking error
```

#### **Maximum Drawdown Control**
```
Drawdown_Control = Dynamic position sizing based on recent performance
Reduce positions during drawdown periods, increase during profitable periods
```

### **3. Adaptive Learning Integration**

#### **State-Specific Learning**
- **Different learning rates** for different states
- **State transition success tracking**
- **Performance attribution** by state and transition type

#### **Market Regime Adaptation**
- **Bull market states**: Favor long positions and leverage
- **Bear market states**: Favor short positions and cash
- **Volatile markets**: Reduce position sizes and increase cash buffers

---

## 🧪 **Testing and Validation Framework**

### **1. Unit Testing Requirements**

#### **State Machine Logic**
- **All 32 transition scenarios** must be tested
- **Edge cases**: Invalid states, extreme signals, market conditions
- **Performance tests**: Transition speed and memory usage

#### **Optimization Engine**
- **Kelly criterion calculations** for various scenarios
- **Risk metric calculations** (VaR, drawdown, correlation)
- **Market impact estimation** accuracy

#### **Risk Manager**
- **Constraint validation** for all scenarios
- **Dynamic constraint adjustment** based on market conditions
- **Risk metric calculation** accuracy and speed

### **2. Integration Testing**

#### **With Existing Systems**
- **AdaptivePortfolioManager** integration
- **AdaptiveThresholdManager** compatibility
- **BackendComponent** trade execution
- **PortfolioManager** state tracking

#### **End-to-End Scenarios**
- **Complete trading sessions** with state transitions
- **Multi-day scenarios** with various market conditions
- **Stress testing** with extreme market events

### **3. Performance Validation**

#### **Backtesting Framework**
- **Historical data testing** across multiple market regimes
- **Performance comparison** vs. current system
- **Risk-adjusted return analysis**

#### **Monte Carlo Simulation**
- **Random market scenarios** generation
- **State transition robustness** testing
- **Risk management effectiveness** validation

---

## 📊 **Success Metrics**

### **1. Performance Metrics**

#### **Return Metrics**
- **Total Return**: Target >10% annual return
- **Risk-Adjusted Return**: Target Sharpe ratio >1.5
- **Alpha Generation**: Target >5% annual alpha vs. QQQ benchmark

#### **Risk Metrics**
- **Maximum Drawdown**: Target <10%
- **Value at Risk**: 95% confidence, 1-day VaR <2%
- **Volatility**: Target annual volatility 15-25%

#### **Efficiency Metrics**
- **Capital Utilization**: Target 80-90% deployment
- **Transaction Costs**: Target <1% annual drag
- **State Transition Success**: Target >70% profitable transitions

### **2. System Metrics**

#### **Performance Requirements**
- **Decision Speed**: <1ms per state transition
- **Memory Usage**: <10MB for state machine
- **Throughput**: >1000 decisions per second

#### **Reliability Requirements**
- **Uptime**: 99.9% availability
- **Error Rate**: <0.1% invalid transitions
- **Recovery Time**: <1 second from invalid states

### **3. Validation Criteria**

#### **Theoretical Validation**
- **Kelly criterion compliance**: Position sizes within 10% of optimal
- **Risk parity adherence**: Risk contributions balanced within 20%
- **Correlation management**: Portfolio correlation <0.8

#### **Practical Validation**
- **Outperform current system**: >2% annual return improvement
- **Reduce drawdowns**: <50% of current maximum drawdown
- **Improve consistency**: Reduce return volatility by 20%

---

## 🚀 **Implementation Roadmap**

### **Phase 1: Core State Machine (Week 1-2)**
- [ ] Define complete state space and transition matrix
- [ ] Implement `PositionStateMachine` class
- [ ] Create basic optimization framework
- [ ] Unit test all 32 transition scenarios

### **Phase 2: Optimization Engine (Week 3-4)**
- [ ] Implement Kelly criterion optimization
- [ ] Add risk management constraints
- [ ] Develop market impact estimation
- [ ] Create performance attribution system

### **Phase 3: Integration (Week 5-6)**
- [ ] Integrate with `AdaptivePortfolioManager`
- [ ] Connect to `AdaptiveThresholdManager`
- [ ] Update `BackendComponent` trade execution
- [ ] Comprehensive integration testing

### **Phase 4: Validation and Tuning (Week 7-8)**
- [ ] Extensive backtesting across market regimes
- [ ] Performance comparison vs. current system
- [ ] Parameter optimization and tuning
- [ ] Production readiness validation

### **Phase 5: Deployment (Week 9-10)**
- [ ] Production deployment with monitoring
- [ ] Real-time performance tracking
- [ ] Continuous optimization and learning
- [ ] Documentation and training

---

## 📚 **References and Theoretical Foundation**

### **Academic References**
1. **Kelly, J.L. (1956)**: "A New Interpretation of Information Rate" - Kelly Criterion
2. **Markowitz, H. (1952)**: "Portfolio Selection" - Modern Portfolio Theory
3. **Sharpe, W.F. (1966)**: "Mutual Fund Performance" - Risk-Adjusted Returns
4. **Black, F. & Litterman, R. (1992)**: "Global Portfolio Optimization" - Risk Parity

### **Industry Best Practices**
1. **Risk Management**: Basel III capital requirements adaptation
2. **Algorithmic Trading**: High-frequency trading risk controls
3. **Portfolio Theory**: Institutional asset management practices
4. **State Machines**: Real-time trading system architectures

### **Implementation Guides**
1. **C++ Design Patterns**: State machine implementation patterns
2. **Financial Engineering**: Quantitative risk management techniques
3. **System Architecture**: High-performance trading system design
4. **Testing Frameworks**: Financial system validation methodologies

---

**CONCLUSION**: This Position State Machine represents a **fundamental architectural improvement** that will transform the trading system from ad-hoc decision making to **theoretically sound, systematic position management**. The comprehensive state space coverage, optimization framework, and integration with adaptive mechanisms will provide the foundation for **consistent, profitable trading performance**.

**NEXT STEPS**: Begin Phase 1 implementation with core state machine development, followed by systematic integration and validation across all market scenarios.

---

**Document Version**: 1.0  
**Last Updated**: September 22, 2025  
**Next Review**: Upon Phase 1 Completion  
**Status**: APPROVED FOR IMPLEMENTATION  
**Priority**: P0 - Critical System Architecture


---

## 📋 **TABLE OF CONTENTS**

1. [include/backend/adaptive_portfolio_manager.h](#file-1)
2. [include/backend/adaptive_trading_mechanism.h](#file-2)
3. [include/backend/audit_component.h](#file-3)
4. [include/backend/backend_component.h](#file-4)
5. [include/backend/leverage_data_generator.h](#file-5)

---

## 📄 **FILE 1 of 39**: include/backend/adaptive_portfolio_manager.h

**File Information**:
- **Path**: `include/backend/adaptive_portfolio_manager.h`

- **Size**: 256 lines
- **Modified**: 2025-09-21 22:02:21

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: backend/adaptive_portfolio_manager.h
// Purpose: Comprehensive adaptive portfolio management system that eliminates
//          phantom sell orders and provides intelligent trading capabilities.
//
// Core Components:
// - Position Validator: Prevents phantom sell orders
// - Conflict Resolution Engine: Handles position conflicts automatically
// - Cash Balance Protector: Maintains positive cash balances
// - Profit Maximization Engine: Optimizes instrument selection
// - Risk Manager: Dynamic position sizing and risk controls
// =============================================================================

#include "common/types.h"
#include "strategy/signal_output.h"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sentio {

// Forward declarations
struct Position;
class BackendComponent;

// We need to use BackendComponent::TradeOrder but can't include the full header due to circular dependency
// So we'll define our own TradeOrder that's compatible
struct TradeOrder {
    TradeAction action = TradeAction::HOLD;
    std::string symbol;
    double quantity = 0.0;
    double price = 0.0;
    double trade_value = 0.0;
    double fees = 0.0;
    std::string execution_reason;
    double confidence = 0.0;
    
    TradeOrder() = default;
    TradeOrder(TradeAction act, const std::string& sym, double qty, double prc)
        : action(act), symbol(sym), quantity(qty), price(prc) {
        trade_value = quantity * price;
    }
};

// TradeAction is already defined in common/types.h

// ===================================================================
// POSITION VALIDATOR - Prevents phantom sell orders
// ===================================================================

class PositionValidator {
public:
    struct ValidationResult {
        bool is_valid = false;
        std::string error_message;
        double validated_quantity = 0.0;
    };

    /// Validates a sell order to prevent phantom orders
    /// This is the CRITICAL function that fixes the phantom sell order bug
    ValidationResult validate_sell_order(const std::string& symbol, 
                                       double requested_quantity,
                                       const std::map<std::string, Position>& positions) const;
    
    /// Validates a buy order for cash availability
    bool validate_buy_order(const std::string& symbol, 
                          double quantity, 
                          double price, 
                          double available_cash,
                          double fees = 0.0) const;
};

// ===================================================================
// CONFLICT RESOLUTION ENGINE - Handles position conflicts
// ===================================================================

class ConflictResolutionEngine {
public:
    struct ConflictAnalysis {
        bool has_conflicts = false;
        std::vector<std::string> conflicting_symbols;
        std::vector<TradeOrder> liquidation_orders;
        std::string resolution_strategy;
    };

    /// Analyzes potential conflicts with proposed symbol
    ConflictAnalysis analyze_conflicts(const std::string& proposed_symbol,
                                     const std::map<std::string, Position>& current_positions,
                                     double current_price) const;
    
    /// Automatically resolves conflicts by generating liquidation orders
    std::vector<TradeOrder> resolve_conflicts_automatically(const ConflictAnalysis& analysis) const;

private:
    bool would_conflict(const std::string& proposed, const std::string& existing) const;
};

// ===================================================================
// CASH BALANCE PROTECTOR - Prevents negative cash balances
// ===================================================================

class CashBalanceProtector {
private:
    double minimum_cash_reserve_;
    
public:
    explicit CashBalanceProtector(double min_reserve = 1000.0);
    
    struct CashValidationResult {
        bool is_valid = false;
        std::string error_message;
        double projected_cash = 0.0;
        double max_affordable_quantity = 0.0;
    };
    
    /// Validates transaction won't cause negative cash balance
    CashValidationResult validate_transaction(const TradeOrder& order, 
                                            double current_cash,
                                            double fee_rate = 0.001) const;
    
    /// Adjusts order size to fit cash constraints
    TradeOrder adjust_order_for_cash_constraints(const TradeOrder& original_order,
                                                double current_cash,
                                                double fee_rate = 0.001) const;
};

// ===================================================================
// PROFIT MAXIMIZATION ENGINE - Intelligent instrument selection
// ===================================================================

class ProfitMaximizationEngine {
public:
    struct InstrumentAnalysis {
        std::string symbol;
        double profit_potential = 0.0;
        double risk_score = 0.0;
        double leverage_factor = 1.0;
        double confidence_adjustment = 1.0;
        double final_score = 0.0;
    };
    
    /// Selects optimal instrument based on signal and portfolio state
    std::string select_optimal_instrument(const SignalOutput& signal, 
                                        const std::map<std::string, Position>& current_positions) const;

private:
    InstrumentAnalysis analyze_instrument(const std::string& symbol, 
                                        const SignalOutput& signal,
                                        double leverage_factor) const;
    
    std::vector<InstrumentAnalysis> filter_conflicting_instruments(
        const std::vector<InstrumentAnalysis>& candidates,
        const std::map<std::string, Position>& current_positions) const;
    
    bool would_conflict(const std::string& proposed, const std::string& existing) const;
};

// ===================================================================
// RISK MANAGER - Dynamic position sizing and risk controls
// ===================================================================

class RiskManager {
private:
    double max_position_size_;
    double max_portfolio_risk_;
    double volatility_adjustment_factor_;
    
public:
    RiskManager(double max_pos_size = 0.25, double max_portfolio_risk = 0.15, double vol_adj = 0.1);
    
    struct RiskAnalysis {
        double recommended_position_size = 0.0;
        double max_safe_quantity = 0.0;
        double risk_score = 0.0;
        std::string risk_level; // "LOW", "MEDIUM", "HIGH", "EXTREME"
        std::vector<std::string> risk_warnings;
    };
    
    /// Calculates optimal position size with risk controls
    RiskAnalysis calculate_optimal_position_size(const std::string& symbol,
                                               const SignalOutput& signal,
                                               double available_capital,
                                               double current_price,
                                               const std::map<std::string, Position>& positions) const;

private:
    double get_leverage_factor(const std::string& symbol) const;
    double calculate_portfolio_risk_score(const std::string& new_symbol,
                                        double new_quantity,
                                        double new_price,
                                        const std::map<std::string, Position>& positions) const;
};

// ===================================================================
// ADAPTIVE PORTFOLIO MANAGER - Main orchestrator class
// ===================================================================

class AdaptivePortfolioManager {
private:
    std::unique_ptr<PositionValidator> position_validator_;
    std::unique_ptr<ConflictResolutionEngine> conflict_resolver_;
    std::unique_ptr<CashBalanceProtector> cash_protector_;
    std::unique_ptr<ProfitMaximizationEngine> profit_optimizer_;
    std::unique_ptr<RiskManager> risk_manager_;
    
    // Portfolio state
    std::map<std::string, Position> positions_;
    double cash_balance_;
    double total_portfolio_value_;
    
    // Configuration
    struct Config {
        double buy_threshold = 0.6;
        double sell_threshold = 0.4;
        double fee_rate = 0.001;
        bool enable_auto_conflict_resolution = true;
        bool enable_risk_management = true;
        double minimum_cash_reserve = 1000.0;
    } config_;
    
public:
    explicit AdaptivePortfolioManager(double initial_cash = 100000.0);
    
    /// Main method that replaces the buggy evaluate_signal logic
    /// This method GUARANTEES no phantom sell orders
    std::vector<TradeOrder> execute_adaptive_trade(const SignalOutput& signal, const Bar& bar);
    
    /// Execute individual orders with validation
    bool execute_order(const TradeOrder& order);
    
    // Portfolio state access
    const std::map<std::string, Position>& get_positions() const { return positions_; }
    double get_cash_balance() const { return cash_balance_; }
    double get_total_portfolio_value() const;
    bool has_position(const std::string& symbol) const;
    Position get_position(const std::string& symbol) const;
    
private:
    bool validate_inputs(const SignalOutput& signal, const Bar& bar) const;
    TradeAction determine_trade_action(const SignalOutput& signal) const;
    TradeOrder create_main_order(TradeAction action, const std::string& symbol, 
                               const SignalOutput& signal, const Bar& bar) const;
    TradeOrder apply_risk_management(const TradeOrder& order, const SignalOutput& signal, const Bar& bar) const;
    TradeOrder create_hold_order(const std::string& reason) const;
    bool execute_buy_order(const TradeOrder& order);
    bool execute_sell_order(const TradeOrder& order);
    double calculate_total_portfolio_value() const;
};

} // namespace sentio

```

## 📄 **FILE 2 of 39**: include/backend/adaptive_trading_mechanism.h

**File Information**:
- **Path**: `include/backend/adaptive_trading_mechanism.h`

- **Size**: 502 lines
- **Modified**: 2025-09-21 23:54:35

- **Type**: .h

```text
#pragma once

#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <cmath>
#include <random>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "common/types.h"
#include "strategy/signal_output.h"

// Forward declarations to avoid circular dependencies
namespace sentio {
    class BackendComponent;
}

namespace sentio {

// ===================================================================
// THRESHOLD PAIR STRUCTURE
// ===================================================================

/**
 * @brief Represents a pair of buy and sell thresholds for trading decisions
 * 
 * The ThresholdPair encapsulates the core decision boundaries for the adaptive
 * trading system. Buy threshold determines when signals trigger buy orders,
 * sell threshold determines sell orders, with a neutral zone between them.
 */
struct ThresholdPair {
    double buy_threshold = 0.6;   // Probability threshold for buy orders
    double sell_threshold = 0.4;  // Probability threshold for sell orders
    
    ThresholdPair() = default;
    ThresholdPair(double buy, double sell) : buy_threshold(buy), sell_threshold(sell) {}
    
    /**
     * @brief Validates that thresholds are within acceptable bounds
     * @return true if thresholds are valid, false otherwise
     */
    bool is_valid() const {
        return buy_threshold > sell_threshold + 0.05 && // Min 5% gap
               buy_threshold >= 0.51 && buy_threshold <= 0.90 &&
               sell_threshold >= 0.10 && sell_threshold <= 0.49;
    }
    
    /**
     * @brief Gets the size of the neutral zone between thresholds
     * @return Size of neutral zone (buy_threshold - sell_threshold)
     */
    double get_neutral_zone_size() const {
        return buy_threshold - sell_threshold;
    }
};

// ===================================================================
// MARKET STATE AND REGIME DETECTION
// ===================================================================

/**
 * @brief Enumeration of different market regimes for adaptive threshold selection
 */
enum class MarketRegime {
    BULL_LOW_VOL,     // Rising prices, low volatility - aggressive thresholds
    BULL_HIGH_VOL,    // Rising prices, high volatility - moderate thresholds
    BEAR_LOW_VOL,     // Falling prices, low volatility - moderate thresholds
    BEAR_HIGH_VOL,    // Falling prices, high volatility - conservative thresholds
    SIDEWAYS_LOW_VOL, // Range-bound, low volatility - balanced thresholds
    SIDEWAYS_HIGH_VOL // Range-bound, high volatility - conservative thresholds
};

/**
 * @brief Comprehensive market state information for adaptive decision making
 */
struct MarketState {
    double current_price = 0.0;
    double volatility = 0.0;          // 20-day volatility measure
    double trend_strength = 0.0;      // -1 (strong bear) to +1 (strong bull)
    double volume_ratio = 1.0;        // Current volume / average volume
    MarketRegime regime = MarketRegime::SIDEWAYS_LOW_VOL;
    
    // Signal distribution statistics
    double avg_signal_strength = 0.5;
    double signal_volatility = 0.1;
    
    // Portfolio state
    int open_positions = 0;
    double cash_utilization = 0.0;    // 0.0 to 1.0
};

/**
 * @brief Detects and classifies market regimes for adaptive threshold optimization
 * 
 * The MarketRegimeDetector analyzes price history, volatility, and trend patterns
 * to classify current market conditions. This enables regime-specific threshold
 * optimization for improved performance across different market environments.
 */
class MarketRegimeDetector {
private:
    std::vector<double> price_history_;
    std::vector<double> volume_history_;
    const size_t LOOKBACK_PERIOD = 20;
    
public:
    /**
     * @brief Analyzes current market conditions and returns comprehensive market state
     * @param current_bar Current market data bar
     * @param recent_history Vector of recent bars for trend analysis
     * @param signal Current signal for context
     * @return MarketState with regime classification and metrics
     */
    MarketState analyze_market_state(const Bar& current_bar, 
                                   const std::vector<Bar>& recent_history,
                                   const SignalOutput& signal);
    
private:
    double calculate_volatility();
    double calculate_trend_strength();
    double calculate_volume_ratio();
    MarketRegime classify_market_regime(double volatility, double trend_strength);
};

// ===================================================================
// PERFORMANCE TRACKING AND EVALUATION
// ===================================================================

/**
 * @brief Represents the outcome of a completed trade for learning feedback
 */
struct TradeOutcome {
    // Store essential trade information instead of full TradeOrder to avoid circular dependency
    std::string symbol;
    TradeAction action = TradeAction::HOLD;
    double quantity = 0.0;
    double price = 0.0;
    double trade_value = 0.0;
    double fees = 0.0;
    double actual_pnl = 0.0;
    double pnl_percentage = 0.0;
    bool was_profitable = false;
    int bars_to_profit = 0;
    double max_adverse_move = 0.0;
    double sharpe_contribution = 0.0;
    std::chrono::system_clock::time_point outcome_timestamp;
};

/**
 * @brief Comprehensive performance metrics for adaptive learning evaluation
 */
struct PerformanceMetrics {
    double win_rate = 0.0;              // Percentage of profitable trades
    double profit_factor = 1.0;         // Gross profit / Gross loss
    double sharpe_ratio = 0.0;          // Risk-adjusted return
    double max_drawdown = 0.0;          // Maximum peak-to-trough decline
    double trade_frequency = 0.0;       // Trades per day
    double capital_efficiency = 0.0;    // Return on deployed capital
    double opportunity_cost = 0.0;      // Estimated missed profits
    std::vector<double> returns;        // Historical returns
    int total_trades = 0;
    int winning_trades = 0;
    int losing_trades = 0;
    double gross_profit = 0.0;
    double gross_loss = 0.0;
};

/**
 * @brief Evaluates trading performance and generates learning signals
 * 
 * The PerformanceEvaluator tracks trade outcomes, calculates comprehensive
 * performance metrics, and generates reward signals for the learning algorithms.
 * It maintains rolling windows of performance data for adaptive optimization.
 */
class PerformanceEvaluator {
private:
    std::vector<TradeOutcome> trade_history_;
    std::vector<double> portfolio_values_;
    const size_t MAX_HISTORY = 1000;
    const size_t PERFORMANCE_WINDOW = 100;
    
public:
    /**
     * @brief Adds a completed trade outcome for performance tracking
     * @param outcome TradeOutcome with P&L and timing information
     */
    void add_trade_outcome(const TradeOutcome& outcome);
    
    /**
     * @brief Adds portfolio value snapshot for drawdown calculation
     * @param value Current total portfolio value
     */
    void add_portfolio_value(double value);
    
    /**
     * @brief Calculates comprehensive performance metrics from recent trades
     * @return PerformanceMetrics with win rate, Sharpe ratio, drawdown, etc.
     */
    PerformanceMetrics calculate_performance_metrics();
    
    /**
     * @brief Calculates reward signal for learning algorithms
     * @param metrics Current performance metrics
     * @return Reward value for reinforcement learning
     */
    double calculate_reward_signal(const PerformanceMetrics& metrics);
    
private:
    double calculate_sharpe_ratio(const std::vector<double>& returns);
    double calculate_max_drawdown();
    double calculate_capital_efficiency();
};

// ===================================================================
// Q-LEARNING THRESHOLD OPTIMIZER
// ===================================================================

/**
 * @brief State-action pair for Q-learning lookup table
 */
struct StateActionPair {
    int state_hash;
    int action_index;
    
    bool operator<(const StateActionPair& other) const {
        return std::tie(state_hash, action_index) < std::tie(other.state_hash, other.action_index);
    }
};

/**
 * @brief Available actions for threshold adjustment in Q-learning
 */
enum class ThresholdAction {
    INCREASE_BUY_SMALL,      // +0.01
    INCREASE_BUY_MEDIUM,     // +0.03
    DECREASE_BUY_SMALL,      // -0.01
    DECREASE_BUY_MEDIUM,     // -0.03
    INCREASE_SELL_SMALL,     // +0.01
    INCREASE_SELL_MEDIUM,    // +0.03
    DECREASE_SELL_SMALL,     // -0.01
    DECREASE_SELL_MEDIUM,    // -0.03
    MAINTAIN_THRESHOLDS,     // No change
    COUNT
};

/**
 * @brief Q-Learning based threshold optimizer for adaptive trading
 * 
 * Implements reinforcement learning to find optimal buy/sell thresholds.
 * Uses epsilon-greedy exploration and Q-value updates to learn from
 * trading outcomes and maximize long-term performance.
 */
class QLearningThresholdOptimizer {
private:
    std::map<StateActionPair, double> q_table_;
    std::map<int, int> state_visit_count_;
    
    // Hyperparameters
    double learning_rate_ = 0.1;
    double discount_factor_ = 0.95;
    double exploration_rate_ = 0.1;
    double exploration_decay_ = 0.995;
    double min_exploration_ = 0.01;
    
    // State discretization
    const int THRESHOLD_BINS = 20;
    const int PERFORMANCE_BINS = 10;
    
    std::mt19937 rng_;
    
public:
    QLearningThresholdOptimizer();
    
    /**
     * @brief Selects next action using epsilon-greedy policy
     * @param state Current market state
     * @param current_thresholds Current threshold values
     * @param performance Recent performance metrics
     * @return Selected threshold action
     */
    ThresholdAction select_action(const MarketState& state, 
                                 const ThresholdPair& current_thresholds,
                                 const PerformanceMetrics& performance);
    
    /**
     * @brief Updates Q-value based on observed reward
     * @param prev_state Previous market state
     * @param prev_thresholds Previous thresholds
     * @param prev_performance Previous performance
     * @param action Action taken
     * @param reward Observed reward
     * @param new_state New market state
     * @param new_thresholds New thresholds
     * @param new_performance New performance
     */
    void update_q_value(const MarketState& prev_state,
                       const ThresholdPair& prev_thresholds,
                       const PerformanceMetrics& prev_performance,
                       ThresholdAction action,
                       double reward,
                       const MarketState& new_state,
                       const ThresholdPair& new_thresholds,
                       const PerformanceMetrics& new_performance);
    
    /**
     * @brief Applies selected action to current thresholds
     * @param current_thresholds Current threshold values
     * @param action Action to apply
     * @return New threshold values after action
     */
    ThresholdPair apply_action(const ThresholdPair& current_thresholds, ThresholdAction action);
    
    /**
     * @brief Gets current learning progress (1.0 - exploration_rate)
     * @return Learning progress from 0.0 to 1.0
     */
    double get_learning_progress() const;
    
private:
    int discretize_state(const MarketState& state, 
                        const ThresholdPair& thresholds,
                        const PerformanceMetrics& performance);
    double get_q_value(const StateActionPair& sa_pair);
    double get_max_q_value(int state_hash);
    ThresholdAction get_best_action(int state_hash);
};

// ===================================================================
// MULTI-ARMED BANDIT OPTIMIZER
// ===================================================================

/**
 * @brief Represents a bandit arm (threshold combination) with statistics
 */
struct BanditArm {
    ThresholdPair thresholds;
    double estimated_reward = 0.0;
    int pull_count = 0;
    double confidence_bound = 0.0;
    
    BanditArm(const ThresholdPair& t) : thresholds(t) {}
};

/**
 * @brief Multi-Armed Bandit optimizer for threshold selection
 * 
 * Implements UCB1 algorithm to balance exploration and exploitation
 * across different threshold combinations. Maintains confidence bounds
 * for each arm and selects based on upper confidence bounds.
 */
class MultiArmedBanditOptimizer {
private:
    std::vector<BanditArm> arms_;
    int total_pulls_ = 0;
    std::mt19937 rng_;
    
public:
    MultiArmedBanditOptimizer();
    
    /**
     * @brief Selects threshold pair using UCB1 algorithm
     * @return Selected threshold pair
     */
    ThresholdPair select_thresholds();
    
    /**
     * @brief Updates reward for selected threshold pair
     * @param thresholds Threshold pair that was used
     * @param reward Observed reward
     */
    void update_reward(const ThresholdPair& thresholds, double reward);
    
private:
    void initialize_arms();
    void update_confidence_bounds();
};

// ===================================================================
// ADAPTIVE THRESHOLD MANAGER - Main Orchestrator
// ===================================================================

/**
 * @brief Learning algorithm selection for adaptive threshold optimization
 */
enum class LearningAlgorithm {
    Q_LEARNING,           // Reinforcement learning approach
    MULTI_ARMED_BANDIT,   // UCB1 bandit algorithm
    ENSEMBLE              // Combination of multiple algorithms
};

/**
 * @brief Configuration parameters for adaptive threshold system
 */
struct AdaptiveConfig {
    LearningAlgorithm algorithm = LearningAlgorithm::Q_LEARNING;
    double learning_rate = 0.1;
    double exploration_rate = 0.1;
    int performance_window = 50;
    int feedback_delay = 5;
    double max_drawdown_limit = 0.05;
    bool enable_regime_adaptation = true;
    bool conservative_mode = false;
};

/**
 * @brief Main orchestrator for adaptive threshold management
 * 
 * The AdaptiveThresholdManager coordinates all components of the adaptive
 * trading system. It manages learning algorithms, performance evaluation,
 * market regime detection, and provides the main interface for getting
 * optimal thresholds and processing trade outcomes.
 */
class AdaptiveThresholdManager {
private:
    // Current state
    ThresholdPair current_thresholds_;
    MarketState current_market_state_;
    PerformanceMetrics current_performance_;
    
    // Learning components
    std::unique_ptr<QLearningThresholdOptimizer> q_learner_;
    std::unique_ptr<MultiArmedBanditOptimizer> bandit_optimizer_;
    std::unique_ptr<MarketRegimeDetector> regime_detector_;
    std::unique_ptr<PerformanceEvaluator> performance_evaluator_;
    
    // Configuration
    AdaptiveConfig config_;
    
    // State tracking
    std::queue<std::pair<TradeOutcome, std::chrono::system_clock::time_point>> pending_trades_;
    std::vector<Bar> recent_bars_;
    bool learning_enabled_ = true;
    bool circuit_breaker_active_ = false;
    
    // Regime-specific thresholds
    std::map<MarketRegime, ThresholdPair> regime_thresholds_;
    
public:
    /**
     * @brief Constructs adaptive threshold manager with configuration
     * @param config Configuration parameters for the adaptive system
     */
    AdaptiveThresholdManager(const AdaptiveConfig& config = AdaptiveConfig());
    
    /**
     * @brief Gets current optimal thresholds for given market conditions
     * @param signal Current signal output
     * @param bar Current market data bar
     * @return Optimal threshold pair for current conditions
     */
    ThresholdPair get_current_thresholds(const SignalOutput& signal, const Bar& bar);
    
    /**
     * @brief Processes trade outcome for learning feedback
     * @param symbol Trade symbol
     * @param action Trade action (BUY/SELL)
     * @param quantity Trade quantity
     * @param price Trade price
     * @param trade_value Trade value
     * @param fees Trade fees
     * @param actual_pnl Actual profit/loss from trade
     * @param pnl_percentage P&L as percentage of trade value
     * @param was_profitable Whether trade was profitable
     */
    void process_trade_outcome(const std::string& symbol, TradeAction action, 
                              double quantity, double price, double trade_value, double fees,
                              double actual_pnl, double pnl_percentage, bool was_profitable);
    
    /**
     * @brief Updates portfolio value for performance tracking
     * @param value Current total portfolio value
     */
    void update_portfolio_value(double value);
    
    // Control methods
    void enable_learning(bool enabled) { learning_enabled_ = enabled; }
    void reset_circuit_breaker() { circuit_breaker_active_ = false; }
    bool is_circuit_breaker_active() const { return circuit_breaker_active_; }
    
    // Analytics methods
    PerformanceMetrics get_current_performance() const { return current_performance_; }
    MarketState get_current_market_state() const { return current_market_state_; }
    double get_learning_progress() const;
    
    /**
     * @brief Generates comprehensive performance report
     * @return Formatted string with performance metrics and insights
     */
    std::string generate_performance_report() const;
    
private:
    void initialize_regime_thresholds();
    void update_performance_and_learn();
    ThresholdPair get_regime_adapted_thresholds();
    ThresholdPair get_conservative_thresholds();
    void check_circuit_breaker();
};

} // namespace sentio

```

## 📄 **FILE 3 of 39**: include/backend/audit_component.h

**File Information**:
- **Path**: `include/backend/audit_component.h`

- **Size**: 34 lines
- **Modified**: 2025-09-21 00:30:30

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: backend/audit_component.h
// Purpose: Post-run analysis of trades and portfolio performance metrics.
//
// Core idea:
// - Consume executed trades and produce risk/performance analytics such as
//   Sharpe ratio, max drawdown, win rate, and conflict detection summaries.
// =============================================================================

#include <vector>
#include <string>
#include <map>

namespace sentio {

struct TradeSummary {
    int total_trades = 0;
    int wins = 0;
    int losses = 0;
    double win_rate = 0.0;
    double sharpe = 0.0;
    double max_drawdown = 0.0;
};

class AuditComponent {
public:
    TradeSummary analyze_equity_curve(const std::vector<double>& equity_curve) const;
};

} // namespace sentio



```

## 📄 **FILE 4 of 39**: include/backend/backend_component.h

**File Information**:
- **Path**: `include/backend/backend_component.h`

- **Size**: 171 lines
- **Modified**: 2025-09-21 23:54:35

- **Type**: .h

```text
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

    TradeOrder evaluate_signal(const SignalOutput& signal, const Bar& bar);
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



```

## 📄 **FILE 5 of 39**: include/backend/leverage_data_generator.h

**File Information**:
- **Path**: `include/backend/leverage_data_generator.h`

- **Size**: 163 lines
- **Modified**: 2025-09-21 16:49:44

- **Type**: .h

```text
#pragma once

// =============================================================================
// Module: backend/leverage_data_generator.h
// Purpose: Accurate leverage data generation using daily return compounding
//
// Core Architecture & Design:
// This module implements the corrected leverage data generation model that
// accurately captures the path-dependent nature of leveraged ETFs through
// daily return compounding rather than simple price multiplication.
//
// Key Design Principles:
// - Daily return compounding: Models actual ETF rebalancing mechanism
// - Path dependency: Captures volatility decay and compounding effects
// - Realistic pricing: Generates data that matches actual ETF behavior
// - Accurate OHLC: Maintains proper intraday relationships
//
// Recent Improvements:
// - Corrected pricing model based on daily returns, not price multiplication
// - Proper handling of inverse instruments with OHLC inversion
// - Accurate modeling of daily rebalancing costs and expense ratios
// - Realistic volume scaling for leverage instruments
// =============================================================================

#include "common/types.h"
#include "leverage_types.h"
#include <vector>
#include <string>
#include <cmath>

namespace sentio {

/// Accurate leverage data generator using daily return compounding
/// 
/// This class generates theoretical OHLCV data for leveraged ETFs using
/// the corrected daily return compounding model that accurately captures
/// the path-dependent nature of leveraged instruments.
class LeverageDataGenerator {
public:
    /// Constructor with configurable parameters
    /// @param daily_decay_rate Daily rebalancing cost (default: 0.0001 = 0.01%)
    /// @param expense_ratio Annual expense ratio (default: 0.0095 = 0.95%)
    LeverageDataGenerator(double daily_decay_rate = 0.0001, 
                         double expense_ratio = 0.0095);
    
    /// Generate a theoretical OHLCV series for a leveraged ETF
    /// 
    /// This method uses the corrected daily return compounding model:
    /// 1. Calculate daily return of base asset (QQQ)
    /// 2. Apply leverage factor to daily return
    /// 3. Apply daily costs (decay + expense ratio)
    /// 4. Compound the leveraged return onto previous day's price
    /// 5. Generate OHLC by scaling intraday movements proportionally
    /// 
    /// @param qqq_data The base QQQ data series
    /// @param symbol The target symbol to generate (e.g., "TQQQ", "SQQQ", "PSQ")
    /// @return A vector of Bars for the leveraged instrument
    /// @throws std::runtime_error if symbol is not supported or is base instrument
    std::vector<Bar> generate_series(const std::vector<Bar>& qqq_data, 
                                   const std::string& symbol);
    
    /// Generate series for all supported leverage instruments
    /// @param qqq_data The base QQQ data series
    /// @return Map of symbol -> generated series
    std::map<std::string, std::vector<Bar>> generate_all_series(const std::vector<Bar>& qqq_data);
    
    /// Get the leverage specification for a symbol
    /// @param symbol The instrument symbol
    /// @return LeverageSpec for the symbol
    LeverageSpec get_leverage_spec(const std::string& symbol) const;
    
    /// Check if a symbol is supported for data generation
    /// @param symbol The instrument symbol
    /// @return true if the symbol can be generated
    bool is_supported_symbol(const std::string& symbol) const;

private:
    double daily_decay_rate_;      /// Daily rebalancing cost
    double daily_expense_rate_;    /// Daily expense ratio (annual / 252)
    
    /// Generate OHLC for a single bar using proportional scaling
    /// @param qqq_bar The base QQQ bar
    /// @param prev_lev_close Previous leveraged close price
    /// @param spec Leverage specification
    /// @return Generated leveraged bar
    Bar generate_bar_ohlc(const Bar& qqq_bar, double prev_lev_close, 
                         const LeverageSpec& spec) const;
    
    /// Calculate the daily return of the base asset
    /// @param prev_close Previous day's close price
    /// @param curr_close Current day's close price
    /// @return Daily return (0.0 if prev_close is 0 or invalid)
    double calculate_daily_return(double prev_close, double curr_close) const;
    
    /// Apply leverage factor and inversion to daily return
    /// @param daily_return Base daily return
    /// @param spec Leverage specification
    /// @return Leveraged daily return
    double apply_leverage_factor(double daily_return, const LeverageSpec& spec) const;
    
    /// Calculate total daily costs (decay + expense)
    /// @return Total daily cost rate
    double calculate_daily_costs() const;
    
    /// Ensure price is positive and reasonable
    /// @param price The price to validate
    /// @return Validated price (minimum 0.01)
    double validate_price(double price) const;
};

/// Utility functions for leverage data generation
namespace leverage_data_utils {

/// Calculate theoretical leverage price using daily return compounding
/// @param prev_price Previous leveraged price
/// @param base_daily_return Daily return of base asset
/// @param leverage_factor Leverage multiplier
/// @param is_inverse Whether instrument is inverse
/// @param daily_costs Total daily costs
/// @return New leveraged price
inline double calculate_leveraged_price(double prev_price, 
                                       double base_daily_return,
                                       double leverage_factor,
                                       bool is_inverse,
                                       double daily_costs) {
    // Apply leverage factor to daily return
    double leveraged_return = base_daily_return * leverage_factor;
    
    // Apply inversion if necessary
    if (is_inverse) {
        leveraged_return = -leveraged_return;
    }
    
    // Apply daily costs and compound
    double new_price = prev_price * (1.0 + leveraged_return - daily_costs);
    
    // Ensure positive price
    return std::max(0.01, new_price);
}

/// Scale intraday movements proportionally
/// @param base_movement Base asset intraday movement ratio
/// @param leverage_factor Leverage multiplier
/// @param is_inverse Whether instrument is inverse
/// @return Scaled movement ratio
inline double scale_intraday_movement(double base_movement, 
                                     double leverage_factor,
                                     bool is_inverse) {
    double scaled = base_movement * leverage_factor;
    return is_inverse ? -scaled : scaled;
}

/// Calculate volume scaling factor for leverage instruments
/// @param leverage_factor Leverage multiplier
/// @return Volume scaling factor (typically 0.3 for 3x leverage)
inline double calculate_volume_scaling(double leverage_factor) {
    // Higher leverage typically has lower volume due to higher risk
    return std::max(0.1, 1.0 / leverage_factor);
}

} // namespace leverage_data_utils

} // namespace sentio

```

