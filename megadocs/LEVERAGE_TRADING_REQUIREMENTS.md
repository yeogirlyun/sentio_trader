# Sentio Trading System - Leverage Trading Requirements Document

## Executive Summary

This document defines the comprehensive requirements for implementing leverage trading support in the Sentio Trading System. The system will support leveraged ETFs (TQQQ, SQQQ, PSQ) with strict position conflict prevention and maximum profit optimization through intelligent leverage selection.

## 1. Leverage Instrument Specifications

### 1.1 Supported Leverage Instruments

| Symbol | Base Asset | Leverage Factor | Direction | Description |
|--------|------------|-----------------|-----------|-------------|
| QQQ    | QQQ        | 1.0x           | Long      | Base QQQ ETF |
| TQQQ   | QQQ        | 3.0x           | Long      | 3x Leveraged QQQ |
| SQQQ   | QQQ        | 3.0x           | Short     | 3x Inverse QQQ |
| PSQ    | QQQ        | 1.0x           | Short     | 1x Inverse QQQ |

### 1.2 Leverage Relationships

```cpp
struct LeverageSpec {
    std::string base_symbol;    // "QQQ"
    double leverage_factor;    // 1.0, 3.0
    bool is_inverse;          // false for TQQQ, true for SQQQ/PSQ
    double expense_ratio;      // Annual expense ratio
    double daily_decay;        // Daily rebalancing cost
};
```

## 2. Position Conflict Prevention Rules

### 2.1 Position Conflict Rules - CORRECTED LOGIC

**CORE RULE: A conflict exists if, and only if, the portfolio holds both a LONG instrument (QQQ/TQQQ) and a SHORT/INVERSE instrument (PSQ/SQQQ) at the same time.**

This single, clear rule correctly prevents capital drain from opposing leveraged positions while allowing for flexible allocation on one side of the market.

#### Prohibited Combinations (Directional Conflicts):
- ❌ QQQ + PSQ (long QQQ vs short QQQ)
- ❌ QQQ + SQQQ (long QQQ vs short QQQ)
- ❌ TQQQ + PSQ (long QQQ vs short QQQ)
- ❌ TQQQ + SQQQ (long QQQ vs short QQQ)

#### Allowed Combinations (No Directional Conflicts):
- ✅ QQQ only (long QQQ)
- ✅ TQQQ only (3x long QQQ)
- ✅ PSQ only (short QQQ)
- ✅ SQQQ only (3x short QQQ)
- ✅ QQQ + TQQQ (amplified long QQQ exposure)
- ✅ PSQ + SQQQ (amplified short QQQ exposure - **CORRECTED: This is now allowed**)

**IMPORTANT CORRECTION**: PSQ + SQQQ is now **ALLOWED** because both instruments provide short QQQ exposure. This is an allocation decision, not a directional conflict. The true conflict occurs when holding both long and short instruments simultaneously.

### 2.2 Allowed Position Combinations - UPDATED

**ONLY** the following combinations are permitted. All others are automatically rejected:

#### Single Positions:
- ✅ QQQ only
- ✅ TQQQ only  
- ✅ SQQQ only
- ✅ PSQ only

#### Dual Positions:
- ✅ QQQ + TQQQ (amplified long QQQ exposure)
- ✅ PSQ + SQQQ (amplified short QQQ exposure) - **NOW ALLOWED**

**NOTE: The corrected logic allows PSQ + SQQQ** - both provide short QQQ exposure, which is an allocation decision, not a directional conflict.

### 2.3 Position Validation Logic

```cpp
enum class PositionType {
    LONG_QQQ,      // QQQ or TQQQ
    SHORT_QQQ,     // PSQ or SQQQ
    NEUTRAL        // No positions
};

class LeveragePositionValidator {
public:
    // Check if a new position can be added
    bool validate_position_addition(const std::string& symbol);
    
    // Check if a position can be removed
    bool validate_position_removal(const std::string& symbol);
    
    // Get current position type
    PositionType get_current_position_type() const;
    
    // Get list of allowed symbols to add
    std::vector<std::string> get_allowed_additions() const;
    
private:
    // Simple whitelist approach - only allow explicitly permitted combinations
    bool is_allowed_combination(const std::vector<std::string>& positions) const;
    
    // Check if symbol is long QQQ exposure
    bool is_long_qqq_symbol(const std::string& symbol) const;
    
    // Check if symbol is short QQQ exposure  
    bool is_short_qqq_symbol(const std::string& symbol) const;
};
```

### 2.4 Validation Rules

The validation logic follows a simple whitelist approach:

1. **Single Positions**: Any individual symbol (QQQ, TQQQ, SQQQ, PSQ) is allowed
2. **Dual Positions**: Only QQQ + TQQQ is allowed (both long QQQ exposure)
3. **All Other Combinations**: Automatically rejected

This ensures maximum simplicity and prevents any conflicting positions.

## 3. Leverage Data Generation

### 3.1 Theoretical Pricing Model

Leverage instruments will be generated using theoretical pricing based on the base QQQ data:

```cpp
class LeverageDataGenerator {
public:
    // Generate TQQQ data (3x long QQQ)
    std::vector<Bar> generate_tqqq_data(const std::vector<Bar>& qqq_data);
    
    // Generate SQQQ data (3x short QQQ)  
    std::vector<Bar> generate_sqqq_data(const std::vector<Bar>& qqq_data);
    
    // Generate PSQ data (1x short QQQ)
    std::vector<Bar> generate_psq_data(const std::vector<Bar>& qqq_data);
    
private:
    double calculate_leverage_price(double base_price, double leverage_factor, 
                                  bool is_inverse, double daily_decay);
};
```

### 3.2 Pricing Formula - CORRECTED MODEL

**IMPORTANT CORRECTION**: The previous pricing formula was incorrect and would lead to unrealistic backtesting results. Leveraged ETFs rebalance daily to maintain their target leverage relative to the base asset's *daily* return.

#### Corrected Daily Return Compounding Model:

```
For each trading day:
1. Calculate daily return of base asset: qqq_return = (qqq_close_today / qqq_close_yesterday) - 1
2. Apply leverage factor to daily return: leveraged_return = qqq_return * leverage_factor * (is_inverse ? -1 : 1)
3. Apply daily costs: total_daily_cost = daily_decay_rate + daily_expense_rate
4. Calculate new leveraged price: lev_close_today = lev_close_yesterday * (1 + leveraged_return - total_daily_cost)
5. Generate OHLC by scaling intraday movements proportionally
```

#### Key Improvements:
- **Path Dependency**: Correctly models the compounding effects of daily rebalancing
- **Realistic Volatility**: Captures the amplified volatility of leveraged instruments
- **Accurate Long-term Performance**: Prevents unrealistic drift from actual ETF performance
- **Daily Rebalancing**: Models the actual mechanism leveraged ETFs use

#### Example Calculation:
```
Day 1: QQQ = $100, TQQQ = $100
Day 2: QQQ = $105 (+5%), TQQQ = $115 (3x * 5% = +15%)
Day 3: QQQ = $100 (-4.76%), TQQQ = $100.24 (3x * -4.76% = -14.28%)
```

This iterative approach correctly models the path-dependent nature of leveraged ETFs.

### 3.3 Data File Structure

Generated leverage files will follow the same format as QQQ_RTH_NH.csv:

```csv
ts_utc,ts_nyt_epoch,open,high,low,close,volume
2022-09-15 13:30:00,1663252200,45.23,45.67,44.89,45.12,1250000
```

## 4. Trading Strategy Enhancements

### 4.1 Leverage Selection Logic

The system will intelligently select leverage instruments based on signal strength:

```cpp
class LeverageSelector {
public:
    std::string select_optimal_leverage(double signal_probability, 
                                      double signal_confidence,
                                      PositionType current_position);
    
private:
    // High confidence bullish signals -> TQQQ
    // Moderate bullish signals -> QQQ
    // High confidence bearish signals -> SQQQ  
    // Moderate bearish signals -> PSQ
};
```

### 4.2 Signal-to-Leverage Mapping

| Signal Probability | Signal Confidence | Selected Instrument | Rationale |
|-------------------|-------------------|-------------------|-----------|
| > 0.7 | > 0.8 | TQQQ | Maximum bullish leverage |
| > 0.6 | > 0.6 | QQQ | Moderate bullish exposure |
| < 0.3 | > 0.8 | SQQQ | Maximum bearish leverage |
| < 0.4 | > 0.6 | PSQ | Moderate bearish exposure |
| 0.4-0.6 | Any | HOLD | Neutral zone |

### 4.3 Position Sizing with Leverage

Leverage instruments require adjusted position sizing to account for increased volatility:

```cpp
double calculate_leverage_position_size(const std::string& symbol, 
                                       double base_position_size,
                                       double leverage_factor) {
    // Reduce position size for higher leverage to maintain risk parity
    double risk_adjusted_size = base_position_size / leverage_factor;
    return risk_adjusted_size;
}
```

## 5. CLI Command Enhancements

### 5.1 Enhanced Trade Command

The `sentio_cli trade` command will support leverage trading:

```bash
# Basic leverage trading
sentio_cli trade --signals latest --leverage-enabled

# Specific leverage instrument
sentio_cli trade --signals latest --target-symbol TQQQ

# Leverage family trading
sentio_cli trade --signals latest --leverage-family QQQ
```

### 5.2 New CLI Parameters

| Parameter | Description | Default | Example |
|-----------|-------------|---------|---------|
| `--leverage-enabled` | Enable leverage trading | false | `--leverage-enabled` |
| `--target-symbol` | Force specific leverage instrument | auto | `--target-symbol TQQQ` |
| `--leverage-family` | Specify base asset family | QQQ | `--leverage-family QQQ` |
| `--max-leverage` | Maximum leverage factor allowed | 3.0 | `--max-leverage 3.0` |
| `--leverage-decay` | Daily decay rate for theoretical pricing | 0.0001 | `--leverage-decay 0.0001` |

### 5.3 Enhanced Audit Reports

The audit reports will include leverage-specific metrics:

```
📊 LEVERAGE PERFORMANCE SUMMARY
┌─────────────────────┬──────────────┬─────────────────────┬──────────────┐
│ Leverage Instrument │ Total Trades │ Avg Position Size   │ Max Drawdown │
├─────────────────────┼──────────────┼─────────────────────┼──────────────┤
│ TQQQ                │           45 │ $15,000.00          │       8.5%  │
│ QQQ                  │           23 │ $25,000.00          │       3.2%  │
│ SQQQ                 │           12 │ $12,000.00          │       6.8%  │
└─────────────────────┴──────────────┴─────────────────────┴──────────────┘
```

## 6. Risk Management

### 6.1 Leverage Risk Controls

- **Maximum Position Size**: Reduced by leverage factor
- **Drawdown Limits**: Stricter limits for higher leverage instruments
- **Volatility Adjustment**: Position sizing adjusted for instrument volatility
- **Daily Rebalancing**: Account for daily decay in leveraged instruments

### 6.2 Conflict Detection

Real-time conflict detection will prevent invalid position combinations:

```cpp
class LeverageConflictDetector {
public:
    bool would_cause_conflict(const std::string& new_symbol, 
                           const std::vector<std::string>& current_positions);
    
    std::string get_conflict_reason() const;
    
private:
    bool is_conflicting_pair(const std::string& symbol1, const std::string& symbol2);
    PositionType get_symbol_position_type(const std::string& symbol);
};
```

## 7. Implementation Phases

### Phase 1: Data Generation
- [ ] Implement leverage data generation from QQQ base data
- [ ] Create theoretical pricing models for TQQQ, SQQQ, PSQ
- [ ] Generate leverage data files in data/equities/

### Phase 2: Position Management
- [ ] Implement leverage position validator
- [ ] Add conflict detection logic
- [ ] Enhance portfolio manager for leverage instruments

### Phase 3: Trading Logic
- [ ] Implement leverage selector
- [ ] Add leverage-aware position sizing
- [ ] Enhance signal-to-instrument mapping

### Phase 4: CLI Integration
- [ ] Add leverage parameters to trade command
- [ ] Enhance audit reports with leverage metrics
- [ ] Add leverage-specific error messages

### Phase 5: Testing & Validation
- [ ] Comprehensive testing of conflict prevention
- [ ] Backtesting with leverage instruments
- [ ] Performance validation and optimization

## 8. Success Criteria

### 8.1 Functional Requirements
- ✅ All prohibited position combinations are rejected
- ✅ Only allowed position combinations are permitted
- ✅ Leverage data is accurately generated from base QQQ data
- ✅ Position sizing accounts for leverage factors
- ✅ CLI commands support leverage trading parameters

### 8.2 Performance Requirements
- ✅ No conflicting positions ever occur
- ✅ Leverage instruments show appropriate volatility
- ✅ Risk-adjusted returns improve with leverage selection
- ✅ System maintains stability with leverage trading

### 8.3 User Experience Requirements
- ✅ Clear error messages for prohibited combinations
- ✅ Intuitive CLI parameters for leverage control
- ✅ Comprehensive audit reports with leverage metrics
- ✅ Professional reporting maintains quality standards

## 9. Technical Specifications

### 9.1 File Organization

```
data/equities/
├── QQQ_RTH_NH.csv          # Base QQQ data
├── TQQQ_RTH_NH.csv         # Generated 3x long QQQ
├── SQQQ_RTH_NH.csv         # Generated 3x short QQQ
└── PSQ_RTH_NH.csv          # Generated 1x short QQQ
```

### 9.2 Code Structure

```
include/backend/
├── leverage_manager.h       # Leverage position management
├── leverage_data_generator.h # Theoretical data generation
└── leverage_conflict_detector.h # Conflict prevention

src/backend/
├── leverage_manager.cpp
├── leverage_data_generator.cpp
└── leverage_conflict_detector.cpp
```

### 9.3 Configuration

```cpp
struct LeverageConfig {
    bool enable_leverage_trading = true;
    double max_leverage_factor = 3.0;
    double daily_decay_rate = 0.0001;
    double expense_ratio = 0.0095;
    bool use_theoretical_pricing = true;
};
```

## 10. Conclusion

This leverage trading implementation will significantly enhance the Sentio Trading System's profit potential while maintaining strict risk controls. The comprehensive conflict prevention system ensures that only valid position combinations are allowed, while the intelligent leverage selection maximizes returns based on signal strength and market conditions.

The implementation follows the system's existing architecture patterns and maintains the professional-grade reporting standards that have been established throughout the development process.
