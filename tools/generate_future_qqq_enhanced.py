#!/usr/bin/env python3
"""
Enhanced Future QQQ Generator with Realistic OHLC Variations

This script generates future QQQ data in the exact same format as Polygon CSV files,
with realistic OHLC variations based on historical QQQ data analysis.

Features:
- Exact Polygon CSV format: timestamp,symbol,open,high,low,close,volume
- Realistic OHLC variations using Monte Carlo simulation
- 10 different market regime tracks (4 weeks each)
- Proper volume modeling
- Consistent with existing MarS-generated close prices
"""

import pandas as pd
import numpy as np
import json
from datetime import datetime, timedelta
import os
import sys

class EnhancedFutureQQQGenerator:
    def __init__(self, ohlc_model_path="tools/ohlc_model.json"):
        """Initialize with OHLC model parameters"""
        
        with open(ohlc_model_path, 'r') as f:
            self.ohlc_model = json.load(f)
        
        self.params = self.ohlc_model['parameters']
        print(f"📊 Loaded OHLC model from {ohlc_model_path}")
        
        # Market regime configurations
        self.regime_configs = {
            'normal': {
                'tracks': [1, 4, 7, 10],
                'volatility_multiplier': 1.0,
                'trend_strength': 0.0,
                'description': 'Normal market conditions'
            },
            'volatile': {
                'tracks': [2, 5, 8],
                'volatility_multiplier': 2.0,
                'trend_strength': 0.0,
                'description': 'High volatility market conditions'
            },
            'trending': {
                'tracks': [3, 6, 9],
                'volatility_multiplier': 1.2,
                'trend_strength': 0.3,
                'description': 'Strong trending market conditions'
            }
        }
    
    def generate_ohlc_for_bar(self, open_price, close_price, regime_multiplier=1.0):
        """Generate realistic OHLC for a single bar"""
        
        # Apply regime-specific volatility scaling
        vol_scale = regime_multiplier
        
        # Generate high-low spread (log-normal distribution)
        spread_mean = self.params['high_low_spread']['mean_pct'] * vol_scale
        spread_std = self.params['high_low_spread']['std_pct'] * vol_scale
        
        # Use log-normal for spread (always positive)
        spread_pct = np.random.lognormal(
            np.log(max(spread_mean, 0.001)),
            max(spread_std / max(spread_mean, 0.001), 0.1)
        ) / 100
        
        # Clamp spread
        spread_pct = np.clip(spread_pct, 
                           self.params['high_low_spread']['min_pct'] / 100,
                           self.params['high_low_spread']['max_pct'] / 100 * vol_scale)
        
        # Calculate base price and spread amount
        base_price = (open_price + close_price) / 2
        spread_amount = base_price * spread_pct
        
        # Generate high extension (how much high exceeds max(open, close))
        high_ext_pct = np.random.normal(
            self.params['high_extension']['vs_open_mean'] * vol_scale,
            self.params['high_extension']['vs_open_std'] * vol_scale
        ) / 100
        
        # Generate low extension (how much low is below min(open, close))
        low_ext_pct = np.random.normal(
            self.params['low_extension']['vs_open_mean'] * vol_scale,
            self.params['low_extension']['vs_open_std'] * vol_scale
        ) / 100
        
        # Calculate high and low
        max_oc = max(open_price, close_price)
        min_oc = min(open_price, close_price)
        
        high = max_oc + abs(high_ext_pct) * base_price
        low = min_oc - abs(low_ext_pct) * base_price
        
        # Ensure minimum spread
        if high - low < spread_amount:
            mid = (high + low) / 2
            high = mid + spread_amount / 2
            low = mid - spread_amount / 2
        
        # Final validation
        high = max(high, max_oc)
        low = min(low, min_oc)
        
        # Generate volume (log-normal distribution)
        volume_mean = self.params['volume']['mean']
        volume_std = self.params['volume']['std']
        
        volume = int(np.random.lognormal(
            np.log(volume_mean),
            volume_std / volume_mean
        ))
        
        # Clamp volume
        volume = np.clip(volume, 
                        int(self.params['volume']['min']),
                        int(self.params['volume']['max']))
        
        return {
            'open': round(open_price, 2),
            'high': round(high, 2),
            'low': round(low, 2), 
            'close': round(close_price, 2),
            'volume': volume
        }
    
    def load_existing_close_prices(self, track_id):
        """Load existing close prices from current future QQQ files"""
        
        current_file = f"data/future_qqq/future_qqq_track_{track_id:02d}.csv"
        
        if not os.path.exists(current_file):
            raise FileNotFoundError(f"Current future QQQ file not found: {current_file}")
        
        df = pd.read_csv(current_file)
        print(f"📈 Loaded {len(df)} existing close prices from track {track_id}")
        
        return df[['timestamp', 'close']].values
    
    def generate_track_with_ohlc(self, track_id, regime_type):
        """Generate a complete track with realistic OHLC variations"""
        
        print(f"\n🚀 Generating track {track_id} ({regime_type} regime)")
        
        # Load existing close prices and timestamps
        existing_data = self.load_existing_close_prices(track_id)
        
        # Get regime configuration
        regime_config = self.regime_configs[regime_type]
        vol_multiplier = regime_config['volatility_multiplier']
        
        # Generate OHLC data
        bars = []
        
        for i, (timestamp_str, close_price) in enumerate(existing_data):
            # Calculate open price
            if i == 0:
                # First bar: open = close (or small gap)
                gap_pct = np.random.normal(0, 0.05) / 100  # Small random gap
                open_price = close_price * (1 + gap_pct)
            else:
                # Subsequent bars: open near previous close with gap
                prev_close = existing_data[i-1][1]
                gap_pct = np.random.normal(0, 0.1) / 100  # Small gap
                open_price = prev_close * (1 + gap_pct)
            
            # Generate OHLC for this bar
            ohlc = self.generate_ohlc_for_bar(open_price, close_price, vol_multiplier)
            
            # Create bar in Polygon CSV format
            bar = {
                'timestamp': timestamp_str,
                'symbol': 'QQQ',
                'open': ohlc['open'],
                'high': ohlc['high'],
                'low': ohlc['low'],
                'close': ohlc['close'],
                'volume': ohlc['volume']
            }
            
            bars.append(bar)
        
        return pd.DataFrame(bars)
    
    def save_track(self, df, track_id):
        """Save track to CSV file in Polygon format"""
        
        output_file = f"data/future_qqq/future_qqq_track_{track_id:02d}.csv"
        
        # Ensure exact Polygon CSV format
        df_output = df[['timestamp', 'symbol', 'open', 'high', 'low', 'close', 'volume']].copy()
        
        # Save with proper formatting
        df_output.to_csv(output_file, index=False, float_format='%.2f')
        
        print(f"💾 Saved track {track_id} to {output_file}")
        
        # Verify the format
        with open(output_file, 'r') as f:
            header = f.readline().strip()
            first_data = f.readline().strip()
        
        print(f"📋 Header: {header}")
        print(f"📋 Sample: {first_data}")
        
        return output_file
    
    def generate_all_tracks(self):
        """Generate all 10 future QQQ tracks with proper OHLC variations"""
        
        print("🚀 Starting Enhanced Future QQQ Generation")
        print("=" * 60)
        
        generated_files = []
        
        # Generate tracks for each regime
        for regime_type, config in self.regime_configs.items():
            print(f"\n📊 Generating {regime_type.upper()} regime tracks: {config['tracks']}")
            
            for track_id in config['tracks']:
                try:
                    # Generate track with OHLC
                    df = self.generate_track_with_ohlc(track_id, regime_type)
                    
                    # Save track
                    output_file = self.save_track(df, track_id)
                    generated_files.append(output_file)
                    
                    # Quick validation
                    self.validate_track(df, track_id, regime_type)
                    
                except Exception as e:
                    print(f"❌ Error generating track {track_id}: {e}")
                    continue
        
        print(f"\n✅ Successfully generated {len(generated_files)} tracks")
        return generated_files
    
    def validate_track(self, df, track_id, regime_type):
        """Validate generated track data"""
        
        # Calculate statistics
        df['high_low_spread_pct'] = (df['high'] - df['low']) / df['close'] * 100
        df['return_pct'] = df['close'].pct_change() * 100
        
        stats = {
            'bars': len(df),
            'avg_spread_pct': df['high_low_spread_pct'].mean(),
            'avg_volume': df['volume'].mean(),
            'volatility': df['return_pct'].std(),
            'price_range': f"{df['close'].min():.2f} - {df['close'].max():.2f}"
        }
        
        print(f"✅ Track {track_id} ({regime_type}): {stats['bars']} bars, "
              f"spread={stats['avg_spread_pct']:.3f}%, vol={stats['volatility']:.3f}%, "
              f"avg_volume={stats['avg_volume']:,.0f}")

def main():
    """Main execution function"""
    
    # Check if OHLC model exists
    if not os.path.exists("tools/ohlc_model.json"):
        print("❌ OHLC model not found. Please run analyze_ohlc_patterns.py first.")
        return 1
    
    # Create generator
    generator = EnhancedFutureQQQGenerator()
    
    # Generate all tracks
    generated_files = generator.generate_all_tracks()
    
    print("\n🎉 Enhanced Future QQQ Generation Complete!")
    print("=" * 60)
    print(f"📁 Generated {len(generated_files)} files in data/future_qqq/")
    print("📋 Format: timestamp,symbol,open,high,low,close,volume (Polygon CSV format)")
    print("📊 Features: Realistic OHLC variations, proper volume modeling")
    print("🎯 Ready for trading system integration")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
