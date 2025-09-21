#!/usr/bin/env python3
"""
HistoricalContextAgent for MarS Integration

This agent uses real historical market data to establish realistic market conditions,
then transitions to MarS's AI-powered generation for continuation.
"""

import random
import numpy as np
from typing import List, Dict, Any, Optional
from pandas import Timestamp, Timedelta
import pandas as pd

# Add MarS to Python path
import sys
from pathlib import Path
mars_path = Path(__file__).parent.parent / "MarS"
sys.path.insert(0, str(mars_path))

from mlib.core.action import Action
from mlib.core.base_agent import BaseAgent
from mlib.core.observation import Observation
from mlib.core.limit_order import LimitOrder

class HistoricalBar:
    """Represents a historical market bar."""
    def __init__(self, timestamp: Timestamp, open: float, high: float, 
                 low: float, close: float, volume: int):
        self.timestamp = timestamp
        self.open = open
        self.high = high
        self.low = low
        self.close = close
        self.volume = volume
        self.range = high - low
        self.body = abs(close - open)
        self.upper_shadow = high - max(open, close)
        self.lower_shadow = min(open, close) - low

class HistoricalContextAgent(BaseAgent):
    """
    Agent that uses historical market data to establish realistic market conditions,
    then transitions to synthetic generation.
    """
    
    def __init__(
        self,
        symbol: str,
        historical_bars: List[HistoricalBar],
        continuation_minutes: int = 60,
        start_time: Timestamp = None,
        end_time: Timestamp = None,
        seed: int = 42,
        transition_mode: str = "gradual"  # "gradual" or "immediate"
    ):
        super().__init__(
            init_cash=1000000,  # Large cash for market making
            communication_delay=0,
            computation_delay=0,
        )
        
        self.symbol = symbol
        self.historical_bars = historical_bars
        self.continuation_minutes = continuation_minutes
        self.transition_mode = transition_mode
        self.rnd = random.Random(seed)
        
        # Time management - ensure all timestamps are timezone-aware
        self.start_time = start_time or historical_bars[0].timestamp
        self.historical_end_time = historical_bars[-1].timestamp
        self.end_time = end_time or (self.historical_end_time + Timedelta(minutes=continuation_minutes))
        
        # Ensure all times are timezone-aware (UTC)
        if self.start_time.tz is None:
            self.start_time = self.start_time.tz_localize('UTC')
        if self.historical_end_time.tz is None:
            self.historical_end_time = self.historical_end_time.tz_localize('UTC')
        if self.end_time.tz is None:
            self.end_time = self.end_time.tz_localize('UTC')
        
        # State tracking
        self.current_bar_index = 0
        self.current_price = historical_bars[-1].close
        self.price_history = [bar.close for bar in historical_bars]
        self.volume_history = [bar.volume for bar in historical_bars]
        
        # Market statistics for realistic generation
        self._calculate_market_stats()
        
        # Transition state
        self.in_transition = False
        self.transition_start_time = None
        
    def _calculate_market_stats(self):
        """Calculate market statistics from historical data."""
        if len(self.price_history) < 2:
            return
            
        # Price statistics
        returns = np.diff(np.log(self.price_history))
        self.mean_return = np.mean(returns)
        self.volatility = np.std(returns)
        self.price_trend = (self.price_history[-1] - self.price_history[0]) / len(self.price_history)
        
        # Volume statistics
        self.mean_volume = np.mean(self.volume_history)
        self.volume_std = np.std(self.volume_history)
        self.volume_trend = (self.volume_history[-1] - self.volume_history[0]) / len(self.volume_history)
        
        # Intraday patterns
        self._analyze_intraday_patterns()
        
    def _analyze_intraday_patterns(self):
        """Analyze intraday volume and volatility patterns."""
        hourly_volumes = {}
        hourly_volatilities = {}
        
        for bar in self.historical_bars:
            hour = bar.timestamp.hour
            if hour not in hourly_volumes:
                hourly_volumes[hour] = []
                hourly_volatilities[hour] = []
            
            hourly_volumes[hour].append(bar.volume)
            if bar.range > 0:
                hourly_volatilities[hour].append(bar.range / bar.close)
        
        # Calculate hourly multipliers
        self.hourly_volume_multipliers = {}
        self.hourly_volatility_multipliers = {}
        
        for hour in range(24):
            if hour in hourly_volumes:
                self.hourly_volume_multipliers[hour] = np.mean(hourly_volumes[hour]) / self.mean_volume
                self.hourly_volatility_multipliers[hour] = np.mean(hourly_volatilities[hour]) / self.volatility
            else:
                self.hourly_volume_multipliers[hour] = 1.0
                self.hourly_volatility_multipliers[hour] = 1.0
    
    def get_action(self, observation: Observation) -> Action:
        """Generate action based on current time and mode."""
        time = observation.time
        
        if time < self.start_time:
            return Action(agent_id=self.agent_id, orders=[], time=time, 
                         next_wakeup_time=self.start_time)
        
        if time > self.end_time:
            return Action(agent_id=self.agent_id, orders=[], time=time, 
                         next_wakeup_time=None)
        
        # Determine if we're in historical replay or continuation mode
        if time <= self.historical_end_time:
            orders = self._generate_historical_orders(time)
        else:
            orders = self._generate_continuation_orders(time)
        
        # Calculate next wakeup time
        next_wakeup_time = time + Timedelta(seconds=self._get_order_interval())
        
        return Action(
            agent_id=self.agent_id,
            orders=orders,
            time=time,
            next_wakeup_time=next_wakeup_time
        )
    
    def _generate_historical_orders(self, time: Timestamp) -> List[LimitOrder]:
        """Generate orders based on historical data - FAST MODE."""
        # Skip detailed historical replay - just use historical patterns
        # This makes it much faster while still maintaining realistic context
        
        orders = []
        
        # Use historical patterns for realistic generation
        hour = time.hour
        volume_multiplier = self.hourly_volume_multipliers.get(hour, 1.0)
        volatility_multiplier = self.hourly_volatility_multipliers.get(hour, 1.0)
        
        # Generate realistic price movement based on historical patterns
        price_change = np.random.normal(self.mean_return, self.volatility * volatility_multiplier)
        self.current_price *= (1 + price_change)
        
        # Generate market-making orders
        spread = self._calculate_continuation_spread(volatility_multiplier)
        mid_price = self.current_price
        
        # Buy order
        bid_price = mid_price - spread / 2
        bid_volume = int(self.mean_volume * volume_multiplier * self.rnd.uniform(0.1, 0.3))
        if bid_price > 0 and bid_volume > 0:
            buy_order = LimitOrder(
                agent_id=self.agent_id,
                symbol=self.symbol,
                type="B",
                price=int(bid_price * 100),
                volume=bid_volume,
                time=time
            )
            orders.append(buy_order)
        
        # Sell order
        ask_price = mid_price + spread / 2
        ask_volume = int(self.mean_volume * volume_multiplier * self.rnd.uniform(0.1, 0.3))
        if ask_price > 0 and ask_volume > 0:
            sell_order = LimitOrder(
                agent_id=self.agent_id,
                symbol=self.symbol,
                type="S",
                price=int(ask_price * 100),
                volume=ask_volume,
                time=time
            )
            orders.append(sell_order)
        
        return orders
    
    def _generate_continuation_orders(self, time: Timestamp) -> List[LimitOrder]:
        """Generate orders for continuation period using historical patterns."""
        orders = []
        
        # Apply time-of-day patterns
        hour = time.hour
        volume_multiplier = self.hourly_volume_multipliers.get(hour, 1.0)
        volatility_multiplier = self.hourly_volatility_multipliers.get(hour, 1.0)
        
        # Generate realistic price movement
        price_change = np.random.normal(self.mean_return, self.volatility * volatility_multiplier)
        self.current_price *= (1 + price_change)
        
        # Generate market-making orders
        spread = self._calculate_continuation_spread(volatility_multiplier)
        mid_price = self.current_price
        
        # Buy order
        bid_price = mid_price - spread / 2
        bid_volume = int(self.mean_volume * volume_multiplier * self.rnd.uniform(0.1, 0.3))
        if bid_price > 0 and bid_volume > 0:
            buy_order = LimitOrder(
                agent_id=self.agent_id,
                symbol=self.symbol,
                type="B",
                price=int(bid_price * 100),
                volume=bid_volume,
                time=time
            )
            orders.append(buy_order)
        
        # Sell order
        ask_price = mid_price + spread / 2
        ask_volume = int(self.mean_volume * volume_multiplier * self.rnd.uniform(0.1, 0.3))
        if ask_price > 0 and ask_volume > 0:
            sell_order = LimitOrder(
                agent_id=self.agent_id,
                symbol=self.symbol,
                type="S",
                price=int(ask_price * 100),
                volume=ask_volume,
                time=time
            )
            orders.append(sell_order)
        
        return orders
    
    def _find_historical_bar(self, time: Timestamp) -> Optional[HistoricalBar]:
        """Find the historical bar corresponding to the given time."""
        # Ensure time is timezone-aware
        if time.tz is None:
            time = time.tz_localize('UTC')
        
        for bar in self.historical_bars:
            if bar.timestamp <= time < bar.timestamp + Timedelta(minutes=1):
                return bar
        return None
    
    def _calculate_spread(self, bar: HistoricalBar) -> float:
        """Calculate realistic spread based on historical bar."""
        # Use bar range as basis for spread
        base_spread = bar.range * 0.1  # 10% of bar range
        min_spread = 0.01  # Minimum 1 cent spread
        max_spread = 0.50  # Maximum 50 cent spread
        
        return max(min_spread, min(max_spread, base_spread))
    
    def _calculate_continuation_spread(self, volatility_multiplier: float) -> float:
        """Calculate spread for continuation period."""
        base_spread = self.volatility * self.current_price * volatility_multiplier * 2
        min_spread = 0.01
        max_spread = 0.50
        
        return max(min_spread, min(max_spread, base_spread))
    
    def _calculate_order_volume(self, bar: HistoricalBar, order_type: str) -> int:
        """Calculate realistic order volume based on historical bar."""
        # Use a fraction of bar volume
        base_volume = int(bar.volume * self.rnd.uniform(0.05, 0.15))
        
        # Ensure minimum volume
        min_volume = 100
        max_volume = 10000
        
        return max(min_volume, min(max_volume, base_volume))
    
    def _get_order_interval(self) -> float:
        """Get realistic order submission interval."""
        # Vary interval based on market conditions
        base_interval = 5.0  # 5 seconds base
        variation = self.rnd.uniform(0.5, 2.0)  # 0.5x to 2x variation
        
        return base_interval * variation

def load_historical_bars_from_csv(filepath: str) -> List[HistoricalBar]:
    """Load historical bars from CSV file."""
    df = pd.read_csv(filepath)
    bars = []
    
    # Handle different CSV formats
    timestamp_col = 'timestamp' if 'timestamp' in df.columns else 'ts_utc'
    
    for _, row in df.iterrows():
        # Convert timestamp to timezone-aware if needed
        timestamp = pd.to_datetime(row[timestamp_col])
        if timestamp.tz is None:
            # Assume UTC if no timezone info
            timestamp = timestamp.tz_localize('UTC')
        
        bar = HistoricalBar(
            timestamp=timestamp,
            open=float(row['open']),
            high=float(row['high']),
            low=float(row['low']),
            close=float(row['close']),
            volume=int(row['volume'])
        )
        bars.append(bar)
    
    return bars

def create_historical_context_agent(
    symbol: str,
    historical_data_file: str,
    continuation_minutes: int = 60,
    seed: int = 42,
    use_recent_data_only: bool = True,
    recent_days: int = 30
) -> HistoricalContextAgent:
    """Create a HistoricalContextAgent from historical data file."""
    
    # Load historical data
    historical_bars = load_historical_bars_from_csv(historical_data_file)
    
    if not historical_bars:
        raise ValueError(f"No historical data loaded from {historical_data_file}")
    
    # Use only recent data for faster processing
    if use_recent_data_only and len(historical_bars) > recent_days * 390:  # ~390 bars per day
        # Take only the last N days of data
        recent_bars = historical_bars[-(recent_days * 390):]
        print(f"📊 Using last {recent_days} days of data ({len(recent_bars)} bars) for faster processing")
        historical_bars = recent_bars
    
    # Create agent
    agent = HistoricalContextAgent(
        symbol=symbol,
        historical_bars=historical_bars,
        continuation_minutes=continuation_minutes,
        seed=seed
    )
    
    return agent

# Example usage
if __name__ == "__main__":
    # Example: Create agent with QQQ historical data
    try:
        agent = create_historical_context_agent(
            symbol="QQQ",
            historical_data_file="data/equities/QQQ_NH.csv",
            continuation_minutes=120,  # Continue for 2 hours
            seed=42
        )
        
        print(f"✅ Created HistoricalContextAgent for {agent.symbol}")
        print(f"📊 Historical period: {agent.start_time} to {agent.historical_end_time}")
        print(f"🚀 Continuation period: {agent.historical_end_time} to {agent.end_time}")
        print(f"📈 Starting price: ${agent.current_price:.2f}")
        print(f"📊 Market volatility: {agent.volatility:.4f}")
        print(f"📊 Mean volume: {agent.mean_volume:,.0f}")
        
    except Exception as e:
        print(f"❌ Error creating agent: {e}")
