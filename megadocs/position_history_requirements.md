Title: Position History & Brokerage-Grade Account Statement Requirements

1. Objective
- Define a standardized, professional account statement and position history format aligned with brokerage statements.
- Ensure CLI audit views and exports match this format by default and support deterministic parsing.

2. Scope
- Applies to CLI output (terminal tables), file exports (CSV/JSONL), and any UI adapters.
- Covers positions, cash, trades, P&L, equity, and summary metrics.

3. Position History Table (CLI)
- Columns (left to right):
  - time: MM/DD HH:MM:SS (UTC by default; configurable to local)
  - symbol: Ticker (e.g., QQQ)
  - action: 🟢BUY | 🔴SELL
  - qty: signed decimal with 3 places, right-aligned, showpos
  - price: currency $ 0.00, right-aligned
  - trade_val: currency, right-aligned, showpos
  - r_pnl: realized PnL delta, currency, right-aligned, showpos
  - equity: post-trade total equity, currency, right-aligned, showpos
  - positions: compact summary "SYM:COUNT" (no ISO timestamps)
  - d_equity: equity change vs previous row, currency, right-aligned, showpos (green/red color in CLI)
- Alignment: fixed widths, pipe-delimited, ASCII fallback separators.
- Pagination: default tail of 20 rows; --max N to override.
- Color: action green/red; d_equity green for >=0, red for <0. ANSI disabled with --no-color.

4. Trade Book JSONL (internal)
- Fields: run_id, timestamp_ms, bar_index, symbol, action, quantity, price, trade_value, fees, cash_before, equity_before, cash_after, equity_after, positions_after, realized_pnl_delta, unrealized_after, positions_summary.
- Contracts:
  - symbol must be a valid ticker (never a timestamp). If unavailable, derive from positions_summary.
  - positions_summary is optional; if present, use for UI-only enrichment.

5. CSV Export (external)
- Columns: time_iso, symbol, action, qty, price, trade_val, fees, r_pnl, equity, d_equity, positions, bar_index, run_id.
- time_iso format: YYYY-MM-DDTHH:MM:SSZ
- Numeric formatting: explicit decimals (qty: 3; price/trade_val/fees/r_pnl/equity/d_equity: 2).
- No ANSI or emoji in CSV.

6. Brokerage-Grade Statement (Monthly/Period)
- Sections:
  - Account Summary: period start/end, starting equity, ending equity, net deposits/withdrawals, total P&L, return %.
  - Activity Summary: trades count, buys/sells, fees, commissions, realized P&L, unrealized change.
  - Cash Summary: starting cash, net cash flows, fees/commissions, ending cash, minimum available.
  - Positions Summary (end of period): per-symbol qty, market price, market value, cost basis, unrealized P&L.
  - Trade Detail: chronologically ordered trades with the Position History columns plus order id if available.
  - Risk Metrics: daily returns Sharpe, max drawdown, volatility.
- Output Formats:
  - CLI compact summary with clearly labeled sections.
  - JSON export with structured sections.
  - CSVs: positions_end.csv, activity.csv, summary.csv.

7. Portfolio Management Constraints
- Cash discipline:
  - No negative cash balances. Reject buy that exceeds available cash (after fees).
  - Selling requires existing position; partial sells allowed up to position size.
  - Fees/commissions deducted from cash immediately.
- Position sizing:
  - Configurable max_position_size (fraction of equity).
  - Signal-based sizing strategy; deterministic and testable.
- Reconciliation:
  - After each trade: update cash, equity, realized/unrealized P&L; positions map maintained.
  - Invariants: equity = cash + sum(positions market value).

8. CLI Commands (requirements)
- sentio_cli audit position-history [--max N] [--no-color] [--local-time]
- sentio_cli audit summarize
- sentio_cli statement generate --run <id> [--period YYYY-MM]
- sentio_cli export csv --run <id> --out-dir <path>

9. Validation & Tests
- Unit tests for formatting helpers (money, qty, time) and alignment widths.
- Simulation tests: verify no negative cash, correct equity math, realized/unrealized accounting.
- Golden-file tests for CSV and JSON exports.

10. Logging
- All info/debug logs to logs/app.log; errors to logs/errors.log; debug to logs/debug.log.
- Redact PII/secrets; include ISO timestamps, level, module:file:line, run_id.

11. Backward Compatibility
- Maintain JSONL schema; symbol correction logic in reader ensures compatibility.
- CLI options preserved; new options are additive and optional.

12. Non-Goals
- Broker connectivity, tax lots selection, and corporate actions are out-of-scope for this phase.


