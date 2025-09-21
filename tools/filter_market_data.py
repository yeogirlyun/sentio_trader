#!/usr/bin/env python3
"""
Market Data Filter for NH (No Holidays)

This script filters raw market data to exclude:
- US market holidays
- All trading hours data is kept (pre-market, regular, after-hours)
"""

import pandas as pd
import pandas_market_calendars as mcal
from datetime import datetime, time
import argparse
import sys
from pathlib import Path

# RTH filtering removed - keeping all trading hours data

def filter_nh(input_file, output_file, symbol):
    """
    Filter market data to exclude holidays only
    
    Args:
        input_file: Path to input CSV file
        output_file: Path to output CSV file  
        symbol: Symbol name for the data
    """
    print(f"Processing {symbol} data from {input_file}...")
    
    # Read the CSV file
    try:
        df = pd.read_csv(input_file)
        print(f"  Loaded {len(df)} bars")
    except Exception as e:
        print(f"  Error reading {input_file}: {e}")
        return False
    
    if df.empty:
        print(f"  No data in {input_file}")
        return False
    
    # Convert timestamp to datetime
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    df.set_index('timestamp', inplace=True)
    
    # Keep data in UTC - no timezone conversion needed
    
    # RTH filtering removed - keeping all trading hours data
    df_filtered = df.copy()
    print(f"  Keeping all trading hours data: {len(df)} bars")
    
    # Filter out US market holidays
    print(f"  Filtering out US market holidays...")
    nyse = mcal.get_calendar('NYSE')
    holidays = nyse.holidays().holidays
    
    # Get unique dates from the data
    data_dates = df_filtered.index.normalize().unique()
    holiday_dates = set(holidays)
    
    # Filter out holiday dates
    df_clean = df_filtered[~df_filtered.index.normalize().isin(holiday_dates)]
    print(f"  Holiday filtering: {len(df_filtered)} -> {len(df_clean)} bars")
    
    if len(df_clean) == 0:
        print(f"  WARNING: No data remaining after filtering!")
        return False
    
    # Add required columns for C++ backtester
    df_clean['ts_utc'] = df_clean.index.strftime('%Y-%m-%dT%H:%M:%S%z').str.replace(r'([+-])(\d{2})(\d{2})', r'\1\2:\3', regex=True)
    df_clean['ts_nyt_epoch'] = df_clean.index.astype('int64') // 10**9
    
    # Select and order columns to match C++ struct
    output_columns = ['ts_utc', 'ts_nyt_epoch', 'open', 'high', 'low', 'close', 'volume']
    df_output = df_clean[output_columns].copy()
    
    # Save to CSV
    df_output.to_csv(output_file, index=False)
    print(f"  Saved {len(df_output)} bars to {output_file}")
    
    # Show date range
    if len(df_output) > 0:
        start_date = df_output['ts_utc'].iloc[0]
        end_date = df_output['ts_utc'].iloc[-1]
        print(f"  Date range: {start_date} to {end_date}")
    
    return True

def main():
    parser = argparse.ArgumentParser(description="Filter market data for RTH and NH")
    parser.add_argument('input_file', help="Input CSV file")
    parser.add_argument('output_file', help="Output CSV file")
    parser.add_argument('--symbol', help="Symbol name (default: extracted from filename)")
    
    args = parser.parse_args()
    
    # Extract symbol from filename if not provided
    if not args.symbol:
        symbol = Path(args.input_file).stem
    else:
        symbol = args.symbol
    
    # Create output directory if it doesn't exist
    Path(args.output_file).parent.mkdir(parents=True, exist_ok=True)
    
    # Filter the data
    success = filter_nh(args.input_file, args.output_file, symbol)
    
    if success:
        print(f"✅ Successfully filtered {symbol} data")
        return 0
    else:
        print(f"❌ Failed to filter {symbol} data")
        return 1

if __name__ == "__main__":
    sys.exit(main())
