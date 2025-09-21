# Negative Returns Investigation - Complete Source Analysis

**Generated**: September 22, 2025  
**Source Directory**: /Users/yeogirlyun/C++/sentio_trader  
**Description**: Critical investigation into systematic negative returns across all trading algorithms with complete source code analysis

---

## 📋 **Table of Contents**

1. [🚨 Critical Bug Report](#critical-bug-report)
2. [💻 Backend Component Analysis](#backend-component-analysis)
3. [🏦 Portfolio Manager Analysis](#portfolio-manager-analysis)
4. [⚙️ Configuration and Cost Models](#configuration-and-cost-models)
5. [📊 Market Data and Signal Analysis](#market-data-and-signal-analysis)
6. [🔍 Investigation Findings](#investigation-findings)
7. [🎯 Recommended Fixes](#recommended-fixes)

---

## 🚨 **CRITICAL BUG REPORT SUMMARY**

### System Failure Evidence
**ALL algorithms show consistent negative returns:**
- 20 blocks: -1.00% return across all algorithms
- 50 blocks: -0.15% return across all algorithms  
- 100 blocks: -1.66% return across all algorithms

### Key Symptoms
1. **100% Negative Return Rate**: Not a single positive result
2. **Identical Performance**: All algorithms show same losses
3. **Zero Realized P&L**: All profits/losses are unrealized
4. **Scaling Losses**: Longer periods = larger losses
5. **No Profit-Taking**: System never realizes gains

---

## 💻 **BACKEND COMPONENT ANALYSIS**

### Fee Calculation Function
**Location**: `src/backend/backend_component.cpp:536-584`

```cpp
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
            fees = trade_value * 0.001;  // 0.1% - REASONABLE
            break;
        case CostModel::ALPACA:
            fees = 0.0;  // Alpaca zero commission model
            break;
        default:
            utils::log_error("CRITICAL: Unknown cost_model=" + std::to_string(static_cast<int>(config_.cost_model)) + 
                            " for trade_value=" + std::to_string(trade_value));
            std::abort();
    }
    
    return fees;
}
```

**Analysis**: Fee structure appears reasonable (0.1% or $1 fixed). Not the primary cause.

### Position Sizing Function
**Location**: `src/backend/backend_component.cpp:586-618`

```cpp
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
```

**🚨 CRITICAL ISSUE IDENTIFIED**: Position sizing logic has a fundamental flaw!

**Problem Analysis**:
- For signal_probability = 0.6: confidence_factor = (0.6 - 0.5) * 2.0 = 0.2 (20% of capital)
- For signal_probability = 0.7: confidence_factor = (0.7 - 0.5) * 2.0 = 0.4 (40% of capital)
- For signal_probability = 0.8: confidence_factor = (0.8 - 0.5) * 2.0 = 0.6 (60% of capital)

**This is reasonable and not the cause of losses.**

### Trading Logic Analysis
**Location**: `src/backend/backend_component.cpp:267-420`

The system implements:
1. **Initial Position Creation**: Creates new positions with aggressive sizing
2. **Position Scaling**: Adds to profitable positions (>2% gain)
3. **Rebalancing**: Adjusts positions based on signal strength
4. **No Profit-Taking**: ❌ **CRITICAL ISSUE** - System never sells profitable positions

**🚨 ROOT CAUSE IDENTIFIED**: **NO SELL ORDERS EXECUTED**

Looking at all test results:
- All trades are BUY orders
- Zero SELL orders executed
- All P&L remains unrealized
- No profit-taking mechanism active

---

## 🏦 **PORTFOLIO MANAGER ANALYSIS**

### Portfolio Manager Implementation
**Location**: `src/backend/portfolio_manager.cpp`

```cpp
void PortfolioManager::execute_buy(const std::string& symbol, double quantity,
                                   double price, double fees) {
    double total_cost = (quantity * price) + fees;
    cash_balance_ -= total_cost;
    if (cash_balance_ < -1e-9) {
        utils::log_error("Negative cash after BUY: symbol=" + symbol +
                         ", qty=" + std::to_string(quantity) +
                         ", price=" + std::to_string(price) +
                         ", fees=" + std::to_string(fees) +
                         ", cash_balance=" + std::to_string(cash_balance_));
        std::abort();
    }
    update_position(symbol, quantity, price);
}

void PortfolioManager::execute_sell(const std::string& symbol, double quantity,
                                    double price, double fees) {
    auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return; // ❌ SILENT FAILURE - Should log this
    }

    double proceeds = (quantity * price) - fees;
    cash_balance_ += proceeds;
    
    // Realized P&L calculation
    double gross_pnl = (price - it->second.avg_price) * quantity;
    realized_pnl_ += (gross_pnl - fees);

    // Update or remove position
    if (it->second.quantity > quantity) {
        it->second.quantity -= quantity;
    } else {
        positions_.erase(it);
    }
}
```

**Analysis**: Portfolio manager sell logic appears correct. The issue is that **sell orders are never being generated**.

---

## ⚙️ **CONFIGURATION AND COST MODELS**

### Cost Model Enumeration
**Location**: `include/common/types.h` (inferred)

```cpp
enum class CostModel {
    ZERO,       // No fees
    FIXED,      // $1 per trade
    PERCENTAGE, // 0.1% of trade value
    ALPACA      // Zero commission
};
```

**Investigation Needed**: What cost model is actually being used in tests?

---

## 📊 **MARKET DATA AND SIGNAL ANALYSIS**

### Signal Quality Investigation

From our tests, we observed:
- Signals range from 0.194 to 0.864
- 35.9% of signals > 0.6 (should trigger buys)
- 21.3% of signals < 0.4 (should trigger sells)
- System generates 18-21 BUY orders but 0 SELL orders

**🚨 CRITICAL FINDING**: The system is **NEVER generating sell signals** that result in actual trades.

### Market Condition Analysis

All test periods show:
- Multiple BUY orders executed
- Positions accumulated in QQQ and TQQQ (bullish instruments)
- All unrealized P&L is negative
- **Hypothesis**: Market was bearish during test periods, but system only bought bullish instruments

---

## 🔍 **INVESTIGATION FINDINGS**

### Root Cause Analysis

**PRIMARY ISSUE**: **No Profit-Taking Mechanism**
- System accumulates positions but never sells them
- All gains/losses remain unrealized
- No mechanism to lock in profits or cut losses

**SECONDARY ISSUES**:

1. **Market Timing Problem**:
   - System may be buying at market peaks
   - No analysis of entry timing quality
   - Leverage instruments (TQQQ) amplify losses

2. **Sell Signal Logic Failure**:
   - Despite 21.3% of signals being < 0.4, no sell orders execute
   - Sell logic may have bugs preventing execution
   - Position validation may be blocking legitimate sells

3. **Market Regime Mismatch**:
   - System buys bullish instruments (QQQ, TQQQ) in bearish periods
   - No adaptation to market conditions
   - Leverage amplifies directional mistakes

### Detailed Sell Logic Investigation

**Location**: `src/backend/backend_component.cpp:421-521`

The sell logic includes multiple strategies:
1. **Full Liquidation**: For strong sell signals or significant losses
2. **Profit Taking**: For profitable positions with weak signals  
3. **Risk Reduction**: For breakeven positions

**🚨 CRITICAL DISCOVERY**: Looking at the sell conditions:

```cpp
// STRATEGY 2: Partial profit-taking for profitable positions with weak signals
} else if (current_pnl_percent > 0.03 &&         // Position is profitable (>3%)
          signal.probability < sell_threshold + 0.02) { // Moderately weak signal
```

**THE PROBLEM**: Positions must be **profitable (+3%)** to trigger profit-taking, but all positions are **losing money**!

**THE LOGIC FLAW**: 
- System only sells profitable positions on weak signals
- System only sells losing positions on very strong sell signals
- **No mechanism to cut moderate losses**
- Positions get stuck in "unrealized loss limbo"

---

## 🎯 **RECOMMENDED FIXES**

### IMMEDIATE FIXES (P0 - Critical)

#### 1. **Add Loss-Cutting Mechanism**
```cpp
// Add this condition to sell logic:
} else if (current_pnl_percent < -0.02) { // Position losing >2%
    // Sell 50% to cut losses
    double loss_cut_percentage = 0.5;
    order.quantity = position.quantity * loss_cut_percentage;
    order.action = TradeAction::SELL;
    order.execution_reason = "LOSS CUTTING: Reducing losing position";
}
```

#### 2. **Add Time-Based Profit Taking**
```cpp
// Add position age tracking and time-based exits
if (position_age > MAX_HOLD_PERIOD) {
    // Force exit after holding too long
    order.action = TradeAction::SELL;
    order.execution_reason = "TIME EXIT: Maximum hold period reached";
}
```

#### 3. **Fix Sell Signal Execution**
```cpp
// Simplify sell conditions - sell on any sell signal
if (signal.probability < sell_threshold) {
    if (portfolio_manager_->has_position(order.symbol)) {
        // ALWAYS sell on sell signal, regardless of P&L
        order.action = TradeAction::SELL;
        order.quantity = position.quantity * 0.5; // Sell half
        order.execution_reason = "SELL SIGNAL: Reducing position on weak signal";
    }
}
```

#### 4. **Add Market Regime Detection**
```cpp
// Detect market trend and adjust instrument selection
if (market_trend_is_bearish()) {
    // Use inverse instruments (SQQQ, PSQ) instead of bullish ones
    selected_instrument = select_bearish_instrument(signal);
} else {
    selected_instrument = select_bullish_instrument(signal);
}
```

### VALIDATION TESTS (P1 - High Priority)

#### 1. **Test Sell Logic Directly**
```bash
# Create test with strong sell signals to verify sell execution
./build/sentio_cli trade --signals latest --blocks 10 --buy 0.9 --sell 0.1
```

#### 2. **Test Different Cost Models**
```bash
# Test with zero fees to isolate fee impact
./build/sentio_cli trade --signals latest --blocks 20 --cost-model zero
```

#### 3. **Test Market Regime Adaptation**
```bash
# Test in different market periods
./build/sentio_cli trade --signals latest --blocks 20 --start-date bearish-period
```

### LONG-TERM IMPROVEMENTS (P2 - Important)

1. **Dynamic Position Management**
   - Implement trailing stops
   - Add profit targets
   - Risk-based position sizing

2. **Market Regime Detection**
   - Detect bull/bear markets
   - Adapt instrument selection
   - Adjust position sizing

3. **Performance Attribution**
   - Track entry/exit quality
   - Measure timing effectiveness
   - Identify loss sources

---

## 🚨 **CRITICAL ACTION ITEMS**

### Immediate Investigation Required

1. **Check Cost Model Configuration**
   ```bash
   grep -r "cost_model" src/ include/ --include="*.cpp" --include="*.h"
   ```

2. **Analyze Sell Signal Generation**
   ```bash
   grep "SELL ORDER" logs/debug.log | wc -l  # Should be > 0
   ```

3. **Verify Market Data Quality**
   ```bash
   # Check if prices are reasonable and current
   head -10 data/equities/QQQ_RTH_NH.csv
   ```

4. **Test Simplified Sell Logic**
   - Temporarily remove all sell conditions except basic signal check
   - Force sell orders to execute and verify P&L calculation

### Success Criteria

- [ ] At least one algorithm shows positive returns in favorable conditions
- [ ] Sell orders are generated and executed
- [ ] Realized P&L becomes non-zero
- [ ] Loss-cutting mechanisms prevent excessive drawdowns
- [ ] Different algorithms show different performance characteristics

---

## 📞 **EMERGENCY PROTOCOL**

This represents a **system-critical failure** that renders the trading platform unusable. 

**Immediate Actions Required**:
1. **Stop all production deployment** until fixed
2. **Emergency investigation** into sell logic and position management
3. **Complete audit** of trading logic and market timing
4. **Validation testing** with known profitable scenarios

**The system currently has a 100% failure rate and would destroy capital in production use.**

---

**CONCLUSION**: The primary issue appears to be **inadequate sell logic** that prevents the system from realizing profits or cutting losses effectively. The system accumulates positions but never exits them profitably, leading to systematic negative returns as unrealized losses accumulate.

**PRIORITY**: **P0 Emergency Fix Required** - System is fundamentally broken and unsuitable for any trading use until this issue is resolved.
