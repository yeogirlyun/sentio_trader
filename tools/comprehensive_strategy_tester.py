#!/usr/bin/env python3
"""
Comprehensive Strategy Tester
============================

This script runs all registered strategies through strattest, performs audit validation,
and generates a comprehensive test results report.

Usage:
    python tools/comprehensive_strategy_tester.py

Features:
- Tests all registered strategies with default parameters
- Runs audit validation for each strategy
- Validates metrics consistency across strattest and audit
- Generates comprehensive all-test-result.md report
- Confirms dataset period and test period alignment
"""

import subprocess
import json
import re
import os
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import time

class ComprehensiveStrategyTester:
    def __init__(self, project_root: str = None):
        self.project_root = Path(project_root) if project_root else Path(__file__).parent.parent
        self.results = []
        self.audit_results = []
        self.report_file = self.project_root / "all-test-result.md"
        
        # Registered strategies from REGISTER_STRATEGY macros
        self.strategies = [
            "ire",
            "bsb",  # BollingerSqueezeBreakout
            "kochi_ppo",
            "mm",   # MarketMaking
            "mvp",  # MomentumVolume
            "orb",  # OpeningRangeBreakout
            "ofi",  # OrderFlowImbalance
            "ofs",  # OrderFlowScalping
            "tfa",
            "vwap", # VWAPReversion
            "death_cross",
            "rsi_reversion",
            "bollinger_reversion"
        ]
        
        print(f"🚀 Comprehensive Strategy Tester Initialized")
        print(f"📁 Project Root: {self.project_root}")
        print(f"📊 Strategies to Test: {len(self.strategies)}")
        print(f"📄 Report File: {self.report_file}")
        print()

    def run_strattest(self, strategy: str) -> Dict:
        """Run strattest for a strategy using new Trading Block system"""
        print(f"🧪 Testing Strategy: {strategy}")
        print(f"📊 Using Trading Block Configuration: 10 TB × 480 bars (default)")
        
        try:
            # Run strattest command with Trading Block defaults
            # Default: 10 TB, 480 bars per TB, 1 simulation
            cmd = ["./build/sentio_cli", "strattest", strategy, "--blocks", "10", "--simulations", "1"]
            result = subprocess.run(
                cmd, 
                cwd=self.project_root,
                capture_output=True, 
                text=True, 
                timeout=300  # 5 minute timeout
            )
            
            if result.returncode != 0:
                print(f"❌ Strategy {strategy} failed: {result.stderr}")
                return {
                    "strategy": strategy,
                    "status": "FAILED",
                    "error": result.stderr,
                    "stdout": result.stdout
                }
            
            # Parse strattest output
            parsed = self.parse_strattest_output(result.stdout, strategy)
            parsed["status"] = "SUCCESS"
            parsed["raw_output"] = result.stdout
            
            print(f"✅ Strategy {strategy} completed successfully")
            return parsed
            
        except subprocess.TimeoutExpired:
            print(f"⏰ Strategy {strategy} timed out")
            return {
                "strategy": strategy,
                "status": "TIMEOUT",
                "error": "Test timed out after 5 minutes"
            }
        except Exception as e:
            print(f"💥 Strategy {strategy} crashed: {str(e)}")
            return {
                "strategy": strategy,
                "status": "CRASHED",
                "error": str(e)
            }

    def parse_strattest_output(self, output: str, strategy: str) -> Dict:
        """Parse strattest output to extract Trading Block metrics"""
        result = {
            "strategy": strategy,
            "run_id": "unknown",
            "dataset": "unknown",
            "dataset_period": "unknown", 
            "test_period": "10 TB (4800 bars ≈ 12.3 trading days)",
            "trading_blocks": 0,
            "total_bars": 0,
            "metrics": {}
        }
        
        lines = output.split('\n')
        
        # Extract run ID
        for line in lines:
            if "Run ID:" in line:
                result["run_id"] = line.split("Run ID:")[-1].strip()
                break
        
        # Extract Trading Block configuration and dataset information
        for line in lines:
            # Extract run ID from audit system
            if "Run ID:" in line:
                result["run_id"] = line.split("Run ID:")[-1].strip()
            elif "Trading Blocks:" in line and "TB ×" in line:
                # Extract: Trading Blocks: 10 TB × 480 bars
                match = re.search(r'Trading Blocks: (\d+) TB × (\d+) bars', line)
                if match:
                    result["trading_blocks"] = int(match.group(1))
                    block_size = int(match.group(2))
                    result["total_bars"] = result["trading_blocks"] * block_size
            elif "Total Duration:" in line and "bars" in line:
                # Extract: Total Duration: 4800 bars ≈ 12.3 trading days
                match = re.search(r'Total Duration: (\d+) bars', line)
                if match:
                    result["total_bars"] = int(match.group(1))
            elif "Loading" in line and "data/" in line:
                # Extract dataset path
                match = re.search(r'data/([^/]+/[^/]+\.csv)', line)
                if match:
                    result["dataset"] = match.group(1)
                    if "future_qqq" in match.group(1):
                        result["dataset_period"] = "Future QQQ Track"
        
        # Extract metrics from CANONICAL EVALUATION COMPLETE section
        canonical_section = False
        for line in lines:
            if "CANONICAL EVALUATION COMPLETE" in line:
                canonical_section = True
                continue
            elif canonical_section and line.strip():
                if "Mean RPB:" in line:
                    # Extract: Mean RPB: 0.0234%
                    match = re.search(r'Mean RPB: (-?\d+\.?\d*)%', line)
                    if match:
                        result["metrics"]["Mean RPB"] = float(match.group(1))
                elif "Std Dev RPB:" in line:
                    # Extract: Std Dev RPB: 0.0156%
                    match = re.search(r'Std Dev RPB: (-?\d+\.?\d*)%', line)
                    if match:
                        result["metrics"]["Std Dev RPB"] = float(match.group(1))
                elif "ARB (Annualized):" in line:
                    # Extract: ARB (Annualized): 12.45%
                    match = re.search(r'ARB \(Annualized\): (-?\d+\.?\d*)%', line)
                    if match:
                        result["metrics"]["ARB (Annualized)"] = float(match.group(1))
                elif "20TB Return" in line and "≈1 month" in line:
                    # Extract: 20TB Return (≈1 month): 4.82%
                    match = re.search(r'20TB Return.*?: (-?\d+\.?\d*)%', line)
                    if match:
                        result["metrics"]["20TB Return"] = float(match.group(1))
                elif "Aggregate Sharpe:" in line:
                    # Extract: Aggregate Sharpe: 1.23
                    match = re.search(r'Aggregate Sharpe: (-?\d+\.?\d*)', line)
                    if match:
                        result["metrics"]["Aggregate Sharpe"] = float(match.group(1))
                elif "Total Fills:" in line:
                    # Extract: Total Fills: 234
                    match = re.search(r'Total Fills: (\d+)', line)
                    if match:
                        result["metrics"]["Total Fills"] = int(match.group(1))
                elif "Consistency Score:" in line:
                    # Extract: Consistency Score: 0.0234
                    match = re.search(r'Consistency Score: (-?\d+\.?\d*)', line)
                    if match:
                        result["metrics"]["Consistency Score"] = float(match.group(1))
                elif line.startswith("==="):
                    break
        
        return result

    def run_audit_summarize(self, run_id: str) -> Dict:
        """Run audit summarize for a run ID"""
        print(f"🔍 Running audit summarize for Run ID: {run_id}")
        
        try:
            cmd = ["./saudit", "summarize", run_id]
            result = subprocess.run(
                cmd,
                cwd=self.project_root, 
                capture_output=True,
                text=True,
                timeout=60  # 1 minute timeout
            )
            
            if result.returncode != 0:
                print(f"❌ Audit summarize failed for {run_id}: {result.stderr}")
                return {
                    "run_id": run_id,
                    "status": "FAILED",
                    "error": result.stderr
                }
            
            # Parse audit output
            parsed = self.parse_audit_output(result.stdout, run_id)
            parsed["status"] = "SUCCESS"
            parsed["raw_output"] = result.stdout
            
            print(f"✅ Audit summarize completed for {run_id}")
            return parsed
            
        except subprocess.TimeoutExpired:
            print(f"⏰ Audit summarize timed out for {run_id}")
            return {
                "run_id": run_id,
                "status": "TIMEOUT",
                "error": "Audit summarize timed out"
            }
        except Exception as e:
            print(f"💥 Audit summarize crashed for {run_id}: {str(e)}")
            return {
                "run_id": run_id,
                "status": "CRASHED", 
                "error": str(e)
            }

    def parse_audit_output(self, output: str, run_id: str) -> Dict:
        """Parse audit summarize output with Trading Block support"""
        result = {
            "run_id": run_id,
            "strategy": "unknown",
            "trading_blocks": 0,
            "is_legacy_run": True,
            "dataset": "unknown",
            "dataset_period": "unknown",
            "test_period": "unknown",
            "metrics": {}
        }
        
        lines = output.split('\n')
        
        # Extract strategy and dataset info
        for line in lines:
            if "Strategy:" in line:
                result["strategy"] = line.split("Strategy:")[-1].strip()
            elif "Dataset Type:" in line:
                dataset_type = line.split("Dataset Type:")[-1].strip()
                result["dataset"] = f"future_qqq_track_01.csv ({dataset_type})"
            elif "Test Period:" in line:
                test_period = line.split("Test Period:")[-1].strip()
                result["test_period"] = test_period
        
        # Extract Trading Block metrics from new audit format
        performance_section = False
        for line in lines:
            if "Trading Performance (Trading Block Analysis):" in line:
                performance_section = True
                result["is_legacy_run"] = False
                continue
            elif "Trading Performance:" in line and "Trading Block" not in line:
                performance_section = True
                result["is_legacy_run"] = True
                continue
            elif performance_section and line.strip():
                # New Trading Block format
                if "📊 Trading Blocks:" in line and "TB" in line:
                    # Extract: 📊 Trading Blocks: 10 TB (480 bars each ≈ 8hrs)
                    match = re.search(r'(\d+) TB', line)
                    if match:
                        result["trading_blocks"] = int(match.group(1))
                elif "📈 Mean RPB (Return Per Block):" in line:
                    # Extract: 📈 Mean RPB (Return Per Block): 0.0234%
                    match = re.search(r'(-?\d+\.?\d*)%', line)
                    if match:
                        result["metrics"]["Mean RPB"] = float(match.group(1))
                elif "📈 Total Return (Compounded):" in line:
                    # Extract: 📈 Total Return (Compounded): 4.82%
                    match = re.search(r'(-?\d+\.?\d*)%', line)
                    if match:
                        result["metrics"]["Total Return"] = float(match.group(1))
                elif "🏆 20TB Return (≈1 month):" in line:
                    # Extract: 🏆 20TB Return (≈1 month): 4.82%
                    match = re.search(r'(-?\d+\.?\d*)%', line)
                    if match:
                        result["metrics"]["20TB Return"] = float(match.group(1))
                elif "📊 Sharpe Ratio:" in line:
                    # Extract: 📊 Sharpe Ratio: 1.23
                    match = re.search(r'(-?\d+\.?\d*)', line)
                    if match:
                        result["metrics"]["Aggregate Sharpe"] = float(match.group(1))
                elif "🔄 Daily Trades:" in line:
                    # Extract: 🔄 Daily Trades: 15.2
                    match = re.search(r'(-?\d+\.?\d*)', line)
                    if match:
                        result["metrics"]["Average Daily Trades"] = float(match.group(1))
                elif "📉 Max Drawdown:" in line:
                    # Extract: 📉 Max Drawdown: 12.45%
                    match = re.search(r'(\d+\.?\d*)%', line)
                    if match:
                        result["metrics"]["Maximum Drawdown"] = float(match.group(1))
                # Legacy format support
                elif "Total Return:" in line and "📈" not in line:
                    match = re.search(r'(-?\d+\.?\d*)%', line)
                    if match:
                        result["metrics"]["Total Return"] = float(match.group(1))
                elif "Monthly Projected Return (MPR):" in line:
                    match = re.search(r'(-?\d+\.?\d*)%', line)
                    if match:
                        result["metrics"]["Monthly Projected Return"] = float(match.group(1))
                elif "Sharpe Ratio:" in line and "📊" not in line:
                    match = re.search(r'(-?\d+\.?\d*)', line)
                    if match:
                        result["metrics"]["Sharpe Ratio"] = float(match.group(1))
                elif line.startswith("P&L Summary:") or line.startswith("Instrument Distribution:"):
                    break
        
        return result

    def compare_metrics(self, strattest_result: Dict, audit_result: Dict) -> Dict:
        """Compare Trading Block metrics between strattest and audit results"""
        comparison = {
            "run_id_match": strattest_result.get("run_id") == audit_result.get("run_id"),
            "dataset_match": strattest_result.get("dataset") == audit_result.get("dataset"),
            "trading_blocks_match": strattest_result.get("trading_blocks") == audit_result.get("trading_blocks"),
            "is_tb_run": not audit_result.get("is_legacy_run", True),
            "metrics_comparison": {},
            "discrepancies": [],
            "alignment_score": 0.0
        }
        
        # Compare Trading Block configuration
        strattest_blocks = strattest_result.get("trading_blocks", 0)
        audit_blocks = audit_result.get("trading_blocks", 0)
        strattest_bars = strattest_result.get("total_bars", 0)
        
        if strattest_blocks != audit_blocks:
            comparison["discrepancies"].append(
                f"Trading Block count mismatch: strattest={strattest_blocks} vs audit={audit_blocks}"
            )
        
        # Compare metrics with Trading Block priority
        strattest_metrics = strattest_result.get("metrics", {})
        audit_metrics = audit_result.get("metrics", {})
        
        # Define Trading Block metric mappings (strattest -> audit)
        tb_metric_mappings = {
            "Mean RPB": "Mean RPB",
            "ARB (Annualized)": "ARB (Annualized)", 
            "20TB Return": "20TB Return",
            "Aggregate Sharpe": "Aggregate Sharpe",
            "Total Fills": "Total Fills",
            "Consistency Score": "Consistency Score"
        }
        
        # Legacy metric mappings for backward compatibility
        legacy_metric_mappings = {
            "Monthly Projected Return": "Monthly Projected Return",
            "Sharpe Ratio": "Sharpe Ratio",
            "Maximum Drawdown": "Maximum Drawdown",
            "Total Return": "Total Return",
            "Average Daily Trades": "Average Daily Trades"
        }
        
        # Use Trading Block metrics if available, otherwise fall back to legacy
        metric_mappings = tb_metric_mappings if comparison["is_tb_run"] else legacy_metric_mappings
        
        matches = 0
        total_compared = 0
        
        for strattest_key, audit_key in metric_mappings.items():
            strattest_val = strattest_metrics.get(strattest_key, "MISSING")
            audit_val = audit_metrics.get(audit_key, "MISSING")
            
            total_compared += 1
            
            if strattest_val == "MISSING" or audit_val == "MISSING":
                comparison["metrics_comparison"][strattest_key] = {
                    "strattest": strattest_val,
                    "audit": audit_val,
                    "match": False,
                    "status": "MISSING_DATA"
                }
                comparison["discrepancies"].append(f"Missing data for {strattest_key}")
            elif isinstance(strattest_val, (int, float)) and isinstance(audit_val, (int, float)):
                # Numeric comparison with tolerance for Trading Block metrics
                tolerance = 1e-4 if "RPB" in strattest_key or "Return" in strattest_key else 1e-6
                match = abs(strattest_val - audit_val) < tolerance
                comparison["metrics_comparison"][strattest_key] = {
                    "strattest": strattest_val,
                    "audit": audit_val,
                    "match": match,
                    "difference": abs(strattest_val - audit_val) if not match else 0,
                    "status": "MATCH" if match else "MISMATCH"
                }
                if match:
                    matches += 1
                else:
                    comparison["discrepancies"].append(
                        f"{strattest_key}: strattest={strattest_val} vs audit={audit_val} (diff={abs(strattest_val - audit_val):.6f})"
                    )
            else:
                # String comparison
                match = str(strattest_val) == str(audit_val)
                comparison["metrics_comparison"][strattest_key] = {
                    "strattest": strattest_val,
                    "audit": audit_val,
                    "match": match,
                    "status": "MATCH" if match else "MISMATCH"
                }
                if match:
                    matches += 1
                else:
                    comparison["discrepancies"].append(f"{strattest_key}: values don't match")
        
        # Calculate alignment score
        comparison["alignment_score"] = (matches / total_compared) * 100 if total_compared > 0 else 0
        
        return comparison

    def run_position_history(self, run_id: str) -> Dict:
        """Run audit position-history for a run ID"""
        print(f"📊 Running position history for Run ID: {run_id}")
        
        try:
            cmd = ["./saudit", "position-history", "--run", run_id, "--limit", "50"]
            result = subprocess.run(
                cmd,
                cwd=self.project_root, 
                capture_output=True,
                text=True,
                timeout=60  # 1 minute timeout
            )
            
            if result.returncode != 0:
                print(f"❌ Position history failed for {run_id}: {result.stderr}")
                return {
                    "run_id": run_id,
                    "status": "FAILED",
                    "error": result.stderr
                }
            
            # Parse position history output
            parsed = self.parse_position_history_output(result.stdout, run_id)
            parsed["status"] = "SUCCESS"
            parsed["raw_output"] = result.stdout
            
            print(f"✅ Position history completed for {run_id}")
            return parsed
            
        except subprocess.TimeoutExpired:
            print(f"⏰ Position history timed out for {run_id}")
            return {
                "run_id": run_id,
                "status": "TIMEOUT",
                "error": "Position history timed out"
            }
        except Exception as e:
            print(f"💥 Position history crashed for {run_id}: {str(e)}")
            return {
                "run_id": run_id,
                "status": "CRASHED", 
                "error": str(e)
            }

    def parse_position_history_output(self, output: str, run_id: str) -> Dict:
        """Parse position history output"""
        result = {
            "run_id": run_id,
            "position_events": 0,
            "unique_symbols": set(),
            "final_positions": {},
            "has_trading_block_data": False
        }
        
        lines = output.split('\n')
        
        # Look for Trading Block header first
        for line in lines:
            if "Trading Blocks:" in line and "TB" in line:
                result["has_trading_block_data"] = True
                break
        
        # Count position events and extract symbols - look for table format
        in_table = False
        for line in lines:
            if "│ Symbol │" in line and "Side │" in line:
                in_table = True
                continue
            elif in_table and "│" in line:
                if line.strip().startswith("├") or line.strip().startswith("└"):
                    break  # End of table
                    
                parts = [p.strip() for p in line.split("│")]
                if len(parts) >= 3:
                    symbol = parts[1].strip()
                    side = parts[2].strip() if len(parts) > 2 else ""
                    
                    # Valid symbol check - should be like QQQ, SPY, etc.
                    if (symbol and symbol not in ["Symbol", "", "-", "Total"] and 
                        not symbol.startswith("=") and len(symbol) <= 10 and
                        side in ["BUY", "SELL"]):
                        result["unique_symbols"].add(symbol)
                        result["position_events"] += 1
        
        result["unique_symbols"] = list(result["unique_symbols"])
        return result

    def run_comprehensive_tests(self):
        """Run comprehensive tests for all strategies"""
        print("🚀 Starting Comprehensive Strategy Testing")
        print("=" * 60)
        
        start_time = datetime.now()
        
        for i, strategy in enumerate(self.strategies, 1):
            print(f"\n📊 [{i}/{len(self.strategies)}] Testing Strategy: {strategy}")
            print("-" * 40)
            
            # Run strattest
            strattest_result = self.run_strattest(strategy)
            self.results.append(strattest_result)
            
            # If strattest succeeded, run audit verification
            if strattest_result["status"] == "SUCCESS" and strattest_result.get("run_id") != "unknown":
                run_id = strattest_result["run_id"]
                
                # Run audit summarize
                audit_result = self.run_audit_summarize(run_id)
                self.audit_results.append(audit_result)
                
                # Run position history
                position_result = self.run_position_history(run_id)
                
                # Compare results
                comparison = self.compare_metrics(strattest_result, audit_result)
                
                print(f"📈 Trading Block Verification:")
                print(f"   Run ID Match: {comparison['run_id_match']}")
                print(f"   Trading Blocks Match: {comparison['trading_blocks_match']}")
                print(f"   Is TB Run: {comparison['is_tb_run']}")
                print(f"   Alignment Score: {comparison['alignment_score']:.1f}%")
                
                if comparison['discrepancies']:
                    print(f"   ⚠️  Discrepancies: {len(comparison['discrepancies'])}")
                    for disc in comparison['discrepancies'][:3]:  # Show first 3
                        print(f"      - {disc}")
                    if len(comparison['discrepancies']) > 3:
                        print(f"      ... and {len(comparison['discrepancies']) - 3} more")
                
                print(f"📊 Position History:")
                if position_result["status"] == "SUCCESS":
                    print(f"   Events: {position_result['position_events']}")
                    print(f"   Symbols: {position_result['unique_symbols']}")
                    print(f"   TB Data: {position_result['has_trading_block_data']}")
                else:
                    print(f"   Status: {position_result['status']}")
                
                # Store comparison and position data in strattest result
                strattest_result["audit_comparison"] = comparison
                strattest_result["position_history"] = position_result
            else:
                print(f"⚠️  Skipping audit for {strategy} due to strattest failure")
            
            # Small delay between tests
            time.sleep(1)
        
        end_time = datetime.now()
        duration = end_time - start_time
        
        print(f"\n🏁 Comprehensive Testing Completed")
        print(f"⏱️  Total Duration: {duration}")
        print(f"📊 Strategies Tested: {len(self.results)}")
        print(f"🔍 Audits Completed: {len(self.audit_results)}")

    def generate_report(self):
        """Generate comprehensive test results report"""
        print(f"\n📄 Generating Report: {self.report_file}")
        
        with open(self.report_file, 'w') as f:
            f.write("# Comprehensive Strategy Test Results\n")
            f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
            
            # Summary statistics
            f.write("## Summary Statistics\n\n")
            total_strategies = len(self.results)
            successful_strategies = sum(1 for r in self.results if r["status"] == "SUCCESS")
            failed_strategies = total_strategies - successful_strategies
            
            f.write(f"- **Total Strategies**: {total_strategies}\n")
            f.write(f"- **Successful Tests**: {successful_strategies}\n")
            f.write(f"- **Failed Tests**: {failed_strategies}\n")
            f.write(f"- **Success Rate**: {(successful_strategies/total_strategies)*100:.1f}%\n\n")
            
            # Detailed results
            f.write("## Detailed Test Results\n\n")
            
            for i, result in enumerate(self.results, 1):
                f.write(f"### {i}. {result['strategy']}\n\n")
                f.write(f"**Status**: {result['status']}\n\n")
                
                if result["status"] == "SUCCESS":
                    f.write(f"**Run ID**: `{result.get('run_id', 'unknown')}`\n\n")
                    f.write(f"**Dataset**: {result.get('dataset', 'unknown')}\n\n")
                    f.write(f"**Dataset Period**: {result.get('dataset_period', 'unknown')}\n\n")
                    
                    # Trading Block configuration
                    tb_count = result.get('trading_blocks', 0)
                    total_bars = result.get('total_bars', 0)
                    if tb_count > 0:
                        f.write(f"**Trading Blocks**: {tb_count} TB × 480 bars = {total_bars} total bars\n\n")
                        f.write(f"**Test Period**: {result.get('test_period', 'unknown')}\n\n")
                    else:
                        f.write(f"**Test Period**: {result.get('test_period', 'unknown')}\n\n")
                    
                    # Trading Block Metrics
                    f.write("**Metrics**:\n\n")
                    metrics = result.get("metrics", {})
                    
                    # Show Trading Block metrics first if available
                    tb_metrics = ["Mean RPB", "Std Dev RPB", "ARB (Annualized)", "20TB Return", "Aggregate Sharpe", "Total Fills", "Consistency Score"]
                    legacy_metrics = ["Monthly Projected Return", "Sharpe Ratio", "Maximum Drawdown", "Average Daily Trades"]
                    
                    # Display TB metrics with priority
                    for metric in tb_metrics:
                        if metric in metrics:
                            value = metrics[metric]
                            if isinstance(value, float):
                                if "RPB" in metric or "Return" in metric or "ARB" in metric:
                                    f.write(f"- {metric}: {value:.4f}%\n")
                                else:
                                    f.write(f"- {metric}: {value:.4f}\n")
                            else:
                                f.write(f"- {metric}: {value}\n")
                    
                    # Display legacy metrics if no TB metrics found
                    if not any(m in metrics for m in tb_metrics):
                        for metric in legacy_metrics:
                            if metric in metrics:
                                value = metrics[metric]
                                if isinstance(value, float):
                                    if "Return" in metric or "Drawdown" in metric:
                                        f.write(f"- {metric}: {value:.2f}%\n")
                                    else:
                                        f.write(f"- {metric}: {value:.2f}\n")
                                else:
                                    f.write(f"- {metric}: {value}\n")
                    
                    f.write("\n")
                    
                    # Trading Block Audit Validation
                    if "audit_comparison" in result:
                        comparison = result["audit_comparison"]
                        f.write("**Trading Block Audit Validation**:\n\n")
                        f.write(f"- Run ID Match: {comparison['run_id_match']}\n")
                        f.write(f"- Trading Blocks Match: {comparison['trading_blocks_match']}\n")
                        f.write(f"- Is TB Run: {comparison['is_tb_run']}\n")
                        f.write(f"- Alignment Score: {comparison['alignment_score']:.1f}%\n\n")
                        
                        # Metrics comparison
                        f.write("**Metrics Comparison**:\n\n")
                        for metric, comp in comparison["metrics_comparison"].items():
                            status_emoji = "✅" if comp["match"] else "❌"
                            f.write(f"- {metric}: {status_emoji} {comp['status']}\n")
                            if not comp["match"] and "difference" in comp:
                                f.write(f"  - Strattest: {comp['strattest']}\n")
                                f.write(f"  - Audit: {comp['audit']}\n")
                                f.write(f"  - Difference: {comp['difference']}\n")
                        f.write("\n")
                    
                    # Position History Verification
                    if "position_history" in result:
                        pos_hist = result["position_history"]
                        f.write("**Position History Verification**:\n\n")
                        f.write(f"- Status: {pos_hist['status']}\n")
                        if pos_hist["status"] == "SUCCESS":
                            f.write(f"- Position Events: {pos_hist['position_events']}\n")
                            f.write(f"- Unique Symbols: {pos_hist['unique_symbols']}\n")
                            f.write(f"- Has TB Data: {pos_hist['has_trading_block_data']}\n")
                        f.write("\n")
                else:
                    f.write(f"**Error**: {result.get('error', 'Unknown error')}\n\n")
                
                f.write("---\n\n")
            
            # Overall conclusions
            f.write("## Overall Conclusions\n\n")
            
            # Trading Block Analysis
            f.write("### Trading Block System Analysis\n\n")
            tb_runs = sum(1 for r in self.results 
                         if r.get("audit_comparison", {}).get("is_tb_run", False))
            legacy_runs = successful_strategies - tb_runs
            
            # Calculate average alignment scores
            alignment_scores = [r.get("audit_comparison", {}).get("alignment_score", 0) 
                              for r in self.results if r["status"] == "SUCCESS" and "audit_comparison" in r]
            avg_alignment = sum(alignment_scores) / len(alignment_scores) if alignment_scores else 0
            
            f.write(f"- **Trading Block Runs**: {tb_runs}/{successful_strategies} strategies\n")
            f.write(f"- **Legacy Runs**: {legacy_runs}/{successful_strategies} strategies\n")
            f.write(f"- **Average Alignment Score**: {avg_alignment:.1f}%\n")
            
            # Trading Block configuration consistency
            tb_matches = sum(1 for r in self.results 
                           if r.get("audit_comparison", {}).get("trading_blocks_match", False))
            f.write(f"- **Trading Block Configuration Match**: {tb_matches}/{successful_strategies} strategies\n\n")
            
            # Metrics consistency analysis
            f.write("### Metrics Consistency Analysis\n\n")
            perfect_alignment = sum(1 for score in alignment_scores if score >= 99.0)
            good_alignment = sum(1 for score in alignment_scores if 90.0 <= score < 99.0)
            poor_alignment = sum(1 for score in alignment_scores if score < 90.0)
            
            f.write(f"- **Perfect Alignment (≥99%)**: {perfect_alignment}/{successful_strategies} strategies\n")
            f.write(f"- **Good Alignment (90-99%)**: {good_alignment}/{successful_strategies} strategies\n")
            f.write(f"- **Poor Alignment (<90%)**: {poor_alignment}/{successful_strategies} strategies\n\n")
            
            # Position History Analysis
            f.write("### Position History Analysis\n\n")
            pos_success = sum(1 for r in self.results 
                            if r.get("position_history", {}).get("status") == "SUCCESS")
            total_events = sum(r.get("position_history", {}).get("position_events", 0) 
                             for r in self.results if r["status"] == "SUCCESS")
            
            f.write(f"- **Position History Success**: {pos_success}/{successful_strategies} strategies\n")
            f.write(f"- **Total Position Events**: {total_events}\n\n")
            
            # Trading Block Recommendations
            f.write("### Trading Block System Recommendations\n\n")
            if failed_strategies > 0:
                f.write(f"- **{failed_strategies} strategies failed** - investigate strategy-specific issues\n")
            if legacy_runs > 0:
                f.write(f"- **{legacy_runs} strategies using legacy metrics** - update to Trading Block system\n")
            if avg_alignment < 95.0:
                f.write(f"- **Low alignment score ({avg_alignment:.1f}%)** - investigate strattest vs audit discrepancies\n")
            if poor_alignment > 0:
                f.write(f"- **{poor_alignment} strategies with poor alignment** - review metric calculations\n")
            if tb_matches < successful_strategies:
                f.write(f"- **Trading Block config mismatches** - verify 10TB × 480bars configuration\n")
            if pos_success < successful_strategies:
                f.write(f"- **Position history issues** - check audit position recording\n")
            
            if (failed_strategies == 0 and tb_runs == successful_strategies and 
                avg_alignment >= 99.0 and perfect_alignment == successful_strategies):
                f.write("- **🎉 Perfect Trading Block System** - all strategies aligned and ready for production\n")
            elif failed_strategies == 0 and avg_alignment >= 95.0:
                f.write("- **✅ Trading Block System operational** - good alignment across strategies\n")
            else:
                f.write("- **⚠️  Trading Block System needs attention** - address alignment issues before production\n")
        
        print(f"✅ Report generated: {self.report_file}")

    def run(self):
        """Main execution method"""
        try:
            self.run_comprehensive_tests()
            self.generate_report()
            print(f"\n🎉 Comprehensive Strategy Testing Complete!")
            print(f"📄 Report available at: {self.report_file}")
        except KeyboardInterrupt:
            print(f"\n⚠️  Testing interrupted by user")
            print(f"📄 Partial report available at: {self.report_file}")
        except Exception as e:
            print(f"\n💥 Testing failed with error: {str(e)}")
            sys.exit(1)

def main():
    """Main entry point"""
    tester = ComprehensiveStrategyTester()
    tester.run()

if __name__ == "__main__":
    main()
