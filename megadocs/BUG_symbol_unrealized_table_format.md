# Bug Report: Position-History Table Shows Timestamps In Symbol and Unrealized P&L Columns

## Title
Symbol and Unrealized P&L columns display timestamp strings instead of expected values in `audit position-history` output

## Environment
- Project: Sentio C++ Trading System
- Command: `sentio_cli audit position-history --max N`
- Affected files:
  - `src/cli/sentio_cli_main.cpp` (table rendering and parsing logic)
  - `src/common/utils.cpp` (JSON parser `utils::from_json`)
  - Trade book JSONL generated via `backend_component::process_to_jsonl`

## Current Behavior
- The `Symbol` column sometimes prints an ISO-like timestamp (e.g., `2025-09-12T14:44:00-04:00`) rather than the ticker (e.g., `QQQ`).
- The `Unrealized P&L` column shows a timestamp-like string in some rows.
- The table borders use ASCII with small gaps which looks less professional.

## Impact
- Report is misleading and not brokerage-grade.
- Post-hoc analysis and CSV exports may be corrupted if users copy from the table.

## Root Cause (confirmed)
1) The minimal JSON parser previously split pairs on commas without respecting quotes, corrupting values that contain commas (e.g., `positions_summary`).
2) When `positions_summary` leaked into adjacent fields during parsing, `symbol` and `unrealized_after` could be overwritten with timestamp-like substrings present in malformed values.

## Fix Implemented
- Hardened `utils::from_json` to split on commas/colons only when not inside quotes, handling escaped quotes correctly.
- In `position-history` renderer:
  - Sanitized `positions_summary` into `SYM:COUNT` pairs; skip entries that look like timestamps.
  - If `symbol` looks like a timestamp, recover ticker from first `positions_summary` entry.
  - Guarded numeric parsing (`std::stod`) with try/catch to avoid bleed-through.

## Remaining Issues
- Some trade books may already contain corrupted lines. The renderer now recovers best-effort, but historical data may still carry artifacts.
- Table borders are ASCII. Upgrade to Unicode box-drawing for professional appearance.
- Add color cues for positive/negative totals in summary boxes.

## Repro Steps
1. Run a trade to produce a JSONL trade book.
2. Run `sentio_cli audit position-history --max 30`.
3. Observe some rows showing ISO timestamps in `Symbol` and `Unrealized P&L` prior to fixes.

## Validation After Fix
- Rerun `sentio_cli audit position-history --max 30`.
- Verify `Symbol` shows ticker (e.g., `QQQ`) and `Unrealized P&L` shows numeric currency.
- Confirm no rows show timestamps in numeric columns.

## Proposed Enhancements
- Switch table borders to Unicode box characters (no gaps).
- Colorize action labels, and apply green/red to realized/unrealized totals and return %.
- Add `--no-unicode` flag to fall back to ASCII.

## Owner
- Assignee: Backend/CLI
- Priority: High
- Status: Fix landed; enhancements pending
