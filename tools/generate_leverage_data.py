#!/usr/bin/env python3
"""
Sentio Trading System - Leverage Data Generator

This script generates theoretical leverage data for TQQQ, SQQQ, and PSQ
based on the base QQQ data using accurate pricing models.

Usage:
    python3 generate_leverage_data.py [--input QQQ_RTH_NH.csv] [--output-dir data/equities/]
"""

import pandas as pd
import numpy as np
import argparse
import os
from pathlib import Path
from datetime import datetime, timedelta
import math

class LeverageDataGenerator:
    """Generate theoretical leverage data from base QQQ data"""
    
    def __init__(self, daily_decay_rate=0.0001, expense_ratio=0.0095):
        """
        Initialize the leverage data generator
        
        Args:
            daily_decay_rate: Daily rebalancing cost (default: 0.01%)
            expense_ratio: Annual expense ratio (default: 0.95%)
        """
        self.daily_decay_rate = daily_decay_rate
        self.expense_ratio = expense_ratio
        
        # Leverage specifications
        self.leverage_specs = {
            'TQQQ': {'factor': 3.0, 'inverse': False, 'description': '3x Long QQQ'},
            'SQQQ': {'factor': 3.0, 'inverse': True, 'description': '3x Short QQQ'},
            'PSQ': {'factor': 1.0, 'inverse': True, 'description': '1x Short QQQ'}
        }
    
    def calculate_leverage_price(self, prev_lev_price, qqq_daily_return, leverage_factor, is_inverse):
        """
        Calculate theoretical leverage price using daily return compounding
        
        CORRECTED MODEL: This now uses the accurate daily return compounding approach
        that correctly models the path-dependent nature of leveraged ETFs.
        
        Args:
            prev_lev_price: Previous leveraged ETF price
            qqq_daily_return: Daily return of base QQQ asset
            leverage_factor: Leverage multiplier (1.0 or 3.0)
            is_inverse: Whether this is an inverse instrument
            
        Returns:
            New theoretical leverage price
        """
        # Apply leverage factor to daily return
        leveraged_return = qqq_daily_return * leverage_factor
        
        # Apply inversion if necessary
        if is_inverse:
            leveraged_return = -leveraged_return
        
        # Apply daily costs (decay + expense ratio)
        daily_costs = self.daily_decay_rate + (self.expense_ratio / 252.0)
        
        # Calculate new price using compounding
        new_price = prev_lev_price * (1.0 + leveraged_return - daily_costs)
        
        return max(new_price, 0.01)  # Ensure positive price
    
    def generate_leverage_series(self, qqq_data, symbol):
        """
        Generate leverage series for a specific symbol using corrected daily return compounding
        
        CORRECTED MODEL: This now uses the accurate daily return compounding approach
        that correctly models the path-dependent nature of leveraged ETFs.
        
        Args:
            qqq_data: DataFrame with QQQ OHLCV data
            symbol: Target symbol (TQQQ, SQQQ, PSQ)
            
        Returns:
            DataFrame with leverage OHLCV data
        """
        if symbol not in self.leverage_specs:
            raise ValueError(f"Unsupported leverage symbol: {symbol}")
        
        spec = self.leverage_specs[symbol]
        leverage_factor = spec['factor']
        is_inverse = spec['inverse']
        
        print(f"Generating {symbol} data ({spec['description']}) using corrected daily return compounding...")
        
        # Create output dataframe
        leverage_data = qqq_data.copy()
        
        # Initialize starting price for leveraged ETF
        starting_price = 100.0
        if is_inverse:
            starting_price = 50.0  # Inverse ETFs typically start lower
        
        prev_lev_price = starting_price
        
        # Calculate theoretical prices using daily return compounding with progress tracking
        total_rows = len(qqq_data)
        print(f"Processing {total_rows:,} rows for {symbol}...")
        
        # Process in chunks for better performance
        chunk_size = 10000
        for chunk_start in range(1, total_rows, chunk_size):
            chunk_end = min(chunk_start + chunk_size, total_rows)
            
            # Show progress
            progress_pct = (chunk_start / total_rows) * 100
            print(f"  Progress: {progress_pct:.1f}% ({chunk_start:,}/{total_rows:,} rows)")
            
            # Process chunk
            for i in range(chunk_start, chunk_end):
                prev_qqq_close = qqq_data.iloc[i-1]['close']
                curr_qqq_close = qqq_data.iloc[i]['close']
                
                # Calculate daily return of base QQQ
                qqq_daily_return = (curr_qqq_close / prev_qqq_close) - 1.0
                
                # Calculate new leveraged price using compounding
                curr_lev_price = self.calculate_leverage_price(
                    prev_lev_price, qqq_daily_return, leverage_factor, is_inverse
                )
                
                # Calculate OHLC based on intraday movements
                curr_qqq_open = qqq_data.iloc[i]['open']
                curr_qqq_high = qqq_data.iloc[i]['high']
                curr_qqq_low = qqq_data.iloc[i]['low']
                
                # Calculate intraday movement ratios
                open_ratio = (curr_qqq_open - prev_qqq_close) / prev_qqq_close
                high_ratio = (curr_qqq_high - prev_qqq_close) / prev_qqq_close
                low_ratio = (curr_qqq_low - prev_qqq_close) / prev_qqq_close
                
                # Apply leverage factor to intraday movements
                lev_open_ratio = open_ratio * leverage_factor * (-1 if is_inverse else 1)
                lev_high_ratio = high_ratio * leverage_factor * (-1 if is_inverse else 1)
                lev_low_ratio = low_ratio * leverage_factor * (-1 if is_inverse else 1)
                
                # Calculate leveraged OHLC
                lev_open = prev_lev_price * (1.0 + lev_open_ratio)
                lev_high = prev_lev_price * (1.0 + lev_high_ratio)
                lev_low = prev_lev_price * (1.0 + lev_low_ratio)
                
                # For inverse ETFs, high and low are swapped
                if is_inverse:
                    lev_high, lev_low = lev_low, lev_high
                
                # Ensure OHLC relationships
                leverage_data.iloc[i, leverage_data.columns.get_loc('open')] = max(lev_open, 0.01)
                leverage_data.iloc[i, leverage_data.columns.get_loc('high')] = max(lev_high, lev_open, curr_lev_price, 0.01)
                leverage_data.iloc[i, leverage_data.columns.get_loc('low')] = max(min(lev_low, lev_open, curr_lev_price), 0.01)
                leverage_data.iloc[i, leverage_data.columns.get_loc('close')] = curr_lev_price
                
                # Adjust volume (typically lower for leverage instruments)
                volume_scaling = 0.3 if leverage_factor == 3.0 else 0.5
                leverage_data.iloc[i, leverage_data.columns.get_loc('volume')] = int(qqq_data.iloc[i]['volume'] * volume_scaling)
                
                prev_lev_price = curr_lev_price
        
        print(f"Generated {len(leverage_data)} bars for {symbol} using corrected daily return compounding")
        return leverage_data
    
    def generate_all_leverage_data(self, qqq_file_path, output_dir, max_rows=None):
        """
        Generate all leverage data files
        
        Args:
            qqq_file_path: Path to QQQ base data file
            output_dir: Directory to save leverage data files
            max_rows: Maximum number of rows to process (for testing)
        """
        print(f"Loading QQQ data from: {qqq_file_path}")
        
        # Load QQQ data
        try:
            qqq_data = pd.read_csv(qqq_file_path)
            original_rows = len(qqq_data)
            
            # Limit rows if specified (for testing)
            if max_rows and max_rows < original_rows:
                qqq_data = qqq_data.head(max_rows)
                print(f"⚠️  Limited to {max_rows:,} rows for testing (original: {original_rows:,})")
            
            print(f"Loaded {len(qqq_data):,} QQQ bars")
        except Exception as e:
            print(f"Error loading QQQ data: {e}")
            return False
        
        # Ensure output directory exists
        output_path = Path(output_dir)
        output_path.mkdir(parents=True, exist_ok=True)
        
        # Generate leverage data for each symbol
        for symbol in self.leverage_specs.keys():
            try:
                leverage_data = self.generate_leverage_series(qqq_data, symbol)
                
                # Save to file
                output_file = output_path / f"{symbol}_RTH_NH.csv"
                leverage_data.to_csv(output_file, index=False)
                
                print(f"Saved {symbol} data to: {output_file}")
                
                # Display sample data
                print(f"Sample {symbol} data:")
                print(leverage_data.head(3))
                print()
                
            except Exception as e:
                print(f"Error generating {symbol} data: {e}")
                return False
        
        print("✅ All leverage data files generated successfully!")
        return True

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description='Generate leverage data for Sentio Trading System')
    parser.add_argument('--input', default='data/equities/QQQ_RTH_NH.csv',
                       help='Path to QQQ base data file')
    parser.add_argument('--output-dir', default='data/equities/',
                       help='Directory to save leverage data files')
    parser.add_argument('--daily-decay', type=float, default=0.0001,
                       help='Daily decay rate for leverage instruments')
    parser.add_argument('--expense-ratio', type=float, default=0.0095,
                       help='Annual expense ratio')
    parser.add_argument('--max-rows', type=int, help='Maximum number of rows to process (for testing)')
    
    args = parser.parse_args()
    
    # Check if input file exists
    if not os.path.exists(args.input):
        print(f"❌ Error: Input file not found: {args.input}")
        print("Please ensure QQQ_RTH_NH.csv exists in the data/equities/ directory")
        return 1
    
    # Create generator
    generator = LeverageDataGenerator(
        daily_decay_rate=args.daily_decay,
        expense_ratio=args.expense_ratio
    )
    
    # Generate leverage data
    success = generator.generate_all_leverage_data(args.input, args.output_dir, args.max_rows)
    
    if success:
        print("\n🎯 Leverage Data Generation Complete!")
        print("Generated files:")
        print("  - TQQQ_RTH_NH.csv (3x Long QQQ)")
        print("  - SQQQ_RTH_NH.csv (3x Short QQQ)")
        print("  - PSQ_RTH_NH.csv (1x Short QQQ)")
        print("\nThese files can now be used for leverage trading in sentio_cli trade command.")
        return 0
    else:
        print("❌ Leverage data generation failed!")
        return 1

if __name__ == "__main__":
    exit(main())
