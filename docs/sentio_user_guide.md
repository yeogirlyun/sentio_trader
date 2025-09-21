# Sentio Trading System User Guide

**Version 2.0** | **Complete Reference for CLI and Audit Systems**

---

## Table of Contents

1. [Introduction](#introduction)
2. [Quick Start](#quick-start)
3. [Sentio CLI Reference](#sentio-cli-reference)
4. [Sentio Audit Reference](#sentio-audit-reference)
5. [Data Management](#data-management)
6. [Strategy Testing](#strategy-testing)
7. [Performance Analysis](#performance-analysis)
8. [Troubleshooting](#troubleshooting)
9. [Best Practices](#best-practices)
10. [Appendices](#appendices)

---

## Introduction

Sentio is a high-performance algorithmic trading system designed for equity markets. It provides:

- **Strategy Development**: Multiple built-in strategies (IRE, TFA, Momentum, etc.)
- **Backtesting Engine**: Comprehensive historical testing with audit trails
- **Data Management**: Automated data downloading and validation
- **Performance Analysis**: Detailed metrics and robustness testing
- **Live Trading Ready**: Alpaca integration with risk management

### System Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Sentio CLI    │    │  Sentio Audit   │    │   Live Trading  │
│                 │    │                 │    │                 │
│ • Data Download │    │ • Run Analysis  │    │ • Alpaca API    │
│ • Strategy Test │    │ • Trade History │    │ • Risk Mgmt     │
│ • System Probe  │    │ • Performance   │    │ • Position Mgmt │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
                    ┌─────────────────┐
                    │  Core Engine    │
                    │                 │
                    │ • Router/Sizer  │
                    │ • Signal Engine │
                    │ • Audit System  │
                    │ • Feature Eng   │
                    └─────────────────┘
```

---

## Quick Start

### Installation & Setup

1. **Build the system:**
   ```bash
   make build/sentio_cli build/sentio_audit
   ```

2. **Set up environment:**
   ```bash
   cp config.env.example config.env
   # Edit config.env with your settings
   source config.env
   ```

3. **Create shortcuts (optional):**
   ```bash
   # Use the provided shortcuts
   ./sencli --help      # Shortcut for ./build/sentio_cli
   ./saudit --help      # Shortcut for ./build/sentio_audit
   ```

### First Steps

1. **Check system status:**
   ```bash
   sentio_cli probe
   ```

2. **Download data:**
   ```bash
   sentio_cli download QQQ --period 3y --family
   ```

3. **Test a strategy:**
   ```bash
   sentio_cli strattest ire QQQ --comprehensive
   ```

4. **Analyze results:**
   ```bash
   sentio_audit summarize
   ```

---

## Sentio CLI Reference

The Sentio CLI provides four main command categories:

### Command Overview

| **Category** | **Commands** | **Purpose** |
|--------------|--------------|-------------|
| **Strategy Testing** | `strattest` | Unified strategy robustness testing |
| **Data Management** | `download`, `probe` | Data acquisition and validation |
| **Development** | `audit-validate` | Strategy validation and debugging |

### Global Options

All commands support these global options:

- `--help, -h`: Show command-specific help
- `--verbose, -v`: Enable verbose output  
- `--output <format>`: Output format (console|json|csv)

---

## Strategy Testing Commands

### `strattest` - Unified Strategy Testing

**Purpose**: Comprehensive strategy robustness testing with multiple simulation modes.

**Usage:**
```bash
sentio_cli strattest <strategy> <symbol> [options]
```

**Required Arguments:**
- `<strategy>`: Strategy name (ire, tfa, momentum, etc.)
- `<symbol>`: Trading symbol (QQQ, SPY, etc.)

**Options:**

| **Option** | **Description** | **Default** |
|------------|-----------------|-------------|
| `--mode <mode>` | Simulation mode: monte-carlo\|historical\|ai-regime\|hybrid | hybrid |
| `--simulations <n>` | Number of simulations | 1 |
| `--duration <period>` | Test duration: 1h, 4h, 1d, 5d, 1w, 2w, 4w, 1m | 4w |
| `--historical-data <file>` | Historical data file (auto-detect if not specified) | - |
| `--regime <regime>` | Market regime: normal\|volatile\|trending\|bear\|bull | normal |
| `--stress-test` | Enable stress testing scenarios | false |
| `--regime-switching` | Test across multiple market regimes | false |
| `--liquidity-stress` | Simulate low liquidity conditions | false |
| `--alpaca-fees` | Use Alpaca fee structure | true |
| `--alpaca-limits` | Apply Alpaca position/order limits | false |
| `--confidence <level>` | Confidence level: 90\|95\|99 | 95 |
| `--save-results <file>` | Save detailed results to file | - |
| `--benchmark <symbol>` | Benchmark symbol | SPY |
| `--quick` | Quick mode: fewer simulations, faster execution | false |
| `--comprehensive` | Comprehensive mode: extensive testing scenarios | false |
| `--params <json>` | Strategy parameters as JSON string | '{}' |

**Examples:**

```bash
# Basic strategy test
sentio_cli strattest ire QQQ

# Comprehensive robustness test
sentio_cli strattest ire QQQ --comprehensive --stress-test

# Monte Carlo simulation with custom parameters
sentio_cli strattest momentum QQQ --mode monte-carlo --simulations 100

# AI regime testing
sentio_cli strattest ire SPY --mode ai-regime --regime volatile

# Quick test for development
sentio_cli strattest tfa QQQ --quick --duration 1d

# Custom strategy parameters
sentio_cli strattest ire QQQ --params '{"buy_hi":0.8,"sell_lo":0.2}'
```

**Output:**
The command generates a comprehensive robustness report including:
- Core performance metrics (MPR, Sharpe, drawdown)
- Confidence intervals for all metrics
- Risk assessment and warnings
- Alpaca-specific trading analysis
- Deployment readiness score

---

## Data Management Commands

### `download` - Data Acquisition

**Purpose**: Download historical market data from Polygon.io API.

**Usage:**
```bash
sentio_cli download <symbol> [options]
```

**Required Arguments:**
- `<symbol>`: Trading symbol to download

**Options:**

| **Option** | **Description** | **Default** |
|------------|-----------------|-------------|
| `--period <period>` | Time period: 1y, 6m, 3m, 1m, 2w, 5d | 3y |
| `--timespan <span>` | Data resolution: day\|hour\|minute | minute |
| `--holidays` | Include market holidays | false (exclude) |
| `--output <dir>` | Output directory | data/equities/ |
| `--family` | Download symbol family (QQQ → QQQ,TQQQ,SQQQ) | false |
| `--force` | Overwrite existing files | false |

**Examples:**

```bash
# Download 3 years of QQQ minute data (default)
sentio_cli download QQQ

# Download QQQ family (QQQ, TQQQ, SQQQ)
sentio_cli download QQQ --family --period 3y

# Download daily data for backtesting
sentio_cli download SPY --period 1y --timespan day

# Download with holidays included
sentio_cli download QQQ --period 6m --holidays
```

### `probe` - System Status

**Purpose**: Check data availability and system health.

**Usage:**
```bash
sentio_cli probe
```

**Output:**
- Available strategies (11 total)
- Data availability for key symbols (QQQ, SPY, AAPL, MSFT, TSLA)
- Daily and minute data status
- System readiness assessment

**Example:**
```bash
sentio_cli probe
```

**Sample Output:**
```
=== SENTIO SYSTEM PROBE ===

📊 Available Strategies (11 total):
=====================
  • ire
  • tfa
  • momentum
  • sma_cross
  • bollinger_squeeze_breakout
  ...

📈 Data Availability Check:
==========================
Symbol: QQQ
  📅 Daily:  ✅ Available (2021-01-01 to 2024-09-13)
  ⏰ Minute: ✅ Available (2021-01-01 to 2024-09-13)

📋 Summary:
===========
  🎉 All data is properly aligned and ready for strategy testing!
  📋 Ready to run: ./build/sentio_cli strattest ire QQQ --comprehensive
```

---

## Development Commands

### `audit-validate` - Strategy Validation

**Purpose**: Validate that all strategies work correctly with the audit system.

**Usage:**
```bash
sentio_cli audit-validate
```

**Output:**
- Validation results for all registered strategies
- Success/failure status with error details
- Summary of audit compatibility

**Example:**
```bash
sentio_cli audit-validate
```

**Sample Output:**
```
🔍 **STRATEGY-AGNOSTIC AUDIT VALIDATION**
Validating that all registered strategies work with the audit system...

📊 **AUDIT VALIDATION RESULTS**
==================================================
✅ ire - PASSED
✅ tfa - PASSED
✅ momentum - PASSED
❌ experimental_strategy - FAILED: Signal generation timeout

==================================================
📈 Summary: 10 passed, 1 failed
⚠️  Some strategies need fixes before audit compatibility
```

---

## Sentio Audit Reference

The Sentio Audit system provides comprehensive analysis of trading runs with a canonical interface.

### Command Overview

| **Command** | **Purpose** | **Key Features** |
|-------------|-------------|------------------|
| `list` | Show all audit runs | Run history, filtering |
| `summarize` | Performance summary | Metrics, returns, risk |
| `position-history` | Account transactions | Progressive disclosure |
| `trade-flow` | Trade execution analysis | Buy/sell/hold filtering |
| `signal-flow` | Signal generation analysis | Drop reasons, filtering |

### Global Options

All audit commands support:
- `--help, -h`: Show command-specific help
- `--run-id <id>`: Specify run ID (default: latest)

---

## Audit Commands

### `list` - Run History

**Purpose**: Display all audit runs with filtering capabilities.

**Usage:**
```bash
sentio_audit list [options]
```

**Options:**

| **Option** | **Description** | **Default** |
|------------|-----------------|-------------|
| `--limit <n>` | Maximum runs to show | 20 |
| `--strategy <name>` | Filter by strategy name | all |
| `--days <n>` | Show runs from last N days | all |

**Examples:**

```bash
# Show recent runs
sentio_audit list

# Show last 10 runs
sentio_audit list --limit 10

# Show only IRE strategy runs
sentio_audit list --strategy ire

# Show runs from last 7 days
sentio_audit list --days 7
```

**Sample Output:**
```
📊 AUDIT RUNS
=============

RUN_ID  STRATEGY     TEST_TYPE    STARTED_AT           DURATION  TRADES  STATUS
------  -----------  -----------  -------------------  --------  ------  ------
123456  ire          strattest    2024-09-13 14:30:15  45.2s     156     ✅
123455  tfa          strattest    2024-09-13 14:15:22  38.7s     89      ✅
123454  momentum     strattest    2024-09-13 14:00:10  52.1s     203     ✅

📈 Total: 3 runs | ✅ 3 successful | ❌ 0 failed
```

### `summarize` - Performance Summary

**Purpose**: Show comprehensive performance metrics for a trading run.

**Usage:**
```bash
sentio_audit summarize [options]
```

**Options:**

| **Option** | **Description** | **Default** |
|------------|-----------------|-------------|
| `--run-id <id>` | Specific run ID | latest |
| `--detailed` | Show detailed breakdown | false |

**Examples:**

```bash
# Summarize latest run
sentio_audit summarize

# Summarize specific run
sentio_audit summarize --run-id 123456

# Detailed summary
sentio_audit summarize --detailed
```

**Sample Output:**
```
📊 PERFORMANCE SUMMARY
======================
🎯 Strategy: ire | Test: strattest | Run: 123456
📅 Date: 2024-09-13 14:30:15 (Local Time)

💰 CORE METRICS
===============
Monthly Projected Return:  8.45%
Annual Projected Return:   168.2%
Sharpe Ratio:             2.34
Maximum Drawdown:         -3.2%
Total Return:             12.8%

📈 TRADING ACTIVITY
===================
Total Trades:             156
Win Rate:                 68.5%
Avg Trade Duration:       4.2 hours
Daily Trade Rate:         12.3 trades/day

💵 FINANCIAL SUMMARY
====================
Starting Capital:         $100,000
Ending Balance:           $112,800
Total P&L:               $12,800
Transaction Fees:         $234.50

🎯 DEPLOYMENT READINESS: ✅ READY
Confidence Level: HIGH | Risk Level: MODERATE
```

### `position-history` - Account Transactions

**Purpose**: Show detailed account position changes over time with progressive disclosure.

**Usage:**
```bash
sentio_audit position-history [options]
```

**Options:**

| **Option** | **Description** | **Default** |
|------------|-----------------|-------------|
| `--run-id <id>` | Specific run ID | latest |
| `--max [n]` | Max transactions to show (no number = all) | 20 |
| `--symbol <sym>` | Filter by symbol | all |

**Examples:**

```bash
# Show recent position changes
sentio_audit position-history

# Show all transactions
sentio_audit position-history --max

# Show last 50 transactions
sentio_audit position-history --max 50

# Show only QQQ transactions
sentio_audit position-history --symbol QQQ
```

**Sample Output:**
```
📊 POSITION HISTORY
===================
🎯 Strategy: ire | Test: strattest | Run: 123456
📅 Date: 2024-09-13 14:30:15 (Local Time)

💼 EXECUTIVE SUMMARY
====================
Total Transactions: 156
Symbols Traded: QQQ, TQQQ, SQQQ
Position Changes: 89 entries, 67 exits
Net P&L: $12,800 (+12.8%)

📈 PERFORMANCE BREAKDOWN
========================
Winning Trades: 107 (68.5%) | Avg: +$185.20
Losing Trades: 49 (31.5%) | Avg: -$89.40
Largest Win: +$1,245.80 | Largest Loss: -$234.60

📋 RECENT TRANSACTIONS (Last 20)
=================================

TIME         SYMBOL  ACTION      QTY    PRICE     POSITION   REALIZED_PNL  EQUITY
-----------  ------  ----------  -----  --------  ---------  ------------  --------
14:29:45     QQQ     SELL        -100   $387.45   0          +$1,245.80    $112,800
14:25:30     TQQQ    BUY         +50    $45.20    50         $0.00         $111,554
14:20:15     QQQ     BUY         +100   $385.20   100        $0.00         $111,294
...

💡 Use --max to see all transactions or --symbol <SYM> to filter by symbol
```

### `trade-flow` - Trade Execution Analysis

**Purpose**: Analyze trade execution patterns with filtering by action type.

**Usage:**
```bash
sentio_audit trade-flow [options]
```

**Options:**

| **Option** | **Description** | **Default** |
|------------|-----------------|-------------|
| `--run-id <id>` | Specific run ID | latest |
| `--max [n]` | Max events to show (no number = all) | 20 |
| `--buy` | Show only buy trades | false |
| `--sell` | Show only sell trades | false |
| `--hold` | Show only hold decisions | false |

**Examples:**

```bash
# Show recent trade flow
sentio_audit trade-flow

# Show only buy trades
sentio_audit trade-flow --buy

# Show buy and sell trades (exclude holds)
sentio_audit trade-flow --buy --sell

# Show all trade events
sentio_audit trade-flow --max
```

**Sample Output:**
```
📊 TRADE FLOW ANALYSIS
======================
🎯 Strategy: ire | Test: strattest | Run: 123456
📅 Date: 2024-09-13 14:30:15 (Local Time)

💼 EXECUTIVE SUMMARY
====================
Total Events: 1,247
Buy Orders: 89 | Sell Orders: 67 | Hold Decisions: 1,091
Execution Rate: 98.5% | Average Fill Time: 0.2s

📈 PERFORMANCE BREAKDOWN
========================
Buy Success: 87/89 (97.8%) | Avg Size: 125 shares
Sell Success: 66/67 (98.5%) | Avg Size: 118 shares
Hold Accuracy: 94.2% (correct no-trade decisions)

📋 RECENT EVENTS (Last 20)
===========================

TIME         ACTION  SYMBOL  QTY    PRICE     STATUS    FILL_TIME  REASON
-----------  ------  ------  -----  --------  --------  ---------  ------------------
14:29:45     SELL    QQQ     100    $387.45   FILLED    0.15s      Strong sell signal
14:29:44     HOLD    TQQQ    -      -         -         -          Below confidence
14:29:43     HOLD    SQQQ    -      -         -         -          Risk limit reached

14:25:30     BUY     TQQQ    50     $45.20    FILLED    0.22s      Momentum breakout
14:25:29     HOLD    QQQ     -      -         -         -          Position size limit
...

💡 Use --buy, --sell, --hold to filter by action type
```

### `signal-flow` - Signal Generation Analysis

**Purpose**: Analyze signal generation and filtering with detailed drop reasons.

**Usage:**
```bash
sentio_audit signal-flow [options]
```

**Options:**

| **Option** | **Description** | **Default** |
|------------|-----------------|-------------|
| `--run-id <id>` | Specific run ID | latest |
| `--max [n]` | Max events to show (no number = all) | 20 |
| `--buy` | Show only buy signals | false |
| `--sell` | Show only sell signals | false |
| `--hold` | Show only hold signals | false |

**Examples:**

```bash
# Show recent signal flow
sentio_audit signal-flow

# Show only buy signals
sentio_audit signal-flow --buy

# Show all signal events
sentio_audit signal-flow --max

# Show sell signals only
sentio_audit signal-flow --sell
```

**Sample Output:**
```
📊 SIGNAL FLOW ANALYSIS
=======================
🎯 Strategy: ire | Test: strattest | Run: 123456
📅 Date: 2024-09-13 14:30:15 (Local Time)

💼 EXECUTIVE SUMMARY
====================
Total Signals: 2,156
Buy Signals: 234 | Sell Signals: 189 | Hold Signals: 1,733
Signal Quality: 87.3% | False Positive Rate: 12.7%

📈 PERFORMANCE BREAKDOWN
========================
Strong Signals: 423 (19.6%) | Conversion Rate: 95.2%
Weak Signals: 1,733 (80.4%) | Conversion Rate: 8.1%
Filtered Signals: 267 (11.0%) | Top Reason: Risk limits

📋 RECENT SIGNALS (Last 20)
============================

TIME         SIGNAL      SYMBOL  STRENGTH  STATUS     DROP_REASON
-----------  ----------  ------  --------  ---------  ---------------------------
14:29:45     STRONG_SELL QQQ     0.89      EXECUTED   -

14:29:44     HOLD        TQQQ    0.45      DROPPED    Below confidence threshold

14:29:43     SELL        SQQQ    0.72      DROPPED    Risk limit: max daily trades

14:25:30     STRONG_BUY  TQQQ    0.91      EXECUTED   -

14:25:29     BUY         QQQ     0.68      DROPPED    Position size limit reached

14:20:15     STRONG_BUY  QQQ     0.88      EXECUTED   -

14:20:14     HOLD        SQQQ    0.32      DROPPED    Market hours restriction

...

💡 Use --buy, --sell, --hold to filter by signal type
📋 Empty lines separate signal events for easy scanning
```

---

## Data Management

### Data Directory Structure

```
data/
├── equities/
│   ├── QQQ_NH.csv              # QQQ minute data, no holidays, all trading hours
│   ├── QQQ_daily.csv           # QQQ daily data
│   ├── TQQQ_NH.csv             # TQQQ minute data, no holidays, all trading hours
│   ├── SQQQ_NH.csv             # SQQQ minute data, no holidays, all trading hours
│   └── SPY_NH.csv              # SPY minute data, no holidays, all trading hours
└── cache/
    ├── QQQ_features.bin        # Cached feature data
    └── QQQ_aligned.bin         # Aligned bar data
```

### Data Formats

**CSV Format:**
```csv
timestamp,open,high,low,close,volume
2024-01-02 09:30:00,385.20,387.45,384.80,386.90,1250000
2024-01-02 09:31:00,386.90,388.10,386.50,387.80,980000
```

**Binary Cache Format:**
- Optimized for fast loading
- Automatically generated from CSV
- Used when fresher than source CSV

### Data Validation

The system automatically validates:
- **Timestamp Alignment**: All symbols must have matching timestamps
- **Data Completeness**: No gaps in trading hours
- **Price Consistency**: OHLC relationships
- **Volume Sanity**: Reasonable volume ranges

---

## Strategy Testing

### Available Strategies

| **Strategy** | **Type** | **Description** | **Parameters** |
|--------------|----------|-----------------|----------------|
| **ire** | Rule-based | Integrated Rule Ensemble | buy_hi, sell_lo |
| **tfa** | ML-based | Temporal Feature Analysis | model_path |
| **momentum** | Technical | Momentum with volume | lookback, threshold |
| **sma_cross** | Technical | Simple Moving Average crossover | fast_period, slow_period |
| **bollinger_squeeze** | Technical | Bollinger Band squeeze breakout | period, std_dev |

### Testing Modes

#### Monte Carlo Mode
- **Purpose**: Test strategy robustness across synthetic market conditions
- **Method**: Generate random price movements with realistic volatility
- **Best For**: Stress testing, risk assessment

```bash
sentio_cli strattest ire QQQ --mode monte-carlo --simulations 100
```

#### Historical Mode
- **Purpose**: Test on actual historical patterns
- **Method**: Use historical data with synthetic continuation
- **Best For**: Backtesting, performance validation

```bash
sentio_cli strattest ire QQQ --mode historical --duration 1m
```

#### AI Regime Mode
- **Purpose**: Test across different market regimes
- **Method**: AI-generated market scenarios
- **Best For**: Regime analysis, adaptability testing

```bash
sentio_cli strattest ire QQQ --mode ai-regime --regime volatile
```

#### Hybrid Mode (Default)
- **Purpose**: Comprehensive testing combining all methods
- **Method**: Monte Carlo + Historical + AI scenarios
- **Best For**: Complete robustness analysis

```bash
sentio_cli strattest ire QQQ --mode hybrid --comprehensive
```

### Performance Metrics

#### Core Metrics
- **Monthly Projected Return (MPR)**: Annualized return projected to monthly
- **Sharpe Ratio**: Risk-adjusted return measure
- **Maximum Drawdown**: Largest peak-to-trough decline
- **Win Rate**: Percentage of profitable trades

#### Robustness Metrics
- **Consistency Score**: Performance stability across simulations
- **Regime Adaptability**: Performance across market conditions
- **Stress Resilience**: Performance under adverse conditions
- **Liquidity Tolerance**: Performance with liquidity constraints

#### Alpaca-Specific Metrics
- **Estimated Monthly Fees**: Transaction costs
- **Capital Efficiency**: Return per dollar deployed
- **Average Daily Trades**: Trading frequency
- **Position Turnover**: Portfolio turnover rate

---

## Performance Analysis

### Interpreting Results

#### Deployment Readiness Scoring

| **Score** | **Status** | **Meaning** |
|-----------|------------|-------------|
| 90-100 | ✅ **READY** | High confidence, deploy immediately |
| 70-89 | ⚠️ **CAUTION** | Good performance, monitor closely |
| 50-69 | ❌ **NOT READY** | Needs improvement before deployment |
| <50 | 🚫 **FAILED** | Significant issues, do not deploy |

#### Risk Assessment Levels

| **Level** | **Description** | **Action** |
|-----------|-----------------|------------|
| **LOW** | Conservative strategy, stable returns | Standard deployment |
| **MEDIUM** | Moderate risk, good risk/reward | Deploy with monitoring |
| **HIGH** | Aggressive strategy, higher volatility | Deploy with tight risk controls |
| **EXTREME** | Very high risk, potential for large losses | Do not deploy |

#### Confidence Intervals

All metrics include confidence intervals at 90%, 95%, or 99% levels:

```
Monthly Projected Return: 8.45% [7.2% - 9.8%] (95% CI)
```

This means we're 95% confident the true MPR is between 7.2% and 9.8%.

### Sample Analysis Workflow

1. **Run Comprehensive Test:**
   ```bash
   sentio_cli strattest ire QQQ --comprehensive --stress-test
   ```

2. **Analyze Results:**
   ```bash
   sentio_audit summarize --detailed
   ```

3. **Review Trade Quality:**
   ```bash
   sentio_audit trade-flow --buy --sell
   ```

4. **Check Signal Quality:**
   ```bash
   sentio_audit signal-flow --max 100
   ```

5. **Examine Position Management:**
   ```bash
   sentio_audit position-history --max
   ```

---

## Troubleshooting

### Common Issues

#### Data Problems

**Issue**: "FATAL: No data loaded for symbol"
```bash
# Solution: Download data first
sentio_cli download QQQ --period 3y
```

**Issue**: "FATAL: Data alignment check failed"
```bash
# Solution: Re-download all family symbols
sentio_cli download QQQ --family --force
```

#### Strategy Problems

**Issue**: "Invalid strategy name"
```bash
# Solution: Check available strategies
sentio_cli probe
```

**Issue**: "Strategy validation failed"
```bash
# Solution: Run audit validation
sentio_cli audit-validate
```

#### Performance Problems

**Issue**: Strategy shows poor performance
1. Check signal quality: `sentio_audit signal-flow`
2. Analyze trade execution: `sentio_audit trade-flow`
3. Review parameters: Try different strategy parameters
4. Test different time periods: Use `--duration` option

#### System Problems

**Issue**: "No such table: audit_runs"
```bash
# Solution: Initialize audit database
mkdir -p audit
# Run any strategy test to create database
sentio_cli strattest ire QQQ --quick
```

### Debug Mode

Enable verbose output for troubleshooting:
```bash
sentio_cli strattest ire QQQ --verbose
sentio_audit summarize --verbose
```

### Log Files

Check system logs in:
```
logs/
├── app.log          # General application logs
├── errors.log       # Error messages only
└── debug.log        # Detailed debug information
```

---

## Best Practices

### Strategy Development

1. **Start Small**: Begin with quick tests before comprehensive analysis
   ```bash
   sentio_cli strattest new_strategy QQQ --quick --duration 1d
   ```

2. **Validate First**: Always run audit validation for new strategies
   ```bash
   sentio_cli audit-validate
   ```

3. **Progressive Testing**: Gradually increase test complexity
   ```bash
   # Step 1: Quick test
   sentio_cli strattest ire QQQ --quick
   
   # Step 2: Standard test  
   sentio_cli strattest ire QQQ
   
   # Step 3: Comprehensive test
   sentio_cli strattest ire QQQ --comprehensive --stress-test
   ```

### Data Management

1. **Regular Updates**: Update data regularly for accurate backtests
   ```bash
   # Weekly data update
   sentio_cli download QQQ --family --period 1w
   ```

2. **Family Downloads**: Always download symbol families together
   ```bash
   sentio_cli download QQQ --family  # Gets QQQ, TQQQ, SQQQ
   ```

3. **Data Validation**: Check data quality before important tests
   ```bash
   sentio_cli probe
   ```

### Performance Analysis

1. **Multiple Time Frames**: Test across different durations
   ```bash
   sentio_cli strattest ire QQQ --duration 1w
   sentio_cli strattest ire QQQ --duration 1m
   ```

2. **Confidence Levels**: Use appropriate confidence levels
   - 90% for quick assessments
   - 95% for standard analysis (default)
   - 99% for critical deployment decisions

3. **Stress Testing**: Always include stress tests for live deployment
   ```bash
   sentio_cli strattest ire QQQ --stress-test --regime-switching
   ```

### Audit Analysis

1. **Start with Summary**: Always begin with performance summary
   ```bash
   sentio_audit summarize
   ```

2. **Drill Down**: Use specific commands for detailed analysis
   ```bash
   sentio_audit trade-flow --buy --sell  # Focus on executed trades
   sentio_audit signal-flow --max 100    # Examine signal quality
   ```

3. **Filter Appropriately**: Use filters to focus on relevant data
   ```bash
   sentio_audit position-history --symbol QQQ  # Single symbol analysis
   ```

---

## Appendices

### Appendix A: Strategy Parameters

#### IRE Strategy
```json
{
  "buy_hi": 0.80,      // Buy threshold (0.0-1.0)
  "sell_lo": 0.20      // Sell threshold (0.0-1.0)
}
```

#### TFA Strategy
```json
{
  "model_path": "artifacts/TFA/v1/model.pt",
  "confidence_threshold": 0.75
}
```

#### Momentum Strategy
```json
{
  "lookback": 20,      // Lookback period in bars
  "threshold": 0.02    // Momentum threshold
}
```

### Appendix B: Environment Variables

```bash
# config.env
SENTIO_DATA_SUFFIX="_RTH_NH"
SENTIO_DB_PATH="audit/sentio_audit.sqlite3"
POLYGON_API_KEY="your_api_key_here"
ALPACA_API_KEY="your_alpaca_key"
ALPACA_SECRET_KEY="your_alpaca_secret"
```

### Appendix C: File Formats

#### Audit Database Schema
```sql
-- audit_runs table
CREATE TABLE audit_runs (
    run_id TEXT PRIMARY KEY,
    strategy_name TEXT,
    started_at TIMESTAMP,
    note TEXT
);

-- audit_events table  
CREATE TABLE audit_events (
    id INTEGER PRIMARY KEY,
    run_id TEXT,
    event_type TEXT,
    timestamp INTEGER,
    data TEXT
);
```

### Appendix D: API Integration

#### Alpaca Integration
```cpp
// Example configuration
AlpacaConfig config;
config.api_key = getenv("ALPACA_API_KEY");
config.secret_key = getenv("ALPACA_SECRET_KEY");
config.base_url = "https://paper-api.alpaca.markets";  // Paper trading
```

### Appendix E: Performance Benchmarks

#### Typical Performance Ranges

| **Metric** | **Conservative** | **Moderate** | **Aggressive** |
|------------|------------------|--------------|----------------|
| **MPR** | 2-5% | 5-12% | 12-25% |
| **Sharpe** | 1.0-1.5 | 1.5-2.5 | 2.5+ |
| **Max DD** | <5% | 5-15% | 15-30% |
| **Win Rate** | 60-70% | 55-65% | 45-60% |

---

## Support and Resources

### Getting Help

1. **Command Help**: Use `--help` with any command
2. **System Status**: Run `sentio_cli probe` for health check
3. **Validation**: Run `sentio_cli audit-validate` for strategy issues
4. **Logs**: Check `logs/` directory for detailed error information

### Version Information

- **Sentio Version**: 2.0
- **CLI Version**: Canonical Interface
- **Audit Version**: Unified Reporting
- **Last Updated**: September 2024

---

*This guide covers the complete Sentio trading system. For additional support or feature requests, consult the development team.*
