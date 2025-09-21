#!/usr/bin/env python3
"""
Standalone MarS-Inspired Future QQQ Generator

Generates realistic QQQ data with proper minute-by-minute price variations
for three market sets: Normal, Volatile, and Bear markets (100 days each).

Features:
- MarS-inspired market microstructure simulation
- 100 trading days per market set (300 total days) for 2026
- Proper minute-by-minute price variations (no identical consecutive closes)
- Realistic volume patterns and market regimes
- RTH + Extended Hours (Night + After Hours) coverage
"""

import pandas as pd
import numpy as np
import json
import sys
import os
from datetime import datetime, timedelta, time
from pathlib import Path

class MarsInspiredQQQGenerator:
    def __init__(self):
        """Initialize MarS-inspired QQQ generator"""
        
        # 2026 trading calendar
        self.start_date = datetime(2026, 1, 5)  # First Monday of 2026
        self.trading_days = 100  # 100 trading days per market set
        
        # Market hours (Eastern Time)
        self.rth_start = time(9, 30)   # Regular Trading Hours start
        self.rth_end = time(16, 0)     # Regular Trading Hours end
        self.nh_start = time(4, 0)     # Night Hours start (4 AM)
        self.nh_end = time(9, 30)      # Night Hours end (9:30 AM)
        self.ah_start = time(16, 0)    # After Hours start (4 PM)
        self.ah_end = time(20, 0)      # After Hours end (8 PM)
        
        # QQQ parameters
        self.base_price = 400.0  # Starting QQQ price for 2026
        self.daily_volatility = 0.015  # 1.5% daily volatility
        self.tick_size = 0.01
        
        # Market regimes for three distinct sets
        self.market_regimes = {
            'normal': {
                'tracks': [1, 2, 3],  # 3 tracks for normal market
                'volatility_multiplier': 1.0,
                'trend_strength': 0.0,
                'mean_reversion': 0.8,
                'daily_drift': 0.0005,  # +0.05% per day
                'volatility_clustering': 0.3,
                'description': 'Normal market - steady growth with typical volatility'
            },
            'volatile': {
                'tracks': [4, 5, 6],  # 3 tracks for volatile market
                'volatility_multiplier': 2.8,
                'trend_strength': 0.15,
                'mean_reversion': 0.4,
                'daily_drift': 0.0,  # No consistent trend
                'volatility_clustering': 0.7,
                'description': 'Volatile market - high volatility with frequent reversals'
            },
            'bear': {
                'tracks': [7, 8, 9],  # 3 tracks for bear market
                'volatility_multiplier': 1.5,
                'trend_strength': -0.2,  # Mild downward trend
                'mean_reversion': 0.4,
                'daily_drift': -0.001,  # -0.1% per day
                'volatility_clustering': 0.4,
                'description': 'Bear market - sustained downward trend with elevated volatility'
            }
        }
        
        print(f"📅 Generating 3 market sets × {self.trading_days} trading days = {3 * self.trading_days} total days")
        print(f"🗓️  Starting from {self.start_date.strftime('%Y-%m-%d')} (2026)")
    
    def is_trading_day(self, date):
        """Check if date is a trading day (Monday-Friday, excluding major holidays)"""
        if date.weekday() >= 5:  # Saturday or Sunday
            return False
        
        # Major holidays in 2026 (approximate)
        holidays_2026 = [
            datetime(2026, 1, 1),   # New Year's Day
            datetime(2026, 1, 19),  # MLK Day
            datetime(2026, 2, 16),  # Presidents Day
            datetime(2026, 4, 3),   # Good Friday
            datetime(2026, 5, 25),  # Memorial Day
            datetime(2026, 7, 3),   # Independence Day (observed)
            datetime(2026, 9, 7),   # Labor Day
            datetime(2026, 11, 26), # Thanksgiving
            datetime(2026, 12, 25), # Christmas
        ]
        
        return date.date() not in [h.date() for h in holidays_2026]
    
    def generate_trading_dates(self):
        """Generate list of trading dates for 2026"""
        trading_dates = []
        current_date = self.start_date
        
        while len(trading_dates) < self.trading_days:
            if self.is_trading_day(current_date):
                trading_dates.append(current_date)
            current_date += timedelta(days=1)
        
        return trading_dates
    
    def generate_minute_timestamps(self, date):
        """Generate minute-by-minute timestamps for a trading day (NH + RTH + AH)"""
        timestamps = []
        
        # Night Hours: 4:00 AM - 9:30 AM (330 minutes)
        nh_start_dt = datetime.combine(date, self.nh_start)
        nh_end_dt = datetime.combine(date, self.nh_end)
        current = nh_start_dt
        while current < nh_end_dt:
            timestamps.append(current)
            current += timedelta(minutes=1)
        
        # Regular Trading Hours: 9:30 AM - 4:00 PM (390 minutes)
        rth_start_dt = datetime.combine(date, self.rth_start)
        rth_end_dt = datetime.combine(date, self.rth_end)
        current = rth_start_dt
        while current < rth_end_dt:
            timestamps.append(current)
            current += timedelta(minutes=1)
        
        # After Hours: 4:00 PM - 8:00 PM (240 minutes)
        ah_start_dt = datetime.combine(date, self.ah_start)
        ah_end_dt = datetime.combine(date, self.ah_end)
        current = ah_start_dt
        while current < ah_end_dt:
            timestamps.append(current)
            current += timedelta(minutes=1)
        
        return timestamps  # Total: 960 minutes per day
    
    def simulate_mars_inspired_prices(self, num_minutes, regime_config, base_price):
        """MarS-inspired market microstructure simulation"""
        
        # Extract regime parameters
        vol_multiplier = regime_config['volatility_multiplier']
        trend_strength = regime_config['trend_strength']
        mean_reversion = regime_config['mean_reversion']
        daily_drift = regime_config['daily_drift']
        vol_clustering = regime_config['volatility_clustering']
        
        # Initialize arrays
        prices = np.zeros(num_minutes)
        volatilities = np.zeros(num_minutes)
        prices[0] = base_price
        
        # Base minute-level volatility
        base_minute_vol = self.daily_volatility * vol_multiplier / np.sqrt(390)
        volatilities[0] = base_minute_vol
        
        # Daily drift converted to minute-level
        minute_drift = daily_drift / (24 * 60)
        
        # Market microstructure simulation
        for i in range(1, num_minutes):
            # 1. Volatility clustering (GARCH-like)
            prev_return = abs(prices[i-1] / prices[i-2] - 1) if i > 1 else 0
            vol_shock = vol_clustering * prev_return + (1 - vol_clustering) * base_minute_vol
            volatilities[i] = max(base_minute_vol * 0.5, min(base_minute_vol * 3, vol_shock))
            
            # 2. Base random walk
            random_shock = np.random.normal(0, volatilities[i])
            
            # 3. Trend component (regime-specific)
            if trend_strength < 0:  # Bear market
                # Persistent downward pressure with occasional relief rallies
                trend_component = trend_strength * volatilities[i] * (0.8 + 0.4 * np.random.random())
                if np.random.random() < 0.05:  # 5% chance of relief rally
                    trend_component *= -0.5
            elif trend_strength > 0:  # Trending market
                # Momentum with occasional pullbacks
                trend_component = trend_strength * volatilities[i] * np.sign(np.random.normal())
                if np.random.random() < 0.1:  # 10% chance of pullback
                    trend_component *= -0.3
            else:  # Normal market
                trend_component = 0
            
            # 4. Daily drift
            drift_component = minute_drift
            
            # 5. Mean reversion (towards recent moving average)
            if i > 30:
                ma_window = min(30, i)
                recent_ma = np.mean(prices[i-ma_window:i])
                deviation = (prices[i-1] - recent_ma) / recent_ma
                mean_rev_component = -mean_reversion * 0.1 * deviation
            else:
                mean_rev_component = 0
            
            # 6. Microstructure effects
            # Bid-ask bounce
            bid_ask_bounce = np.random.normal(0, volatilities[i] * 0.15)
            
            # Order flow imbalance (simplified)
            if i > 5:
                recent_trend = np.mean(np.diff(prices[i-5:i]))
                flow_imbalance = 0.3 * np.sign(recent_trend) * volatilities[i] * np.random.random()
            else:
                flow_imbalance = 0
            
            # 7. Time-of-day effects
            minute_of_day = i % 960  # 960 minutes per day
            if 330 <= minute_of_day < 720:  # RTH (9:30 AM - 4:00 PM)
                time_effect = 1.0
            elif minute_of_day < 330 or minute_of_day >= 720:  # Extended hours
                time_effect = 0.6  # Lower volatility in extended hours
            else:
                time_effect = 0.8
            
            # 8. Combine all components
            total_return = (random_shock * time_effect + 
                          trend_component + 
                          drift_component + 
                          mean_rev_component + 
                          bid_ask_bounce + 
                          flow_imbalance)
            
            # Apply return to price
            prices[i] = prices[i-1] * (1 + total_return)
            
            # 9. Price bounds (prevent extreme moves) - apply before tick adjustment
            min_price = base_price * 0.5  # Don't drop below 50%
            max_price = base_price * 2.0   # Don't go above 200%
            
            # If hitting bounds, add some random variation to prevent sticking
            if prices[i] <= min_price:
                prices[i] = min_price + np.random.exponential(min_price * 0.01)
            elif prices[i] >= max_price:
                prices[i] = max_price - np.random.exponential(max_price * 0.01)
            
            # 10. Ensure minimum price movement (no identical consecutive prices)
            if abs(prices[i] - prices[i-1]) < self.tick_size:
                direction = 1 if np.random.random() > 0.5 else -1
                prices[i] = prices[i-1] + direction * self.tick_size
        
        # Round to tick size
        prices = np.round(prices / self.tick_size) * self.tick_size
        
        return prices
    
    def generate_realistic_ohlc(self, close_prices):
        """Generate realistic OHLC with proper market microstructure"""
        
        num_bars = len(close_prices)
        ohlc_data = []
        
        for i in range(num_bars):
            close_price = close_prices[i]
            
            # Open price (gap from previous close)
            if i == 0:
                open_price = close_price
            else:
                # Realistic gap modeling
                gap_pct = np.random.normal(0, 0.0015)  # 0.15% average gap
                # Larger gaps at market open
                minute_of_day = i % 960
                if minute_of_day == 330:  # Market open (9:30 AM)
                    gap_pct *= 2.0
                
                open_price = close_prices[i-1] * (1 + gap_pct)
                open_price = round(open_price / self.tick_size) * self.tick_size
            
            # Intrabar range modeling
            price_move = abs(close_price - open_price)
            base_range = max(price_move, close_price * 0.001)  # Minimum 0.1% range
            
            # High and low extensions (log-normal distribution)
            high_extension = np.random.lognormal(np.log(base_range * 0.3), 0.5)
            low_extension = np.random.lognormal(np.log(base_range * 0.3), 0.5)
            
            # Calculate high and low
            high = max(open_price, close_price) + high_extension
            low = min(open_price, close_price) - low_extension
            
            # Ensure proper OHLC relationships
            high = max(high, max(open_price, close_price))
            low = min(low, min(open_price, close_price))
            
            # Round to tick size
            high = round(high / self.tick_size) * self.tick_size
            low = round(low / self.tick_size) * self.tick_size
            
            # Volume modeling (realistic patterns)
            minute_of_day = i % 960
            base_volume = 30000  # Base volume per minute
            
            # Time-of-day volume patterns
            if 330 <= minute_of_day < 360:  # Market open (9:30-10:00 AM)
                volume_multiplier = 3.0 + np.random.exponential(2.0)
            elif 360 <= minute_of_day < 690:  # Mid-day RTH
                volume_multiplier = 1.5 + np.random.exponential(1.0)
            elif 690 <= minute_of_day < 720:  # Market close (3:30-4:00 PM)
                volume_multiplier = 2.5 + np.random.exponential(1.5)
            elif minute_of_day < 330 or minute_of_day >= 720:  # Extended hours
                volume_multiplier = 0.4 + np.random.exponential(0.3)
            else:
                volume_multiplier = 1.0
            
            # Volume correlates with price movement and range
            price_impact = (high - low) / close_price
            volume_multiplier *= (1 + price_impact * 5)
            
            volume = int(base_volume * volume_multiplier)
            volume = max(volume, 50)  # Minimum volume
            
            ohlc_data.append({
                'open': round(open_price, 2),
                'high': round(high, 2),
                'low': round(low, 2),
                'close': round(close_price, 2),
                'volume': volume
            })
        
        return ohlc_data
    
    def generate_track(self, track_id, regime_type):
        """Generate a complete track for one market regime"""
        
        print(f"\n🚀 Generating Track {track_id} ({regime_type.upper()} market)")
        
        # Get regime configuration
        regime_config = self.market_regimes[regime_type]
        
        # Generate trading dates
        trading_dates = self.generate_trading_dates()
        
        # Generate all minute timestamps
        all_timestamps = []
        for date in trading_dates:
            day_timestamps = self.generate_minute_timestamps(date)
            all_timestamps.extend(day_timestamps)
        
        total_minutes = len(all_timestamps)
        print(f"📊 Generating {total_minutes:,} minute bars ({len(trading_dates)} trading days)")
        
        # Simulate prices using MarS-inspired methods
        close_prices = self.simulate_mars_inspired_prices(total_minutes, regime_config, self.base_price)
        
        # Generate realistic OHLC
        ohlc_data = self.generate_realistic_ohlc(close_prices)
        
        # Create DataFrame in Polygon format
        bars = []
        for i, timestamp in enumerate(all_timestamps):
            bar = {
                'timestamp': timestamp.strftime('%Y-%m-%d %H:%M:%S'),
                'symbol': 'QQQ',
                'open': ohlc_data[i]['open'],
                'high': ohlc_data[i]['high'],
                'low': ohlc_data[i]['low'],
                'close': ohlc_data[i]['close'],
                'volume': ohlc_data[i]['volume']
            }
            bars.append(bar)
        
        df = pd.DataFrame(bars)
        
        # Validate no identical consecutive closes
        consecutive_same = (df['close'].diff() == 0).sum()
        print(f"✅ Consecutive identical closes: {consecutive_same} (should be 0)")
        
        # Calculate statistics
        returns = df['close'].pct_change().dropna()
        price_start = df['close'].iloc[0]
        price_end = df['close'].iloc[-1]
        total_return = (price_end / price_start - 1) * 100
        volatility = returns.std() * np.sqrt(252 * 390) * 100  # Annualized volatility
        
        print(f"📈 Price: ${price_start:.2f} → ${price_end:.2f} ({total_return:+.1f}%)")
        print(f"📊 Annualized volatility: {volatility:.1f}%")
        print(f"📦 Average volume: {df['volume'].mean():,.0f}")
        
        return df
    
    def save_track(self, df, track_id):
        """Save track to CSV file"""
        
        # Ensure output directory exists
        output_dir = Path("data/future_qqq")
        output_dir.mkdir(parents=True, exist_ok=True)
        
        output_file = output_dir / f"future_qqq_track_{track_id:02d}.csv"
        
        # Save in exact Polygon CSV format
        df.to_csv(output_file, index=False, float_format='%.2f')
        
        print(f"💾 Saved to {output_file}")
        
        return str(output_file)
    
    def generate_all_tracks(self):
        """Generate all tracks for the three market regimes"""
        
        print("🚀 Starting MarS-Inspired Future QQQ Generation")
        print("=" * 80)
        print(f"📅 Period: 100 trading days each × 3 market sets = 300 total days")
        print(f"📈 Market Sets:")
        print(f"   • NORMAL MARKET (Tracks 1-3): Steady growth, typical volatility")
        print(f"   • VOLATILE MARKET (Tracks 4-6): High volatility, frequent reversals")
        print(f"   • BEAR MARKET (Tracks 7-9): Sustained downtrend, elevated volatility")
        print(f"⏰ Hours: Night (4-9:30 AM) + RTH (9:30 AM-4 PM) + After (4-8 PM)")
        print(f"📊 Resolution: 1-minute bars with realistic price variations")
        print("=" * 80)
        
        generated_files = []
        
        # Generate tracks for each regime
        for regime_type, config in self.market_regimes.items():
            print(f"\n📈 Generating {regime_type.upper()} MARKET tracks: {config['tracks']}")
            print(f"   {config['description']}")
            
            for track_id in config['tracks']:
                try:
                    # Generate track
                    df = self.generate_track(track_id, regime_type)
                    
                    # Save track
                    output_file = self.save_track(df, track_id)
                    generated_files.append(output_file)
                    
                except Exception as e:
                    print(f"❌ Error generating track {track_id}: {e}")
                    import traceback
                    traceback.print_exc()
                    continue
        
        print(f"\n🎉 Successfully generated {len(generated_files)} tracks")
        print("=" * 80)
        
        # Summary statistics
        if generated_files:
            sample_df = pd.read_csv(generated_files[0])
            minutes_per_day = len(sample_df) // self.trading_days
            total_bars = len(sample_df) * len(generated_files)
            
            print(f"📊 Generation Summary:")
            print(f"   • {len(generated_files)} tracks generated (3 market sets)")
            print(f"   • {self.trading_days} trading days per track")
            print(f"   • {minutes_per_day:,} minutes per day (960 total: NH + RTH + AH)")
            print(f"   • {len(sample_df):,} minute bars per track")
            print(f"   • {total_bars:,} total minute bars across all tracks")
            print(f"")
            print(f"📈 Market Characteristics:")
            print(f"   • NORMAL: +0.05%/day drift, 1.0x volatility, mean-reverting")
            print(f"   • VOLATILE: 0%/day drift, 2.8x volatility, trending bursts")
            print(f"   • BEAR: -0.3%/day drift, 1.8x volatility, persistent downtrend")
            print(f"")
            print(f"✅ Quality Features:")
            print(f"   • No identical consecutive closes (realistic price variations)")
            print(f"   • Volume patterns based on time-of-day and volatility")
            print(f"   • Proper OHLC relationships with market microstructure")
            print(f"   • Volatility clustering and mean reversion effects")
            print(f"   • Realistic gap modeling and intraday patterns")
        
        return generated_files

def main():
    """Main execution function"""
    
    try:
        # Create generator
        generator = MarsInspiredQQQGenerator()
        
        # Generate all tracks
        generated_files = generator.generate_all_tracks()
        
        if generated_files:
            print("\n✅ MarS-Inspired Future QQQ Generation Complete!")
            print("🎯 Ready for Sentio trading system integration")
            return 0
        else:
            print("\n❌ No files were generated")
            return 1
            
    except Exception as e:
        print(f"\n❌ Generation failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
