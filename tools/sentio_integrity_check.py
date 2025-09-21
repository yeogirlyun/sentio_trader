#!/usr/bin/env python3
"""
Sentio Trading System - Comprehensive Integrity Check
=====================================================

This script performs a full end-to-end integrity check of the Sentio trading system:
1. Signal generation (strattest with sigor strategy)
2. Trade execution (order book generation)
3. Portfolio audit (position history analysis)
4. Anomaly detection and validation

Usage: python3 tools/sentio_integrity_check.py
"""

import subprocess
import sys
import os
import json
import re
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Tuple, Optional

class Colors:
    """ANSI color codes for terminal output"""
    RESET = '\033[0m'
    BOLD = '\033[1m'
    DIM = '\033[2m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'

class SentioIntegrityChecker:
    def __init__(self, project_root: str = None):
        self.project_root = Path(project_root) if project_root else Path.cwd()
        self.build_dir = self.project_root / "build"
        self.cli_path = self.build_dir / "sentio_cli"
        self.data_dir = self.project_root / "data"
        self.signals_dir = self.data_dir / "signals"
        self.trades_dir = self.data_dir / "trades"
        
        # Test parameters
        self.dataset = "data/equities/QQQ_RTH_NH.csv"
        self.strategy = "sigor"
        self.blocks = 20
        self.capital = 100000
        self.buy_threshold = 0.6
        self.sell_threshold = 0.4
        
        # Results tracking
        self.results = {
            'signal_generation': {'success': False, 'file': None, 'count': 0},
            'trade_execution': {'success': False, 'file': None, 'count': 0, 'run_id': None},
            'audit_report': {'success': False, 'summary': {}, 'anomalies': []},
            'overall_status': 'PENDING'
        }

    def log(self, message: str, level: str = 'INFO'):
        """Enhanced logging with colors and timestamps"""
        timestamp = datetime.now().strftime('%H:%M:%S')
        color_map = {
            'INFO': Colors.CYAN,
            'SUCCESS': Colors.GREEN,
            'WARNING': Colors.YELLOW,
            'ERROR': Colors.RED,
            'HEADER': Colors.BOLD + Colors.MAGENTA
        }
        color = color_map.get(level, Colors.RESET)
        print(f"{Colors.DIM}[{timestamp}]{Colors.RESET} {color}{message}{Colors.RESET}")

    def run_command(self, cmd: List[str], description: str) -> Tuple[bool, str, str]:
        """Execute command and return success status with output"""
        self.log(f"Running: {description}", 'INFO')
        self.log(f"Command: {' '.join(cmd)}", 'INFO')
        
        try:
            result = subprocess.run(
                cmd, 
                cwd=self.project_root,
                capture_output=True, 
                text=True, 
                timeout=300  # 5 minute timeout
            )
            
            success = result.returncode == 0
            if success:
                self.log(f"✅ {description} completed successfully", 'SUCCESS')
            else:
                self.log(f"❌ {description} failed with exit code {result.returncode}", 'ERROR')
                self.log(f"STDERR: {result.stderr}", 'ERROR')
            
            return success, result.stdout, result.stderr
            
        except subprocess.TimeoutExpired:
            self.log(f"❌ {description} timed out after 5 minutes", 'ERROR')
            return False, "", "Command timed out"
        except Exception as e:
            self.log(f"❌ {description} failed with exception: {e}", 'ERROR')
            return False, "", str(e)

    def check_prerequisites(self) -> bool:
        """Verify all required files and directories exist"""
        self.log("🔍 Checking prerequisites...", 'HEADER')
        
        checks = [
            (self.cli_path, "sentio_cli executable"),
            (self.project_root / self.dataset, "QQQ dataset"),
            (self.signals_dir, "signals directory"),
            (self.trades_dir, "trades directory")
        ]
        
        all_good = True
        for path, description in checks:
            if path.exists():
                self.log(f"✅ {description}: {path}", 'SUCCESS')
            else:
                self.log(f"❌ Missing {description}: {path}", 'ERROR')
                all_good = False
        
        return all_good

    def step1_generate_signals(self) -> bool:
        """Step 1: Generate trading signals using sigor strategy"""
        self.log("📊 STEP 1: Signal Generation", 'HEADER')
        
        cmd = [
            str(self.cli_path),
            "strattest",
            "--dataset", self.dataset,
            "--strategy", self.strategy,
            "--blocks", str(self.blocks)
        ]
        
        success, stdout, stderr = self.run_command(cmd, "Signal generation")
        
        if success:
            # Parse output to extract signal file info
            signal_file_match = re.search(r'Exported (\d+) signals to (.+\.jsonl)', stdout)
            if signal_file_match:
                count = int(signal_file_match.group(1))
                filepath = signal_file_match.group(2)
                
                self.results['signal_generation'] = {
                    'success': True,
                    'file': filepath,
                    'count': count
                }
                
                self.log(f"📈 Signal file generated successfully:", 'SUCCESS')
                self.log(f"   📁 File: {filepath}", 'SUCCESS')
                self.log(f"   📊 Count: {count:,} signals", 'SUCCESS')
                
                # Verify file exists and has content
                full_path = self.project_root / filepath
                if full_path.exists():
                    file_size = full_path.stat().st_size
                    self.log(f"   💾 Size: {file_size:,} bytes", 'SUCCESS')
                    return True
                else:
                    self.log(f"❌ Signal file not found at {full_path}", 'ERROR')
                    return False
            else:
                self.log("❌ Could not parse signal generation output", 'ERROR')
                return False
        
        return False

    def step2_execute_trades(self) -> bool:
        """Step 2: Execute trades and generate order book"""
        self.log("💰 STEP 2: Trade Execution", 'HEADER')
        
        cmd = [
            str(self.cli_path),
            "trade",
            "--signals", "latest",
            "--capital", str(self.capital),
            "--buy", str(self.buy_threshold),
            "--sell", str(self.sell_threshold)
        ]
        
        success, stdout, stderr = self.run_command(cmd, "Trade execution")
        
        if success:
            # Parse output to extract trade info
            trade_count_match = re.search(r'executed trades: (\d+)', stdout)
            run_id_match = re.search(r'Run ID: \x1b\[36m(\d+)\x1b\[0m', stdout)
            trade_file_match = re.search(r'Trades: (.+\.jsonl)', stdout)
            
            if trade_count_match and run_id_match and trade_file_match:
                count = int(trade_count_match.group(1))
                run_id = run_id_match.group(1)
                filepath = trade_file_match.group(1)
                
                self.results['trade_execution'] = {
                    'success': True,
                    'file': filepath,
                    'count': count,
                    'run_id': run_id
                }
                
                self.log(f"📋 Order book generated successfully:", 'SUCCESS')
                self.log(f"   📁 File: {filepath}", 'SUCCESS')
                self.log(f"   🔢 Run ID: {run_id}", 'SUCCESS')
                self.log(f"   📊 Executed trades: {count:,}", 'SUCCESS')
                
                # Verify file exists and has content
                full_path = self.project_root / filepath
                if full_path.exists():
                    file_size = full_path.stat().st_size
                    self.log(f"   💾 Size: {file_size:,} bytes", 'SUCCESS')
                    return True
                else:
                    self.log(f"❌ Trade file not found at {full_path}", 'ERROR')
                    return False
            else:
                self.log("❌ Could not parse trade execution output", 'ERROR')
                self.log(f"Debug - stdout: {stdout[:500]}...", 'ERROR')
                return False
        
        return False

    def step3_audit_portfolio(self) -> bool:
        """Step 3: Generate audit report and analyze results"""
        self.log("📈 STEP 3: Portfolio Audit", 'HEADER')
        
        cmd = [
            str(self.cli_path),
            "audit",
            "position-history",
            "--max", "50"
        ]
        
        success, stdout, stderr = self.run_command(cmd, "Portfolio audit")
        
        if success:
            self.results['audit_report']['success'] = True
            
            # Parse account performance summary
            summary = self.parse_account_summary(stdout)
            self.results['audit_report']['summary'] = summary
            
            # Display summary
            self.log("📊 Account Performance Summary:", 'SUCCESS')
            for key, value in summary.items():
                self.log(f"   {key}: {value}", 'SUCCESS')
            
            # Check for anomalies
            anomalies = self.detect_anomalies(summary, stdout)
            self.results['audit_report']['anomalies'] = anomalies
            
            if anomalies:
                self.log(f"⚠️  Detected {len(anomalies)} anomalies:", 'WARNING')
                for anomaly in anomalies:
                    self.log(f"   • {anomaly}", 'WARNING')
            else:
                self.log("✅ No anomalies detected in trading data", 'SUCCESS')
            
            return True
        
        return False

    def parse_account_summary(self, audit_output: str) -> Dict[str, str]:
        """Extract key metrics from audit output"""
        summary = {}
        
        # Extract performance metrics using regex (updated for new table format)
        patterns = {
            'Starting Equity': r'Starting Capital\s*│\s*\$\s*([\d,.-]+)',
            'Current Equity': r'Current Equity\s*│\s*\$\s*([\d,.-]+)',
            'Total Return': r'Total Return\s*│\s*([-+]?[\d.]+%)',
            'Total Trades': r'Total Trades\s*│\s*(\d+)',
            'Realized P&L': r'Realized P&L\s*│\s*\$\s*([-+]?[\d,.-]+)',
            'Unrealized P&L': r'Unrealized\s*│\s*\$\s*([-+]?[\d,.-]+)',
            'Cash Balance': r'Cash Balance\s*│\s*\$\s*([\d,.-]+)',
            'Position Value': r'Position Value\s*│\s*\$\s*([\d,.-]+)',
            'Open Positions': r'Open Pos\.\s*│\s*(\d+)'
        }
        
        for key, pattern in patterns.items():
            match = re.search(pattern, audit_output)
            if match:
                summary[key] = match.group(1)
            else:
                summary[key] = 'N/A'
        
        return summary

    def detect_anomalies(self, summary: Dict[str, str], audit_output: str) -> List[str]:
        """Detect potential anomalies in trading data"""
        anomalies = []
        
        try:
            # Check for negative cash balance
            cash_balance = float(summary.get('Cash Balance', '0').replace(',', '').replace('$', ''))
            if cash_balance < -0.01:  # Allow for small floating point errors
                anomalies.append(f"Negative cash balance: ${cash_balance:,.2f}")
            
            # Check for unrealistic returns
            total_return_str = summary.get('Total Return', '0%').replace('%', '')
            if total_return_str != 'N/A':
                total_return = float(total_return_str)
                if abs(total_return) > 50:  # More than 50% return seems suspicious for short test
                    anomalies.append(f"Extreme return: {total_return}%")
            
            # Check for zero trades
            total_trades = int(summary.get('Total Trades', '0'))
            if total_trades == 0:
                anomalies.append("No trades executed")
            
            # Check for equity consistency
            starting_equity = float(summary.get('Starting Equity', '0').replace(',', ''))
            current_equity = float(summary.get('Current Equity', '0').replace(',', ''))
            cash_balance = float(summary.get('Cash Balance', '0').replace(',', ''))
            position_value = float(summary.get('Position Value', '0').replace(',', ''))
            
            calculated_equity = cash_balance + position_value
            if abs(calculated_equity - current_equity) > 0.01:
                anomalies.append(f"Equity mismatch: calculated={calculated_equity:.2f}, reported={current_equity:.2f}")
            
            # Check for symbol corruption in trade history
            if '2025-08-' in audit_output or '2022-09-' in audit_output:
                # Look for timestamp patterns in symbol column
                timestamp_in_symbol = re.search(r'\|\s*20\d{2}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}', audit_output)
                if timestamp_in_symbol:
                    anomalies.append("Symbol column contains timestamps instead of ticker symbols")
            
            # Check for all-zero quantities
            zero_quantity_pattern = r'\|\s*\+0\.000\s*\|'
            zero_quantities = len(re.findall(zero_quantity_pattern, audit_output))
            total_trade_lines = len(re.findall(r'\|\s*\d{2}/\d{2}\s+\d{2}:\d{2}:\d{2}', audit_output))
            
            if total_trade_lines > 0 and zero_quantities == total_trade_lines:
                anomalies.append("All trade quantities are zero")
            
        except (ValueError, TypeError) as e:
            anomalies.append(f"Error parsing summary data: {e}")
        
        return anomalies

    def generate_final_report(self):
        """Generate comprehensive final report"""
        self.log("📋 FINAL INTEGRITY CHECK REPORT", 'HEADER')
        self.log("=" * 60, 'HEADER')
        
        # Overall status
        all_steps_passed = (
            self.results['signal_generation']['success'] and
            self.results['trade_execution']['success'] and
            self.results['audit_report']['success'] and
            len(self.results['audit_report']['anomalies']) == 0
        )
        
        self.results['overall_status'] = 'PASSED' if all_steps_passed else 'FAILED'
        
        status_color = 'SUCCESS' if all_steps_passed else 'ERROR'
        status_icon = '✅' if all_steps_passed else '❌'
        
        self.log(f"{status_icon} OVERALL STATUS: {self.results['overall_status']}", status_color)
        self.log("", 'INFO')
        
        # Step-by-step results
        self.log("📊 STEP RESULTS:", 'HEADER')
        
        # Signal Generation
        sig_result = self.results['signal_generation']
        if sig_result['success']:
            self.log(f"✅ Signal Generation: {sig_result['count']:,} signals → {sig_result['file']}", 'SUCCESS')
        else:
            self.log("❌ Signal Generation: FAILED", 'ERROR')
        
        # Trade Execution
        trade_result = self.results['trade_execution']
        if trade_result['success']:
            self.log(f"✅ Trade Execution: {trade_result['count']:,} trades → {trade_result['file']} (Run: {trade_result['run_id']})", 'SUCCESS')
        else:
            self.log("❌ Trade Execution: FAILED", 'ERROR')
        
        # Portfolio Audit
        audit_result = self.results['audit_report']
        if audit_result['success']:
            anomaly_count = len(audit_result['anomalies'])
            if anomaly_count == 0:
                self.log("✅ Portfolio Audit: PASSED (No anomalies)", 'SUCCESS')
            else:
                self.log(f"⚠️  Portfolio Audit: {anomaly_count} anomalies detected", 'WARNING')
        else:
            self.log("❌ Portfolio Audit: FAILED", 'ERROR')
        
        self.log("", 'INFO')
        
        # Account Summary
        if audit_result['success'] and audit_result['summary']:
            self.log("💰 ACCOUNT SUMMARY:", 'HEADER')
            summary = audit_result['summary']
            for key, value in summary.items():
                self.log(f"   {key}: {value}", 'INFO')
            self.log("", 'INFO')
        
        # Anomalies
        if audit_result['anomalies']:
            self.log("⚠️  DETECTED ANOMALIES:", 'WARNING')
            for i, anomaly in enumerate(audit_result['anomalies'], 1):
                self.log(f"   {i}. {anomaly}", 'WARNING')
            self.log("", 'INFO')
        
        # Conclusion
        if all_steps_passed:
            self.log("🎉 CONCLUSION: Sentio trading system integrity check PASSED", 'SUCCESS')
            self.log("   All components are functioning correctly with no anomalies detected.", 'SUCCESS')
        else:
            self.log("🚨 CONCLUSION: Sentio trading system integrity check FAILED", 'ERROR')
            self.log("   Please review the errors and anomalies above.", 'ERROR')
        
        self.log("=" * 60, 'HEADER')

    def run_full_check(self) -> bool:
        """Execute complete integrity check pipeline"""
        start_time = datetime.now()
        
        self.log("🚀 Starting Sentio Trading System Integrity Check", 'HEADER')
        self.log(f"⏰ Start time: {start_time.strftime('%Y-%m-%d %H:%M:%S')}", 'INFO')
        self.log(f"📁 Project root: {self.project_root}", 'INFO')
        self.log("", 'INFO')
        
        # Prerequisites
        if not self.check_prerequisites():
            self.log("❌ Prerequisites check failed. Aborting.", 'ERROR')
            return False
        
        self.log("", 'INFO')
        
        # Step 1: Signal Generation
        if not self.step1_generate_signals():
            self.log("❌ Signal generation failed. Aborting pipeline.", 'ERROR')
            self.generate_final_report()
            return False
        
        self.log("", 'INFO')
        
        # Step 2: Trade Execution
        if not self.step2_execute_trades():
            self.log("❌ Trade execution failed. Aborting pipeline.", 'ERROR')
            self.generate_final_report()
            return False
        
        self.log("", 'INFO')
        
        # Step 3: Portfolio Audit
        if not self.step3_audit_portfolio():
            self.log("❌ Portfolio audit failed. Aborting pipeline.", 'ERROR')
            self.generate_final_report()
            return False
        
        self.log("", 'INFO')
        
        # Final Report
        end_time = datetime.now()
        duration = end_time - start_time
        
        self.log(f"⏰ End time: {end_time.strftime('%Y-%m-%d %H:%M:%S')}", 'INFO')
        self.log(f"⏱️  Duration: {duration.total_seconds():.1f} seconds", 'INFO')
        self.log("", 'INFO')
        
        self.generate_final_report()
        
        return self.results['overall_status'] == 'PASSED'


def main():
    """Main entry point"""
    print(f"{Colors.BOLD}{Colors.CYAN}")
    print("╔══════════════════════════════════════════════════════════════╗")
    print("║              SENTIO TRADING SYSTEM INTEGRITY CHECK          ║")
    print("║                                                              ║")
    print("║  Comprehensive end-to-end validation of:                    ║")
    print("║  • Signal generation (sigor strategy)                       ║")
    print("║  • Trade execution & order book creation                    ║")
    print("║  • Portfolio audit & anomaly detection                      ║")
    print("╚══════════════════════════════════════════════════════════════╝")
    print(f"{Colors.RESET}")
    print()
    
    # Initialize checker
    checker = SentioIntegrityChecker()
    
    # Run full check
    success = checker.run_full_check()
    
    # Exit with appropriate code
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
