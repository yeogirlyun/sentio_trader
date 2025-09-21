#!/usr/bin/env python3
"""
Analyze OHLC patterns in original QQQ data to create realistic variations
for future QQQ generation.
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime
import json

def analyze_ohlc_patterns(csv_file):
    """Analyze OHLC patterns from historical QQQ data"""
    
    print(f"📊 Analyzing OHLC patterns from {csv_file}")
    
    # Read the CSV file
    df = pd.read_csv(csv_file)
    print(f"📈 Loaded {len(df)} bars")
    
    # Calculate OHLC relationships
    df['high_low_spread'] = df['high'] - df['low']
    df['open_close_diff'] = df['close'] - df['open']
    df['high_open_diff'] = df['high'] - df['open']
    df['low_open_diff'] = df['open'] - df['low']
    df['high_close_diff'] = df['high'] - df['close']
    df['low_close_diff'] = df['close'] - df['low']
    
    # Calculate relative spreads (as percentage of price)
    df['high_low_spread_pct'] = df['high_low_spread'] / df['close'] * 100
    df['open_close_diff_pct'] = df['open_close_diff'] / df['close'] * 100
    
    # Calculate returns
    df['return_pct'] = df['close'].pct_change() * 100
    
    # Remove NaN values
    df = df.dropna()
    
    # Calculate statistics
    stats = {
        'high_low_spread': {
            'mean': float(df['high_low_spread_pct'].mean()),
            'std': float(df['high_low_spread_pct'].std()),
            'min': float(df['high_low_spread_pct'].min()),
            'max': float(df['high_low_spread_pct'].max()),
            'percentiles': {
                '25': float(df['high_low_spread_pct'].quantile(0.25)),
                '50': float(df['high_low_spread_pct'].quantile(0.50)),
                '75': float(df['high_low_spread_pct'].quantile(0.75)),
                '90': float(df['high_low_spread_pct'].quantile(0.90)),
                '95': float(df['high_low_spread_pct'].quantile(0.95))
            }
        },
        'open_close_diff': {
            'mean': float(df['open_close_diff_pct'].mean()),
            'std': float(df['open_close_diff_pct'].std()),
            'min': float(df['open_close_diff_pct'].min()),
            'max': float(df['open_close_diff_pct'].max())
        },
        'return_volatility': {
            'daily_vol_annualized': float(df['return_pct'].std() * np.sqrt(252)),
            'minute_vol': float(df['return_pct'].std())
        }
    }
    
    # Analyze OHLC relationships
    # High relative to open and close
    df['high_vs_open_pct'] = (df['high'] - df['open']) / df['open'] * 100
    df['high_vs_close_pct'] = (df['high'] - df['close']) / df['close'] * 100
    
    # Low relative to open and close  
    df['low_vs_open_pct'] = (df['open'] - df['low']) / df['open'] * 100
    df['low_vs_close_pct'] = (df['close'] - df['low']) / df['close'] * 100
    
    stats['high_vs_open'] = {
        'mean': float(df['high_vs_open_pct'].mean()),
        'std': float(df['high_vs_open_pct'].std())
    }
    
    stats['high_vs_close'] = {
        'mean': float(df['high_vs_close_pct'].mean()),
        'std': float(df['high_vs_close_pct'].std())
    }
    
    stats['low_vs_open'] = {
        'mean': float(df['low_vs_open_pct'].mean()),
        'std': float(df['low_vs_open_pct'].std())
    }
    
    stats['low_vs_close'] = {
        'mean': float(df['low_vs_close_pct'].mean()),
        'std': float(df['low_vs_close_pct'].std())
    }
    
    # Volume analysis
    stats['volume'] = {
        'mean': float(df['volume'].mean()),
        'std': float(df['volume'].std()),
        'min': float(df['volume'].min()),
        'max': float(df['volume'].max()),
        'percentiles': {
            '25': float(df['volume'].quantile(0.25)),
            '50': float(df['volume'].quantile(0.50)),
            '75': float(df['volume'].quantile(0.75)),
            '90': float(df['volume'].quantile(0.90)),
            '95': float(df['volume'].quantile(0.95))
        }
    }
    
    print("\n📈 OHLC Pattern Analysis Results:")
    print(f"High-Low Spread: {stats['high_low_spread']['mean']:.4f}% ± {stats['high_low_spread']['std']:.4f}%")
    print(f"Open-Close Diff: {stats['open_close_diff']['mean']:.4f}% ± {stats['open_close_diff']['std']:.4f}%")
    print(f"Minute Volatility: {stats['return_volatility']['minute_vol']:.4f}%")
    print(f"Annualized Volatility: {stats['return_volatility']['daily_vol_annualized']:.1f}%")
    print(f"Average Volume: {stats['volume']['mean']:,.0f}")
    
    return stats, df

def create_ohlc_model(stats):
    """Create a model for generating realistic OHLC variations"""
    
    model = {
        'description': 'OHLC variation model based on historical QQQ data',
        'timestamp': datetime.now().isoformat(),
        'parameters': {
            # High-Low spread model (log-normal distribution works well for spreads)
            'high_low_spread': {
                'distribution': 'lognormal',
                'mean_pct': stats['high_low_spread']['mean'],
                'std_pct': stats['high_low_spread']['std'],
                'min_pct': 0.001,  # Minimum 0.001% spread
                'max_pct': stats['high_low_spread']['percentiles']['95']
            },
            
            # High vs open/close (normal distribution)
            'high_extension': {
                'distribution': 'normal',
                'vs_open_mean': stats['high_vs_open']['mean'],
                'vs_open_std': stats['high_vs_open']['std'],
                'vs_close_mean': stats['high_vs_close']['mean'], 
                'vs_close_std': stats['high_vs_close']['std']
            },
            
            # Low vs open/close (normal distribution)
            'low_extension': {
                'distribution': 'normal',
                'vs_open_mean': stats['low_vs_open']['mean'],
                'vs_open_std': stats['low_vs_open']['std'],
                'vs_close_mean': stats['low_vs_close']['mean'],
                'vs_close_std': stats['low_vs_close']['std']
            },
            
            # Volume model (log-normal distribution)
            'volume': {
                'distribution': 'lognormal',
                'mean': stats['volume']['mean'],
                'std': stats['volume']['std'],
                'min': max(100, stats['volume']['min']),  # Minimum 100 volume
                'max': stats['volume']['percentiles']['95']
            }
        }
    }
    
    return model

def test_ohlc_generation(model, close_prices, num_samples=1000):
    """Test the OHLC generation model"""
    
    print(f"\n🧪 Testing OHLC generation with {num_samples} samples")
    
    # Generate test OHLC data
    np.random.seed(42)  # For reproducible results
    
    generated_data = []
    
    for i, close_price in enumerate(close_prices[:num_samples]):
        if i == 0:
            open_price = close_price
        else:
            # Open is typically close to previous close with some gap
            gap = np.random.normal(0, 0.1) / 100 * close_price  # Small gap
            open_price = close_prices[i-1] + gap
        
        # Generate OHLC using the model
        ohlc = generate_ohlc_for_bar(open_price, close_price, model['parameters'])
        generated_data.append(ohlc)
    
    # Analyze generated data
    gen_df = pd.DataFrame(generated_data)
    gen_df['high_low_spread_pct'] = (gen_df['high'] - gen_df['low']) / gen_df['close'] * 100
    
    print(f"Generated High-Low Spread: {gen_df['high_low_spread_pct'].mean():.4f}% ± {gen_df['high_low_spread_pct'].std():.4f}%")
    print(f"Generated Volume: {gen_df['volume'].mean():,.0f} ± {gen_df['volume'].std():,.0f}")
    
    return generated_data

def generate_ohlc_for_bar(open_price, close_price, model_params):
    """Generate realistic OHLC for a single bar given open and close prices"""
    
    # Generate high-low spread
    spread_pct = np.random.lognormal(
        np.log(model_params['high_low_spread']['mean_pct']),
        model_params['high_low_spread']['std_pct'] / model_params['high_low_spread']['mean_pct']
    ) / 100
    
    # Ensure minimum spread
    spread_pct = max(spread_pct, model_params['high_low_spread']['min_pct'] / 100)
    spread_pct = min(spread_pct, model_params['high_low_spread']['max_pct'] / 100)
    
    # Calculate base price (midpoint of open and close)
    base_price = (open_price + close_price) / 2
    spread_amount = base_price * spread_pct
    
    # Generate high and low extensions
    high_ext_pct = np.random.normal(
        model_params['high_extension']['vs_open_mean'],
        model_params['high_extension']['vs_open_std']
    ) / 100
    
    low_ext_pct = np.random.normal(
        model_params['low_extension']['vs_open_mean'], 
        model_params['low_extension']['vs_open_std']
    ) / 100
    
    # Calculate high and low
    high = max(open_price, close_price) + abs(high_ext_pct) * base_price
    low = min(open_price, close_price) - abs(low_ext_pct) * base_price
    
    # Ensure high >= max(open, close) and low <= min(open, close)
    high = max(high, max(open_price, close_price))
    low = min(low, min(open_price, close_price))
    
    # Ensure high > low with minimum spread
    if high - low < spread_amount:
        mid = (high + low) / 2
        high = mid + spread_amount / 2
        low = mid - spread_amount / 2
    
    # Generate volume
    volume = int(np.random.lognormal(
        np.log(model_params['volume']['mean']),
        model_params['volume']['std'] / model_params['volume']['mean']
    ))
    
    # Clamp volume
    volume = max(volume, model_params['volume']['min'])
    volume = min(volume, model_params['volume']['max'])
    
    return {
        'open': round(open_price, 2),
        'high': round(high, 2), 
        'low': round(low, 2),
        'close': round(close_price, 2),
        'volume': volume
    }

if __name__ == "__main__":
    # Analyze original QQQ data
    csv_file = "data/equities/QQQ_NH.csv"
    stats, df = analyze_ohlc_patterns(csv_file)
    
    # Create OHLC model
    model = create_ohlc_model(stats)
    
    # Save model to file
    with open("tools/ohlc_model.json", "w") as f:
        json.dump(model, f, indent=2)
    
    print(f"\n💾 OHLC model saved to tools/ohlc_model.json")
    
    # Test the model
    test_prices = df['close'].values[:1000]  # Use first 1000 close prices
    test_ohlc_generation(model, test_prices)
    
    print("\n✅ OHLC pattern analysis complete!")
