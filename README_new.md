# Sentio Trading System

A high-performance C++ algorithmic trading system for strategy development, backtesting, and portfolio management.

## 🚀 Quick Start

### Prerequisites

- **C++ Compiler**: GCC 9+ or Clang 10+ with C++17 support
- **CMake**: Version 3.15 or higher
- **Operating System**: Linux, macOS, or Windows with WSL

### Build Instructions

```bash
# Clone the repository
git clone <repository-url>
cd sentio_trader

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Verify build
./sentio_cli --help
```

## 📊 Usage Guide

The Sentio system provides a unified CLI interface for all trading operations:

### 1. Strategy Testing (`strattest`)

Generate trading signals from market data using built-in strategies:

```bash
# Test Sigor strategy with default parameters
./sentio_cli strattest --dataset data/equities/QQQ_RTH_NH.csv --strategy sigor --blocks 20

# Custom output location and format
./sentio_cli strattest \
  --dataset data/equities/QQQ_RTH_NH.csv \
  --strategy sigor \
  --blocks 50 \
  --out data/signals/my_signals.jsonl \
  --format jsonl
```

**Parameters:**
- `--dataset`: Path to market data CSV file
- `--strategy`: Strategy name (`sigor` currently supported)
- `--blocks`: Number of data blocks to process (controls dataset size)
- `--out`: Output file path (auto-generated if not specified)
- `--format`: Output format (`jsonl` or `csv`)

**Output:**
```
Exported 292,365 signals to data/signals/sigor_signals_AUTO.jsonl
```

### 2. Trade Execution (`trade`)

Convert signals into executed trades with portfolio management:

```bash
# Execute trades from latest signals
./sentio_cli trade --signals latest --capital 100000 --buy 0.6 --sell 0.4

# Execute specific signal file with custom parameters
./sentio_cli trade \
  --signals data/signals/my_signals.jsonl \
  --capital 250000 \
  --buy 0.65 \
  --sell 0.35 \
  --blocks 100
```

**Parameters:**
- `--signals`: Signal file path or `latest` for most recent
- `--capital`: Starting capital amount
- `--buy`: Buy threshold (signal probability required for buy orders)
- `--sell`: Sell threshold (signal probability required for sell orders)
- `--blocks`: Number of signal blocks to process

**Output:**
```
Trading last 9000 signals starting at index 283365
Completed. Total signals: 9000, executed trades: 2789
Run ID: 29791548  Trades: data/trades/29791548_trades.jsonl
Executed trades exported.
```

### 3. Portfolio Audit (`audit`)

Analyze trading performance with professional reports:

```bash
# Generate position history report (default 20 trades)
./sentio_cli audit position-history

# Show more trades with custom limit
./sentio_cli audit position-history --max 50

# Analyze specific trading run
./sentio_cli audit position-history --run 29791548 --max 30

# Quick performance summary
./sentio_cli audit summarize --run 29791548
```

**Sample Output:**
```
📊 ACCOUNT PERFORMANCE SUMMARY
┌───────────────────────────────────────────────────────────────────────────────────────────────────┐
│ Starting Capital   │ $ 100000.00 │ Current Equity     │ $ 100022.96 │ Total Return│    +0.02%    │
│ Total Trades       │         376 │ Realized P&L       │    $ +22.96 │ Unrealized  │     $ +0.00 │
│ Cash Balance       │ $ 100022.96 │ Position Value     │      $ 0.00 │ Open Pos.   │        1   │
└───────────────────────────────────────────────────────────────────────────────────────────────────┘

📈 TRADE HISTORY (Last 10 of 376 trades)
┌─────────────────┬────────┬────────┬──────────┬──────────┬───────────────┬──────────────┬─────────────────┬──────────────────────┬─────────────────┐
│ Date/Time       │ Symbol │ Action │ Quantity │ Price    │  Trade Value  │  Realized P&L│  Equity After   │ Positions            │ Unrealized P&L  │
├─────────────────┼────────┼────────┼──────────┼──────────┼───────────────┼──────────────┼─────────────────┼──────────────────────┼─────────────────┤
│ 09/13 00:54:00  │ QQQ    │ 🟢BUY  │   26.393 │ $ 269.64 │    $ +7116.68 │      $ +0.00 │     $ +99893.81 │ QQQ:1                │         $ +0.00 │
│ 09/13 01:38:00  │ QQQ    │ 🔴SELL │  -26.393 │ $ 268.63 │    $ +7089.90 │     $ -26.79 │     $ +99867.02 │ QQQ:0                │         $ +0.00 │
└─────────────────┴────────┴────────┴──────────┴──────────┴───────────────┴──────────────┴─────────────────┴──────────────────────┴─────────────────┘
```

## 🔧 Configuration

### Strategy Parameters

Strategies can be configured through command-line parameters:

```bash
# Sigor strategy with custom thresholds
./sentio_cli strattest \
  --dataset data/equities/QQQ_RTH_NH.csv \
  --strategy sigor \
  --config "buy_threshold=0.65,sell_threshold=0.35,warmup_bars=300"
```

### Data Formats

**Market Data CSV Format:**
```csv
symbol,timestamp_ms,open,high,low,close,volume
QQQ,1663248600000,294.07,294.10,293.60,294.00,755864.0
```

**Alternative QQQ Format:**
```csv
ts_utc,ts_nyt_epoch,open,high,low,close,volume
2022-09-15T09:30:00-04:00,1663248600,294.07,294.10,293.60,294.00,755864.0
```

## 📁 File Organization

The system automatically organizes all output files in the `data/` directory:

```
data/
├── equities/          # Market data files (input)
│   ├── QQQ_RTH_NH.csv
│   └── SQQQ_RTH_NH.csv
├── signals/           # Generated trading signals
│   └── sigor_signals_AUTO.jsonl
├── trades/            # Executed trades and portfolio states
│   ├── 29791548_trades.jsonl
│   └── 87314551_trades.jsonl
└── audit/             # Performance analysis databases
    └── sentio_audit.sqlite3
```

## 🧪 Testing & Validation

### Integrity Check

Run comprehensive system validation:

```bash
# Full end-to-end integrity check
python3 tools/sentio_integrity_check.py

# File organization validation
python3 tools/check_file_organization.py
```

### Performance Benchmarks

Typical performance on modern hardware:
- **Signal Generation**: 290K+ signals in ~1.5 seconds
- **Trade Execution**: 2,800+ trades in ~0.3 seconds
- **Portfolio Audit**: Real-time report generation

## 🎯 Example Workflows

### Complete Trading Pipeline

```bash
# 1. Generate signals from market data
./sentio_cli strattest --dataset data/equities/QQQ_RTH_NH.csv --strategy sigor --blocks 20

# 2. Execute trades based on signals
./sentio_cli trade --signals latest --capital 100000 --buy 0.6 --sell 0.4

# 3. Analyze performance
./sentio_cli audit position-history --max 25

# 4. Get summary statistics
./sentio_cli audit summarize
```

### Strategy Development Workflow

```bash
# Test strategy with different parameters
./sentio_cli strattest --strategy sigor --blocks 10  # Quick test
./sentio_cli trade --signals latest --capital 50000 --buy 0.7 --sell 0.3
./sentio_cli audit position-history --max 10

# Iterate with different thresholds
./sentio_cli trade --signals latest --capital 50000 --buy 0.55 --sell 0.45
./sentio_cli audit summarize

# Full backtest
./sentio_cli strattest --strategy sigor --blocks 50  # Full dataset
./sentio_cli trade --signals latest --capital 100000 --buy 0.6 --sell 0.4
./sentio_cli audit position-history --max 50
```

## 🚨 Error Handling

The system includes comprehensive error handling:

- **File Validation**: Automatic detection of missing or corrupted data files
- **Financial Safeguards**: Prevention of negative cash balances
- **Data Integrity**: Validation of all input formats and calculations
- **Graceful Degradation**: System continues operating with partial failures

## 📚 Additional Resources

- **Architecture**: See `docs/architecture.md` for detailed system design
- **API Documentation**: Header files contain comprehensive inline documentation
- **Examples**: Check `tools/` directory for additional utilities and examples

## 🤝 Contributing

1. Follow the existing code style and commenting conventions
2. Add comprehensive tests for new features
3. Update documentation for any API changes
4. Run integrity checks before submitting changes

## 📄 License

[License information to be added]
