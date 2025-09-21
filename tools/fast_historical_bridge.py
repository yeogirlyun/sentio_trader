#!/usr/bin/env python3
"""
Fast Historical Bridge - Optimized for speed without MarS complexity

This generates realistic market data using historical patterns but without
the overhead of MarS simulation engine.
"""

import pandas as pd
import numpy as np
import json
from datetime import datetime, timedelta
from typing import List, Dict, Any
import argparse
import pytz

def load_historical_data(filepath: str, recent_days: int = 30) -> pd.DataFrame:
    """Load and process historical data efficiently."""
    df = pd.read_csv(filepath)
    
    # Handle different timestamp formats
    timestamp_col = 'timestamp' if 'timestamp' in df.columns else 'ts_utc'
    df['timestamp'] = pd.to_datetime(df[timestamp_col], utc=True)
    
    # Use only recent data for faster processing
    if len(df) > recent_days * 390:  # ~390 bars per day
        df = df.tail(recent_days * 390)
        # Note: Debug print removed for quiet mode compatibility
    
    return df

def analyze_historical_patterns(df: pd.DataFrame) -> Dict[str, Any]:
    """Analyze historical patterns for realistic generation."""
    patterns = {}
    
    # Price statistics
    returns = np.diff(np.log(df['close']))
    patterns['mean_return'] = np.mean(returns)
    patterns['volatility'] = np.std(returns)
    patterns['price_trend'] = (df['close'].iloc[-1] - df['close'].iloc[0]) / len(df)
    
    # Volume statistics
    patterns['mean_volume'] = df['volume'].mean()
    patterns['volume_std'] = df['volume'].std()
    patterns['volume_trend'] = (df['volume'].iloc[-1] - df['volume'].iloc[0]) / len(df)
    
    # Intraday patterns
    df['hour'] = df['timestamp'].dt.hour
    hourly_stats = df.groupby('hour').agg({
        'volume': 'mean',
        'close': lambda x: np.std(np.diff(np.log(x)))
    }).reset_index()
    
    patterns['hourly_volume_multipliers'] = {}
    patterns['hourly_volatility_multipliers'] = {}
    
    for _, row in hourly_stats.iterrows():
        hour = int(row['hour'])
        patterns['hourly_volume_multipliers'][hour] = row['volume'] / patterns['mean_volume']
        patterns['hourly_volatility_multipliers'][hour] = row['close'] / patterns['volatility']
    
    # Fill missing hours
    for hour in range(24):
        if hour not in patterns['hourly_volume_multipliers']:
            patterns['hourly_volume_multipliers'][hour] = 1.0
            patterns['hourly_volatility_multipliers'][hour] = 1.0
    
    return patterns

def generate_realistic_bars(
    patterns: Dict[str, Any],
    start_price: float,
    duration_minutes: int,
    bar_interval_seconds: int = 60,
    symbol: str = "QQQ"
) -> List[Dict[str, Any]]:
    """Generate realistic market bars using historical patterns."""
    
    bars = []
    current_price = start_price
    
    # Always start from today's market open time (9:30 AM ET)
    et_tz = pytz.timezone('US/Eastern')
    today_et = datetime.now(et_tz)
    
    # Market open time (9:30 AM ET) - always use today's open
    current_time = today_et.replace(hour=9, minute=30, second=0, microsecond=0)
    # Note: Debug print removed for quiet mode compatibility
    
    # Convert to UTC for consistent timestamp generation
    current_time = current_time.astimezone(pytz.UTC)
    
    num_bars = duration_minutes * 60 // bar_interval_seconds
    
    for i in range(num_bars):
        # Apply time-of-day patterns
        hour = current_time.hour
        volume_multiplier = patterns['hourly_volume_multipliers'].get(hour, 1.0)
        volatility_multiplier = patterns['hourly_volatility_multipliers'].get(hour, 1.0)
        
        # Generate realistic price movement
        price_change = np.random.normal(
            patterns['mean_return'], 
            patterns['volatility'] * volatility_multiplier
        )
        current_price *= (1 + price_change)
        
        # Generate OHLC
        volatility = patterns['volatility'] * volatility_multiplier * current_price
        high_price = current_price + np.random.exponential(volatility * 0.5)
        low_price = current_price - np.random.exponential(volatility * 0.5)
        open_price = current_price + np.random.normal(0, volatility * 0.1)
        close_price = current_price
        
        # Generate realistic volume
        base_volume = patterns['mean_volume'] * volume_multiplier
        volume = int(np.random.lognormal(np.log(base_volume), 0.3))
        volume = max(1000, min(volume, 1000000))  # Reasonable bounds
        
        bar = {
            "timestamp": int(current_time.timestamp()),
            "open": round(open_price, 2),
            "high": round(high_price, 2),
            "low": round(low_price, 2),
            "close": round(close_price, 2),
            "volume": volume,
            "symbol": symbol
        }
        
        bars.append(bar)
        current_time += timedelta(seconds=bar_interval_seconds)
    
    return bars

def main():
    parser = argparse.ArgumentParser(description="Fast Historical Bridge for Market Data Generation")
    parser.add_argument("--symbol", default="QQQ", help="Symbol to simulate")
    parser.add_argument("--historical-data", required=True, help="Path to historical CSV data file")
    parser.add_argument("--continuation-minutes", type=int, default=60, help="Minutes to generate")
    parser.add_argument("--recent-days", type=int, default=30, help="Days of recent data to use")
    parser.add_argument("--output", default="fast_historical_data.json", help="Output filename")
    parser.add_argument("--format", default="json", choices=["json", "csv"], help="Output format")
    parser.add_argument("--quiet", action="store_true", help="Suppress debug output")
    
    args = parser.parse_args()
    
    if not args.quiet:
        print(f"🚀 Fast Historical Bridge - {args.symbol}")
        print(f"📊 Historical data: {args.historical_data}")
        print(f"⏱️  Duration: {args.continuation_minutes} minutes")
    
    # Load and analyze historical data
    if not args.quiet:
        print("📈 Loading historical data...")
    df = load_historical_data(args.historical_data, args.recent_days)
    
    if not args.quiet:
        print("🔍 Analyzing historical patterns...")
    patterns = analyze_historical_patterns(df)
    
    # Generate realistic data
    if not args.quiet:
        print("🎲 Generating realistic market data...")
    start_price = df['close'].iloc[-1]
    bars = generate_realistic_bars(
        patterns=patterns,
        start_price=start_price,
        duration_minutes=args.continuation_minutes,
        symbol=args.symbol
    )
    
    # Export data
    if args.format == "csv":
        df_output = pd.DataFrame(bars)
        df_output.to_csv(args.output, index=False)
    else:
        with open(args.output, 'w') as f:
            json.dump(bars, f, indent=2)
    
    if not args.quiet:
        print(f"✅ Generated {len(bars)} bars")
        print(f"📈 Price range: ${min(bar['low'] for bar in bars):.2f} - ${max(bar['high'] for bar in bars):.2f}")
        print(f"📊 Volume range: {min(bar['volume'] for bar in bars):,} - {max(bar['volume'] for bar in bars):,}")
        
        # Show time range in Eastern Time for clarity
        if bars:
            start_time = datetime.fromtimestamp(bars[0]['timestamp'], tz=pytz.UTC)
            end_time = datetime.fromtimestamp(bars[-1]['timestamp'], tz=pytz.UTC)
            start_time_et = start_time.astimezone(pytz.timezone('US/Eastern'))
            end_time_et = end_time.astimezone(pytz.timezone('US/Eastern'))
            print(f"⏰ Time range: {start_time_et.strftime('%Y-%m-%d %H:%M:%S %Z')} to {end_time_et.strftime('%Y-%m-%d %H:%M:%S %Z')}")
        
        print(f"💾 Saved to: {args.output}")

if __name__ == "__main__":
    main()
