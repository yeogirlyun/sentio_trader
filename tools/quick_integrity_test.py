#!/usr/bin/env python3
"""
Quick integrity test for one strategy
"""

import subprocess
import re
import sys

def run_command(cmd):
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        return result.returncode == 0, result.stdout, result.stderr
    except Exception as e:
        return False, "", str(e)

def extract_metrics(output):
    # Remove ANSI color codes
    clean_output = re.sub(r'\x1b\[[0-9;]*m', '', output)
    
    metrics = {}
    
    # Extract fills
    fill_matches = re.findall(r'Fills=(\d+)', clean_output)
    if fill_matches:
        metrics['total_fills'] = sum(int(match) for match in fill_matches)
    
    # Extract cash and equity
    for line in clean_output.split('\n'):
        if 'Final Cash:' in line:
            numbers = re.findall(r'\d+\.\d+|\d+', line)
            if numbers:
                metrics['final_cash'] = float(numbers[0])
        elif 'Final Equity:' in line:
            numbers = re.findall(r'\d+\.\d+|\d+', line)
            if numbers:
                metrics['final_equity'] = float(numbers[0])
    
    # Extract positions
    position_section = False
    positions = {}
    for line in clean_output.split('\n'):
        if 'ACTIVE POSITIONS BREAKDOWN:' in line:
            position_section = True
            continue
        if position_section and '│' in line:
            if match := re.search(r'│\s*(\w+)\s*│\s*([+-]?\d+\.?\d*)\s*shares', line):
                symbol = match.group(1)
                qty = float(match.group(2))
                if abs(qty) > 1e-6:
                    positions[symbol] = qty
            elif '└' in line:
                break
    
    metrics['positions'] = positions
    return metrics

def main():
    strategy = sys.argv[1] if len(sys.argv) > 1 else "sigor"
    
    print(f"🎯 Testing {strategy} with 2 blocks...")
    
    # Run strattest
    success, stdout, stderr = run_command([
        "./sencli", "strattest", strategy, "--mode", "historical", "--blocks", "2"
    ])
    
    if not success:
        print(f"❌ StratTest failed: {stderr}")
        return 1
    
    metrics = extract_metrics(stdout)
    print(f"✅ StratTest: {metrics.get('total_fills', 0)} fills, "
          f"${metrics.get('final_cash', 0):.2f} cash, "
          f"${metrics.get('final_equity', 0):.2f} equity")
    print(f"   Positions: {metrics.get('positions', {})}")
    
    # Get latest run and check audit
    success, latest_output, stderr = run_command(["./saudit", "latest"])
    if not success:
        print(f"❌ Failed to get latest run: {stderr}")
        return 1
    
    run_id_match = re.search(r'Latest run:\s*(\w+)', latest_output)
    if not run_id_match:
        print("❌ Could not extract run ID")
        return 1
    
    run_id = run_id_match.group(1)
    print(f"🔍 Run ID: {run_id}")
    
    # Check integrity
    success, integrity_output, stderr = run_command([
        "./saudit", "integrity", "--run", run_id
    ])
    
    if not success:
        print(f"❌ Integrity check failed: {stderr}")
        return 1
    
    if "INTEGRITY CHECK PASSED" in integrity_output:
        print("✅ Integrity check passed")
        return 0
    else:
        print("❌ Integrity check failed")
        print(integrity_output[-500:])  # Show last 500 chars
        return 1

if __name__ == "__main__":
    sys.exit(main())
