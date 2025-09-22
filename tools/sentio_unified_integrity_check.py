#!/usr/bin/env python3
"""
Sentio Unified Integrity Check
===============================

This script serves as the single source of truth for validating the end-to-end
integrity of the Sentio trading system. It tests all major trading algorithms,
validates core trading principles, and reports on both critical violations
and behavioral anomalies to prevent wrongful directional development.

Five Core Principles Validated:
1.  ✅ No Negative Cash Balance
2.  ✅ No Conflicting Positions (e.g., Long + Inverse ETFs)
3.  ✅ All Positions Closed at End of Day (EOD)
4.  ✅ Reasonable Trade Frequency (e.g., one opening trade per bar)
5.  ✅ Effective Capital Utilization

Algorithms Tested:
-   Standard Position State Machine (PSM)
-   Regime-Adaptive Momentum Scalper
-   Q-Learning Adaptive Thresholds
"""

import subprocess
import json
import pandas as pd
import numpy as np
import time
import sys
from pathlib import Path
import re
from typing import Dict, List, Any
from dataclasses import dataclass, field

# --- Configuration ---
@dataclass
class TestCase:
    """Defines a single integrity test case."""
    name: str
    description: str
    trade_args: List[str]
    # Expected behavior for anomaly detection
    expected_trades_min: int
    expected_trades_max: int
    expected_avg_holding_min_minutes: float
    expected_avg_holding_max_minutes: float
    expected_capital_utilization_min_pct: float

@dataclass
class TestResult:
    """Stores the results of a single test case."""
    test_case: TestCase
    run_id: str = ""
    success: bool = False
    execution_time: float = 0.0
    metrics: Dict[str, Any] = field(default_factory=dict)
    violations: List[str] = field(default_factory=list)
    anomalies: List[str] = field(default_factory=list)

# Define all test cases for the integrity check
TEST_CONFIGURATIONS = [
    TestCase(
        name="Standard PSM",
        description="Core logic with static thresholds and leverage.",
        trade_args=["--leverage-enabled"],
        expected_trades_min=1,
        expected_trades_max=50,
        expected_avg_holding_min_minutes=1,
        expected_avg_holding_max_minutes=1440, # 1 day
        expected_capital_utilization_min_pct=10.0
    ),
    TestCase(
        name="Momentum Scalper",
        description="High-frequency, trend-following algorithm.",
        trade_args=["--leverage-enabled", "--scalper"],
        expected_trades_min=1,
        expected_trades_max=100,
        expected_avg_holding_min_minutes=1,
        expected_avg_holding_max_minutes=120, # 2 hours
        expected_capital_utilization_min_pct=10.0
    ),
    TestCase(
        name="Adaptive Thresholds",
        description="Q-Learning based threshold optimization.",
        trade_args=["--leverage-enabled", "--adaptive"],
        expected_trades_min=1,
        expected_trades_max=50,
        expected_avg_holding_min_minutes=1,
        expected_avg_holding_max_minutes=1440,
        expected_capital_utilization_min_pct=10.0
    ),
    TestCase(
        name="No Leverage",
        description="QQQ-only trading without leveraged instruments.",
        trade_args=["--no-leverage"],
        expected_trades_min=1,
        expected_trades_max=50,
        expected_avg_holding_min_minutes=1,
        expected_avg_holding_max_minutes=2880, # 2 days
        expected_capital_utilization_min_pct=10.0
    )
]

# --- ANSI Colors for Professional Reporting ---
class Colors:
    RESET = '\033[0m'
    BOLD = '\033[1m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'

# --- Main Checker Class ---
class UnifiedIntegrityChecker:
    def __init__(self):
        self.base_dir = Path.cwd()
        self.sentio_cli = "./build/sentio_cli"
        self.results: List[TestResult] = []

    def run_command(self, cmd: List[str], timeout: int = 600) -> tuple[bool, str, str, float]:
        """Runs a command, returning success, stdout, stderr, and execution time."""
        print(f"{Colors.CYAN}🔧 Running: {' '.join(cmd)}{Colors.RESET}")
        start_time = time.time()
        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, timeout=timeout, cwd=self.base_dir
            )
            exec_time = time.time() - start_time
            return result.returncode == 0, result.stdout, result.stderr, exec_time
        except Exception as e:
            exec_time = time.time() - start_time
            return False, "", str(e), exec_time

    def run_all_tests(self):
        """Orchestrates the entire integrity check process."""
        print(f"{Colors.BOLD}{Colors.MAGENTA}🚀 Starting Sentio Unified Integrity Check...{Colors.RESET}")

        for test_case in TEST_CONFIGURATIONS:
            result = self.run_test_case(test_case)
            self.results.append(result)

        self.generate_final_report()
        return all(r.success and not r.violations for r in self.results)

    def run_test_case(self, test_case: TestCase) -> TestResult:
        """Runs a single end-to-end test case from signal generation to analysis."""
        print(f"\n{'='*25} TESTING: {test_case.name} {'='*25}")
        result = TestResult(test_case=test_case)

        # 1. Use small test signal file for fast testing
        test_signal_file = "data/signals/test_signals_small.jsonl"
        
        # 2. Trade Execution
        trade_cmd = [self.sentio_cli, "trade", "--signals", test_signal_file, "--blocks", "1"] + test_case.trade_args
        success, stdout, stderr, exec_time = self.run_command(trade_cmd)
        result.execution_time = exec_time
        if not success:
            result.violations.append(f"Trade execution failed: {stderr}")
            return result

        # Extract Run ID
        run_id_match = re.search(r'Run ID:\s*(\d+)', re.sub(r'\x1b\[[0-9;]*m', '', stdout))
        if not run_id_match:
            result.violations.append("Could not extract Run ID from trade output.")
            return result
        result.run_id = run_id_match.group(1)
        print(f"{Colors.GREEN}✅ Trading complete. Run ID: {result.run_id}{Colors.RESET}")

        # 3. Analysis
        trade_file = self.base_dir / "data" / "trades" / f"{result.run_id}_trades.jsonl"
        if not trade_file.exists():
            result.violations.append(f"Trade file not found: {trade_file}")
            return result

        self.analyze_trade_file(trade_file, result)
        result.success = True
        return result

    def analyze_trade_file(self, trade_file: Path, result: TestResult):
        """Performs all checks and anomaly detection on a trade file."""
        df = self.parse_trade_file(trade_file)
        if df.empty:
            result.violations.append("Trade file is empty or could not be parsed.")
            return

        self._check_critical_violations(df, result)
        self._check_behavioral_anomalies(df, result)
        self._extract_metrics(df, result)

    def parse_trade_file(self, trade_file: Path) -> pd.DataFrame:
        """Parses the JSONL trade file into a pandas DataFrame."""
        records = []
        with open(trade_file, 'r') as f:
            for line in f:
                try:
                    records.append(json.loads(line))
                except json.JSONDecodeError:
                    print(f"Warning: Could not parse line in {trade_file}: {line.strip()}")
        df = pd.DataFrame(records)
        if not df.empty:
            # Convert timestamp_ms to numeric first, then to datetime
            df['timestamp_ms'] = pd.to_numeric(df['timestamp_ms'], errors='coerce')
            df['timestamp'] = pd.to_datetime(df['timestamp_ms'], unit='ms', utc=True)
            
            # Convert numeric columns from strings to floats
            numeric_columns = ['cash_after', 'cash_before', 'equity_after', 'equity_before', 
                             'price', 'quantity', 'trade_value', 'fees', 'realized_pnl_delta', 'unrealized_after']
            for col in numeric_columns:
                if col in df.columns:
                    df[col] = pd.to_numeric(df[col], errors='coerce')
        return df

    def _check_critical_violations(self, df: pd.DataFrame, result: TestResult):
        """Checks for undeniable bugs and principle violations."""
        # 1. No Negative Cash
        if 'cash_after' in df.columns and (df['cash_after'] < -0.01).any():
            bad_trades = df[df['cash_after'] < -0.01]
            for _, row in bad_trades.iterrows():
                result.violations.append(f"Negative Cash: ${row['cash_after']:.2f} at {row['timestamp']}")

        # 2. No Conflicting Positions
        long_symbols = {'QQQ', 'TQQQ'}
        short_symbols = {'PSQ', 'SQQQ'}
        if 'positions_summary' in df.columns:
            for i, row in df.iterrows():
                try:
                    # Parse positions summary (format: "symbol:count,symbol:count")
                    positions_str = row['positions_summary']
                    if positions_str and positions_str != "":
                        # Extract symbols that have non-zero positions
                        position_matches = re.findall(r'([A-Z]+):([+-]?\d*\.?\d+)', positions_str)
                        active_positions = {symbol for symbol, count in position_matches if float(count) > 1e-6}
                        
                        # Check for conflicts
                        has_long = any(s in active_positions for s in long_symbols)
                        has_short = any(s in active_positions for s in short_symbols)
                        if has_long and has_short:
                            result.violations.append(f"Conflicting Positions held at {row['timestamp']}: {active_positions}")
                except (ValueError, KeyError, AttributeError):
                    continue

    def _check_behavioral_anomalies(self, df: pd.DataFrame, result: TestResult):
        """Checks for unexpected behavior that may indicate wrongful development."""
        test_case = result.test_case

        # 1. EOD Positions
        if 'timestamp' in df.columns and 'positions_summary' in df.columns:
            df['date'] = df['timestamp'].dt.date
            eod_positions = df.groupby('date').last()['positions_summary'].apply(
                lambda s: bool(s and re.search(r':([1-9]\d*\.?\d*)', str(s)))
            )
            overnight_days = eod_positions[eod_positions].count()
            if overnight_days > 0:
                result.anomalies.append(f"Held positions overnight on {overnight_days} days.")

        # 2. Trading Frequency
        if 'action' in df.columns:
            trade_count = len(df[df['action'] != 'HOLD'])
            if not (test_case.expected_trades_min <= trade_count <= test_case.expected_trades_max):
                result.anomalies.append(
                    f"Trade frequency outside of expected range ({trade_count} trades, expected "
                    f"{test_case.expected_trades_min}-{test_case.expected_trades_max})."
                )

        # 3. Holding Periods (simplified analysis)
        if 'timestamp' in df.columns and len(df) > 1:
            total_duration = (df['timestamp'].iloc[-1] - df['timestamp'].iloc[0]).total_seconds() / 60
            trade_count = len(df[df['action'] != 'HOLD']) if 'action' in df.columns else len(df)
            if trade_count > 0:
                avg_holding = total_duration / trade_count
                if not (test_case.expected_avg_holding_min_minutes <= avg_holding <= test_case.expected_avg_holding_max_minutes):
                    result.anomalies.append(
                        f"Average holding period outside expected range ({avg_holding:.1f} mins, expected "
                        f"{test_case.expected_avg_holding_min_minutes}-{test_case.expected_avg_holding_max_minutes})."
                    )

        # 4. Capital Utilization
        if 'equity_after' in df.columns and 'cash_after' in df.columns:
            df['position_value'] = df['equity_after'] - df['cash_after']
            avg_utilization = (df['position_value'] / df['equity_after']).mean() * 100
            if avg_utilization < test_case.expected_capital_utilization_min_pct:
                result.anomalies.append(
                    f"Low capital utilization ({avg_utilization:.1f}%, expected > "
                    f"{test_case.expected_capital_utilization_min_pct}%)."
                )

    def _extract_metrics(self, df: pd.DataFrame, result: TestResult):
        """Extracts key performance indicators from the trade data."""
        metrics = result.metrics
        if df.empty: 
            return

        metrics['run_id'] = result.run_id
        
        if 'action' in df.columns:
            metrics['total_trades'] = len(df[df['action'] != 'HOLD'])
        else:
            metrics['total_trades'] = len(df)
            
        if 'equity_after' in df.columns:
            metrics['final_equity'] = df['equity_after'].iloc[-1]
            
        if 'equity_before' in df.columns:
            metrics['start_equity'] = df['equity_before'].iloc[0]
        elif 'equity_after' in df.columns:
            metrics['start_equity'] = df['equity_after'].iloc[0]
            
        if 'final_equity' in metrics and 'start_equity' in metrics:
            metrics['total_return_pct'] = (metrics['final_equity'] / metrics['start_equity'] - 1) * 100
            metrics['total_pnl'] = metrics['final_equity'] - metrics['start_equity']
            
        metrics['execution_time_s'] = result.execution_time

    def generate_final_report(self):
        """Prints a comprehensive, formatted report of all test results."""
        print(f"\n{'='*80}\n{Colors.BOLD}{Colors.MAGENTA}✅ SENTIO UNIFIED INTEGRITY REPORT{Colors.RESET}\n{'='*80}")

        overall_pass = True
        for result in self.results:
            if result.violations:
                overall_pass = False

        if overall_pass:
            print(f"{Colors.GREEN}{Colors.BOLD}Overall Status: PASS - No critical violations detected.{Colors.RESET}")
        else:
            print(f"{Colors.RED}{Colors.BOLD}Overall Status: FAIL - Critical violations detected.{Colors.RESET}")

        for result in self.results:
            case = result.test_case
            metrics = result.metrics
            status_color = Colors.GREEN if not result.violations else Colors.RED
            status_text = "PASS" if not result.violations else "FAIL"

            print(f"\n--- Test Case: {Colors.BOLD}{case.name}{Colors.RESET} [{status_color}{status_text}{Colors.RESET}] ---")
            print(f"  {Colors.CYAN}Description:{Colors.RESET} {case.description}")
            print(f"  {Colors.CYAN}Run ID:{Colors.RESET} {result.run_id}")

            # Metrics Table
            print(f"  ┌─────────────────────────┬────────────────────────┐")
            print(f"  │ {Colors.BOLD}Metric{Colors.RESET}                    │ {Colors.BOLD}Value{Colors.RESET}                    │")
            print(f"  ├─────────────────────────┼────────────────────────┤")
            print(f"  │ Total Trades            │ {metrics.get('total_trades', 'N/A'):<22} │")
            print(f"  │ Final Equity            │ ${metrics.get('final_equity', 0):<20,.2f} │")
            print(f"  │ Total Return            │ {metrics.get('total_return_pct', 0):>+8.2f}%{''*12:<12} │")
            print(f"  │ Execution Time          │ {metrics.get('execution_time_s', 0):<20.1f}s │")
            print(f"  └─────────────────────────┴────────────────────────┘")

            if result.violations:
                print(f"  {Colors.RED}{Colors.BOLD}Critical Violations ({len(result.violations)}):{Colors.RESET}")
                for v in result.violations:
                    print(f"    - {v}")

            if result.anomalies:
                print(f"  {Colors.YELLOW}{Colors.BOLD}Behavioral Anomalies ({len(result.anomalies)}):{Colors.RESET}")
                for a in result.anomalies:
                    print(f"    - {a}")
        print("\n" + "="*80)

if __name__ == "__main__":
    checker = UnifiedIntegrityChecker()
    success = checker.run_all_tests()
    sys.exit(0 if success else 1)
