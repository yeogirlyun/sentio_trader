# Sentio Trading System Architecture

## Overview

Sentio is a high-performance, modular C++ trading system designed for algorithmic strategy development and backtesting. The system follows a clean architecture pattern with clear separation of concerns between strategy logic, portfolio management, and execution.

## Core Architecture

### 🏗️ System Design Principles

1. **Separation of Concerns**: Each component has a single, well-defined responsibility
2. **Dependency Injection**: Components receive their dependencies rather than creating them
3. **Pure Domain Logic**: Business rules are isolated from I/O and external dependencies
4. **Event-Driven Flow**: Data flows through the system as immutable events
5. **Testability**: All components can be unit tested in isolation

### 🔄 Data Flow Architecture

```
Market Data → Strategy → Signals → Backend → Portfolio → Audit
     ↓           ↓         ↓         ↓          ↓        ↓
   [CSV]    [Indicators] [JSONL]  [Orders]   [State]  [Reports]
```

## 📦 Component Architecture

### 1. Common Layer (`common/`)

**Purpose**: Shared data types and utilities used across all components

- **`types.h/cpp`**: Core value objects (Bar, Position, PortfolioState)
- **`utils.h/cpp`**: File I/O, JSON parsing, time utilities, math functions
- **`config.h`**: Configuration structures and constants

**Key Design**: Pure data structures with no business logic, ensuring deterministic behavior.

### 2. Strategy Layer (`strategy/`)

**Purpose**: Signal generation and market analysis

- **`strategy_component.h/cpp`**: Base strategy framework with indicator management
- **`sigor_strategy.h/cpp`**: Concrete momentum-based strategy implementation
- **`signal_output.h/cpp`**: Signal data structure and serialization
- **`sigor_config.h/cpp`**: Strategy-specific configuration

**Key Algorithm**: 
- Processes market bars sequentially
- Maintains technical indicators (moving averages, volatility, momentum)
- Generates probability-based trading signals after warmup period
- Outputs signals in standardized format for downstream processing
