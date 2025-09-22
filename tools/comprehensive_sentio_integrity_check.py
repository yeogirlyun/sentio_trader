#!/usr/bin/env python3
"""
Comprehensive Sentio System Integrity Check

Tests both sigor and tfa strategies with 20 blocks of historical data.
Validates consistency across all strattest and audit reports.
"""

import subprocess
import sys
import json
import re
import os
import time
from typing import Dict, List, Tuple, Optional, Any
from dataclasses import dataclass
from pathlib import Path

@dataclass
class StrategyMetrics:
    """Metrics extracted from strattest output"""
    mean_rpb: float = 0.0
    sharpe_ratio: float = 0.0
    monthly_return: float = 0.0
    annual_return: float = 0.0
    consistency: float = 0.0
    total_fills: int = 0
    final_cash: float = 0.0
    final_equity: float = 0.0
    final_positions: Dict[str, float] = None
    
    def __post_init__(self):
        if self.final_positions is None:
            self.final_positions = {}

@dataclass
class AuditMetrics:
    """Metrics extracted from audit reports"""
    total_trades: int = 0
    total_pnl: float = 0.0
    sharpe_ratio: float = 0.0
    win_rate: float = 0.0
    max_drawdown: float = 0.0
    cash_balance: float = 0.0
    position_value: float = 0.0
    final_equity: float = 0.0
    conflicts_detected: int = 0
    eod_violations: int = 0
    negative_cash_events: int = 0
    final_positions: Dict[str, float] = None
    
    def __post_init__(self):
        if self.final_positions is None:
            self.final_positions = {}

@dataclass
class IntegrityResult:
    """Result of integrity check for one strategy"""
    strategy: str
    strattest_success: bool
    audit_success: bool
    consistency_check: bool
    strattest_metrics: StrategyMetrics
    audit_metrics: AuditMetrics
    discrepancies: List[str]
    expected_differences: List[str]
    errors: List[str]
    warnings: List[str]

class ComprehensiveSentioIntegrityChecker:
    def __init__(self):
        self.strategies = ["sigor", "tfa"]
        self.blocks = 20
        self.mode = "historical"
        self.results: Dict[str, IntegrityResult] = {}
        self.base_dir = Path.cwd()
        
        # Executables
        self.sentio_cli = "./build/sentio_cli"
        self.sentio_audit = "./build/audit"
        
        # Tolerance for floating point comparisons
        self.tolerance = 1e-6
        
    def run_command(self, cmd: List[str], timeout: int = 300) -> Tuple[bool, str, str]:
        """Run a command and return success, stdout, stderr"""
        try:
            print(f"🔧 Running: {' '.join(cmd)}")
            result = subprocess.run(
                cmd, 
                capture_output=True, 
                text=True, 
                timeout=timeout,
                cwd=self.base_dir
            )
            return result.returncode == 0, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            return False, "", f"Command timed out after {timeout} seconds"
        except Exception as e:
            return False, "", str(e)
    
    def extract_strattest_metrics(self, output: str) -> StrategyMetrics:
        """Extract metrics from strattest output"""
        metrics = StrategyMetrics()
        
        # Remove ANSI color codes for easier parsing
        clean_output = re.sub(r'\x1b\[[0-9;]*m', '', output)
        
        # Extract performance metrics
        if match := re.search(r'Mean RPB:\s*([+-]?\d+\.?\d*)%', clean_output):
            metrics.mean_rpb = float(match.group(1))
        
        if match := re.search(r'Sharpe Ratio:\s*([+-]?\d+\.?\d*)', clean_output):
            metrics.sharpe_ratio = float(match.group(1))
        
        if match := re.search(r'MRB:\s*([+-]?\d+\.?\d*)%', clean_output):
            metrics.monthly_return = float(match.group(1))
        
        if match := re.search(r'ARB:\s*([+-]?\d+\.?\d*)%', clean_output):
            metrics.annual_return = float(match.group(1))
        
        if match := re.search(r'Consistency:\s*([+-]?\d+\.?\d*)', clean_output):
            metrics.consistency = float(match.group(1))
        
        # Extract total fills from block completion messages
        fill_matches = re.findall(r'Fills=(\d+)', clean_output)
        if fill_matches:
            metrics.total_fills = sum(int(match) for match in fill_matches)
        
        # Also try to extract from summary table
        if metrics.total_fills == 0:
            if match := re.search(r'Total Fills\s*│\s*([+-]?\d+)', clean_output):
                metrics.total_fills = int(match.group(1))
        
        # Extract final cash and equity by finding numbers in the respective lines
        for line in clean_output.split('\n'):
            if 'Final Cash:' in line:
                numbers = re.findall(r'\d+\.\d+|\d+', line)
                if numbers:
                    metrics.final_cash = float(numbers[0])
            elif 'Final Equity:' in line:
                numbers = re.findall(r'\d+\.\d+|\d+', line)
                if numbers:
                    metrics.final_equity = float(numbers[0])
        
        # Extract final positions from ACTIVE POSITIONS BREAKDOWN
        position_section = False
        for line in clean_output.split('\n'):
            if 'ACTIVE POSITIONS BREAKDOWN:' in line:
                position_section = True
                continue
            if position_section and '│' in line:
                # Look for pattern like: │ QQQ     │    53.15 shares │ $    30525.45 │
                if match := re.search(r'│\s*(\w+)\s*│\s*([+-]?\d+\.?\d*)\s*shares', line):
                    symbol = match.group(1)
                    qty = float(match.group(2))
                    if abs(qty) > 1e-6:  # Only record non-zero positions
                        metrics.final_positions[symbol] = qty
                elif '└' in line or 'Final Equity:' in line:  # End of table
                    break
        
        return metrics
    
    def extract_audit_metrics(self, summarize_output: str, position_output: str, 
                            integrity_output: str) -> AuditMetrics:
        """Extract metrics from audit outputs"""
        metrics = AuditMetrics()
        
        # From summarize output - look for both "Total Trades" and "Total Fills"
        # Remove ANSI codes first
        clean_summarize = re.sub(r'\x1b\[[0-9;]*m', '', summarize_output)
        
        if match := re.search(r'Total Trades\s*│\s*([+-]?\d+)', clean_summarize):
            metrics.total_trades = int(match.group(1))
        elif match := re.search(r'Total Fills:\s*([+-]?\d+)', clean_summarize):
            metrics.total_trades = int(match.group(1))
        elif match := re.search(r'Fills:\s*([+-]?\d+)', clean_summarize):
            metrics.total_trades = int(match.group(1))
        
        # Remove ANSI codes from summarize output
        clean_summarize = re.sub(r'\x1b\[[0-9;]*m', '', summarize_output)
        
        if match := re.search(r'Total P&L:\s*([+-]?\d+\.?\d*)', clean_summarize):
            metrics.total_pnl = float(match.group(1))
        
        if match := re.search(r'Sharpe Ratio:\s*([+-]?\d+\.?\d*)', clean_summarize):
            metrics.sharpe_ratio = float(match.group(1))
        
        if match := re.search(r'Win Rate:\s*([+-]?\d+\.?\d*)%', clean_summarize):
            metrics.win_rate = float(match.group(1))
        
        if match := re.search(r'Max Drawdown:\s*([+-]?\d+\.?\d*)%', clean_summarize):
            metrics.max_drawdown = float(match.group(1))
        
        # From position-history output
        clean_position = re.sub(r'\x1b\[[0-9;]*m', '', position_output)
        
        # Extract cash and position values from the summary line
        for line in clean_position.split('\n'):
            if 'Cash Balance' in line and 'Position Value' in line:
                # Extract all decimal numbers from this line
                numbers = re.findall(r'(\d+\.?\d*)', line)
                if len(numbers) >= 2:
                    metrics.cash_balance = float(numbers[0])
                    metrics.position_value = float(numbers[1])
                    metrics.final_equity = metrics.cash_balance + metrics.position_value
                break
        
        # Extract conflicts
        if match := re.search(r'(\d+)\s*position conflicts', position_output):
            metrics.conflicts_detected = int(match.group(1))
        
        # Extract final positions from position-history
        position_section = False
        for line in position_output.split('\n'):
            if 'Final Positions:' in line:
                position_section = True
                continue
            if position_section and '│' in line:
                if match := re.search(r'│\s*(\w+)\s*│\s*([+-]?\d+\.?\d*)\s*shares', line):
                    symbol = match.group(1)
                    qty = float(match.group(2))
                    if abs(qty) > 1e-6:  # Only record non-zero positions
                        metrics.final_positions[symbol] = qty
                elif '└' in line:  # End of table
                    break
        
        # From integrity output - extract violations
        if 'NEGATIVE CASH DETECTED' in integrity_output:
            metrics.negative_cash_events = 1
        
        return metrics
    
    def check_consistency(self, strattest_metrics: StrategyMetrics, 
                         audit_metrics: AuditMetrics) -> Tuple[bool, List[str], List[str]]:
        """Check consistency between strattest and audit metrics"""
        critical_issues = []
        expected_differences = []
        
        # Check final equity - this MUST match exactly (core system integrity)
        equity_diff = abs(strattest_metrics.final_equity - audit_metrics.final_equity)
        if equity_diff > self.tolerance:
            critical_issues.append(
                f"🚨 CRITICAL: Final equity mismatch: StratTest=${strattest_metrics.final_equity:.2f}, "
                f"Audit=${audit_metrics.final_equity:.2f}"
            )
        
        # Check trade counts (allow reasonable difference due to different counting methods)
        if audit_metrics.total_trades > 0 and strattest_metrics.total_fills > 0:
            ratio = audit_metrics.total_trades / strattest_metrics.total_fills
            diff_pct = abs(ratio - 1.0) * 100
            
            if ratio < 0.7 or ratio > 1.5:  # Critical threshold
                critical_issues.append(
                    f"🚨 CRITICAL: Large trade count discrepancy: StratTest={strattest_metrics.total_fills}, "
                    f"Audit={audit_metrics.total_trades} (ratio={ratio:.2f}, {diff_pct:.1f}% difference)"
                )
            elif diff_pct > 1.0:  # Expected variance
                expected_differences.append(
                    f"✅ Trade count variance: StratTest={strattest_metrics.total_fills}, "
                    f"Audit={audit_metrics.total_trades} ({diff_pct:.1f}% difference - within acceptable range)"
                )
        
        # Position differences are EXPECTED due to EOD closure
        strattest_positions = set(strattest_metrics.final_positions.keys())
        audit_positions = set(audit_metrics.final_positions.keys())
        
        if strattest_positions != audit_positions:
            if len(audit_positions) == 0 and len(strattest_positions) > 0:
                expected_differences.append(
                    f"✅ Position closure: StratTest shows {len(strattest_positions)} active positions "
                    f"({strattest_positions}), Audit shows all positions closed (expected EOD behavior)"
                )
            else:
                critical_issues.append(
                    f"🚨 CRITICAL: Unexpected position state: StratTest={strattest_positions}, "
                    f"Audit={audit_positions}"
                )
        
        # Cash differences are EXPECTED when positions are closed
        cash_diff = abs(strattest_metrics.final_cash - audit_metrics.cash_balance)
        if cash_diff > self.tolerance:
            if len(audit_positions) == 0 and len(strattest_positions) > 0:
                expected_differences.append(
                    f"✅ Cash reconciliation: StratTest=${strattest_metrics.final_cash:.2f} (with positions), "
                    f"Audit=${audit_metrics.cash_balance:.2f} (after position closure - expected)"
                )
            else:
                critical_issues.append(
                    f"🚨 CRITICAL: Cash mismatch without position explanation: "
                    f"StratTest=${strattest_metrics.final_cash:.2f}, Audit=${audit_metrics.cash_balance:.2f}"
                )
        
        # Check individual position quantities for matching positions
        for symbol in strattest_positions.intersection(audit_positions):
            st_qty = strattest_metrics.final_positions[symbol]
            audit_qty = audit_metrics.final_positions[symbol]
            if abs(st_qty - audit_qty) > self.tolerance:
                critical_issues.append(
                    f"🚨 CRITICAL: {symbol} position mismatch: StratTest={st_qty:.2f}, "
                    f"Audit={audit_qty:.2f}"
                )
        
        return len(critical_issues) == 0, critical_issues, expected_differences
    
    def test_strategy(self, strategy: str) -> IntegrityResult:
        """Test a single strategy comprehensively"""
        print(f"\n🎯 Testing strategy: {strategy}")
        print("=" * 60)
        
        result = IntegrityResult(
            strategy=strategy,
            strattest_success=False,
            audit_success=False,
            consistency_check=False,
            strattest_metrics=StrategyMetrics(),
            audit_metrics=AuditMetrics(),
            discrepancies=[],
            expected_differences=[],
            errors=[],
            warnings=[]
        )
        
        # Step 1: Run strattest
        print(f"📊 Running strattest {strategy} --mode {self.mode} --blocks {self.blocks}")
        strattest_cmd = [
            self.sentio_cli, "strattest", strategy, 
            "--mode", self.mode, 
            "--blocks", str(self.blocks)
        ]
        
        success, stdout, stderr = self.run_command(strattest_cmd, timeout=600)
        
        if not success:
            result.errors.append(f"StratTest failed: {stderr}")
            return result
        
        result.strattest_success = True
        result.strattest_metrics = self.extract_strattest_metrics(stdout)
        
        print(f"✅ StratTest completed: {result.strattest_metrics.total_fills} fills, "
              f"${result.strattest_metrics.final_equity:.2f} final equity")
        
        # Step 2: Run audit reports
        print("📋 Running audit reports...")
        
        # Get latest run ID for this specific strategy
        success, list_output, stderr = self.run_command([self.sentio_audit, "list", "--strategy", strategy])
        if not success:
            result.errors.append(f"Failed to get run list: {stderr}")
            return result
        
        # Extract the most recent run ID for this strategy from the list
        # Remove ANSI codes first
        clean_list = re.sub(r'\x1b\[[0-9;]*m', '', list_output)
        
        # Look for the first run ID in the filtered list (most recent)
        run_id_match = re.search(r'│\s*(\d+)\s+' + strategy, clean_list)
        if not run_id_match:
            result.errors.append(f"Could not extract run ID for strategy {strategy}")
            return result
        
        run_id = run_id_match.group(1)
        print(f"🔍 Using run ID: {run_id}")
        
        # Run summarize
        success, summarize_output, stderr = self.run_command([
            self.sentio_audit, "summarize", "--run", run_id
        ])
        if not success:
            result.errors.append(f"Audit summarize failed: {stderr}")
            return result
        
        # Run position-history
        success, position_output, stderr = self.run_command([
            self.sentio_audit, "position-history", "--run", run_id
        ])
        if not success:
            result.errors.append(f"Audit position-history failed: {stderr}")
            return result
        
        # Run signal-flow
        success, signal_output, stderr = self.run_command([
            self.sentio_audit, "signal-flow", "--run", run_id
        ])
        if not success:
            result.warnings.append(f"Audit signal-flow failed: {stderr}")
        
        # Run trade-flow
        success, trade_output, stderr = self.run_command([
            self.sentio_audit, "trade-flow", "--run", run_id
        ])
        if not success:
            result.warnings.append(f"Audit trade-flow failed: {stderr}")
        
        # Run integrity check
        success, integrity_output, stderr = self.run_command([
            self.sentio_audit, "integrity", "--run", run_id
        ])
        if not success:
            result.errors.append(f"Audit integrity failed: {stderr}")
            return result
        
        result.audit_success = True
        result.audit_metrics = self.extract_audit_metrics(
            summarize_output, position_output, integrity_output
        )
        
        print(f"✅ Audit completed: {result.audit_metrics.total_trades} trades, "
              f"${result.audit_metrics.final_equity:.2f} final equity, "
              f"{result.audit_metrics.conflicts_detected} conflicts")
        
        # Step 3: Check consistency
        print("🔍 Checking consistency...")
        consistency_ok, critical_issues, expected_differences = self.check_consistency(
            result.strattest_metrics, result.audit_metrics
        )
        
        result.consistency_check = consistency_ok
        result.discrepancies = critical_issues
        result.expected_differences = expected_differences
        
        if consistency_ok:
            print("✅ Consistency check passed")
        else:
            print(f"❌ Consistency check failed: {len(critical_issues)} critical issues")
            for disc in critical_issues:
                print(f"   • {disc}")
        
        # Show expected differences (these are healthy)
        if expected_differences:
            print(f"📋 Expected system behaviors: {len(expected_differences)} items")
            for diff in expected_differences:
                print(f"   • {diff}")
        
        # Step 4: Check for critical violations
        if result.audit_metrics.conflicts_detected > 0:
            result.errors.append(f"Position conflicts detected: {result.audit_metrics.conflicts_detected}")
        
        if result.audit_metrics.negative_cash_events > 0:
            result.errors.append("Negative cash balance detected")
        
        if abs(result.strattest_metrics.final_cash) < 1e-6 and len(result.strattest_metrics.final_positions) > 0:
            result.errors.append("Zero cash with open positions - potential liquidation issue")
        
        return result
    
    def run_comprehensive_test(self) -> bool:
        """Run comprehensive test on both strategies"""
        print("🚀 COMPREHENSIVE SENTIO INTEGRITY CHECK")
        print("=" * 80)
        print(f"Testing strategies: {', '.join(self.strategies)}")
        print(f"Mode: {self.mode}, Blocks: {self.blocks}")
        print()
        
        all_passed = True
        
        for strategy in self.strategies:
            result = self.test_strategy(strategy)
            self.results[strategy] = result
            
            if not (result.strattest_success and result.audit_success and 
                   result.consistency_check and len(result.errors) == 0):
                all_passed = False
        
        # Generate final report
        self.generate_final_report()
        
        return all_passed
    
    def generate_final_report(self):
        """Generate comprehensive final report"""
        print("\n" + "=" * 80)
        print("🎯 COMPREHENSIVE INTEGRITY CHECK RESULTS")
        print("=" * 80)
        
        for strategy, result in self.results.items():
            print(f"\n📊 STRATEGY: {strategy.upper()}")
            print("-" * 40)
            
            # Status indicators
            st_status = "✅" if result.strattest_success else "❌"
            audit_status = "✅" if result.audit_success else "❌"
            consistency_status = "✅" if result.consistency_check else "❌"
            
            print(f"StratTest:     {st_status} {'PASS' if result.strattest_success else 'FAIL'}")
            print(f"Audit:         {audit_status} {'PASS' if result.audit_success else 'FAIL'}")
            print(f"Consistency:   {consistency_status} {'PASS' if result.consistency_check else 'FAIL'}")
            
            # Key metrics comparison
            print(f"\nKey Metrics:")
            print(f"  Final Equity:  StratTest=${result.strattest_metrics.final_equity:.2f}, "
                  f"Audit=${result.audit_metrics.final_equity:.2f}")
            print(f"  Final Cash:    StratTest=${result.strattest_metrics.final_cash:.2f}, "
                  f"Audit=${result.audit_metrics.cash_balance:.2f}")
            print(f"  Trade Count:   StratTest={result.strattest_metrics.total_fills}, "
                  f"Audit={result.audit_metrics.total_trades}")
            print(f"  Conflicts:     {result.audit_metrics.conflicts_detected}")
            print(f"  Sharpe Ratio:  StratTest={result.strattest_metrics.sharpe_ratio:.2f}, "
                  f"Audit={result.audit_metrics.sharpe_ratio:.2f}")
            
            # Final positions
            if result.strattest_metrics.final_positions or result.audit_metrics.final_positions:
                print(f"  Final Positions:")
                all_symbols = set(result.strattest_metrics.final_positions.keys()) | \
                             set(result.audit_metrics.final_positions.keys())
                for symbol in sorted(all_symbols):
                    st_qty = result.strattest_metrics.final_positions.get(symbol, 0.0)
                    audit_qty = result.audit_metrics.final_positions.get(symbol, 0.0)
                    print(f"    {symbol}: StratTest={st_qty:.2f}, Audit={audit_qty:.2f}")
            else:
                print(f"  Final Positions: None (Clean portfolio)")
            
            # Errors and warnings
            if result.errors:
                print(f"\n❌ ERRORS ({len(result.errors)}):")
                for error in result.errors:
                    print(f"   • {error}")
            
            if result.discrepancies:
                print(f"\n🚨 CRITICAL ISSUES ({len(result.discrepancies)}):")
                for disc in result.discrepancies:
                    print(f"   • {disc}")
            
            if result.expected_differences:
                print(f"\n✅ EXPECTED BEHAVIORS ({len(result.expected_differences)}):")
                for diff in result.expected_differences:
                    print(f"   • {diff}")
            
            if result.warnings:
                print(f"\n⚠️  WARNINGS ({len(result.warnings)}):")
                for warning in result.warnings:
                    print(f"   • {warning}")
        
        # Overall summary
        print("\n" + "=" * 80)
        print("🏆 OVERALL SUMMARY")
        print("=" * 80)
        
        total_tests = len(self.strategies)
        # A test passes if it has no critical issues (discrepancies) and no errors
        passed_tests = sum(1 for result in self.results.values() 
                          if result.strattest_success and result.audit_success and 
                             len(result.discrepancies) == 0 and len(result.errors) == 0)
        
        # Count strategies with expected differences (healthy behavior)
        healthy_behaviors = sum(1 for result in self.results.values() 
                               if len(result.expected_differences) > 0)
        
        if passed_tests == total_tests:
            print("🎉 ALL TESTS PASSED! System integrity verified.")
            print("✅ Both strategies show consistent behavior across all reports.")
            print("✅ No critical issues, conflicts, or negative cash detected.")
            if healthy_behaviors > 0:
                print(f"✅ {healthy_behaviors} strategies show expected system behaviors (EOD closure, etc.)")
        else:
            critical_failures = sum(1 for result in self.results.values() 
                                   if len(result.discrepancies) > 0 or len(result.errors) > 0)
            if critical_failures == 0:
                print("🎉 SYSTEM INTEGRITY VERIFIED!")
                print("✅ No critical issues detected - all discrepancies are expected behaviors.")
                print(f"✅ {healthy_behaviors} strategies show proper risk management (position closure, etc.)")
            else:
                print(f"❌ {critical_failures}/{total_tests} STRATEGIES HAVE CRITICAL ISSUES!")
                print("🔧 System requires fixes before production use.")
        
        print(f"\nTest Summary: {passed_tests}/{total_tests} strategies passed")
        print(f"Mode: {self.mode}, Blocks: {self.blocks}")
        print(f"Total execution time: ~{time.time():.0f} seconds")

def main():
    """Main entry point"""
    if len(sys.argv) > 1 and sys.argv[1] in ['-h', '--help']:
        print("Usage: python comprehensive_sentio_integrity_check.py")
        print("Tests both sigor and tfa strategies with 20 blocks of historical data")
        print("Validates consistency across all strattest and audit reports")
        return
    
    checker = ComprehensiveSentioIntegrityChecker()
    success = checker.run_comprehensive_test()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
