# Sentio Trading System - Complete Pseudo-Code Architecture Overview

## 🏗️ **SYSTEM ARCHITECTURE**

```pseudocode
SENTIO_SYSTEM {
    CORE_COMPONENTS {
        • sentio_cli        // Main trading engine and strategy testing
        • sentio_audit      // Audit system for trade verification and analysis
        • MarS              // Market simulation data generator
        • TFA               // Transformer-based AI strategy
        • SIGOR             // Signal-Or strategy with rule-based detectors
    }
    
    DATA_FLOW {
        Market_Data → Strategy → Allocation_Manager → Position_Coordinator → Audit_System
    }
    
    SAFETY_SYSTEMS {
        • Universal_Position_Coordinator  // Prevents conflicting positions
        • Execution_Verifier             // Enforces trading rules
        • Circuit_Breaker               // Emergency protection
        • Audit_Integrity_Checker       // Validates system health
    }
}
```

---

## 📋 **SENTIO_CLI COMMANDS**

### **1. STRATTEST - Core Strategy Testing**
```pseudocode
COMMAND: strattest <strategy> [symbol] [options]
PURPOSE: Test trading strategies with different market data modes

ALGORITHM:
    INPUT: strategy_name, symbol, mode, blocks, regime
    
    STEP 1: Data Loading
        IF mode == "historical":
            LOAD real_market_data FROM "data/equities/"
        ELIF mode == "simulation": 
            LOAD mars_generated_data FROM "data/future_qqq/track_X"
        ELIF mode == "ai-regime":
            GENERATE real_time_data USING MarS_package
    
    STEP 2: Strategy Initialization
        CREATE strategy_instance FROM strategy_registry
        INITIALIZE adaptive_components:
            • StrategyProfiler      // Analyzes strategy behavior
            • AdaptiveAllocationManager  // Dynamic thresholds
            • UniversalPositionCoordinator  // Conflict prevention
            • ExecutionVerifier     // Rule enforcement
            • CircuitBreaker       // Emergency protection
    
    STEP 3: Trading Block Execution
        FOR each_trading_block IN 1..num_blocks:
            RESET portfolio_state
            RESET adaptive_components
            
            FOR each_bar IN trading_block:
                // GOLDEN RULE: System integrity first, strategy second
                
                // 1. Get strategy signal
                signal = strategy.get_signal(market_data)
                
                // 2. Adaptive allocation based on strategy profile
                allocations = allocation_manager.get_allocations(signal, profile)
                
                // 3. Position coordination (conflict prevention)
                decisions = position_coordinator.coordinate(allocations, portfolio)
                
                // 4. Execution verification (one trade per bar, etc.)
                verified_decisions = execution_verifier.verify(decisions)
                
                // 5. Execute trades and update portfolio
                FOR each_decision IN verified_decisions:
                    IF decision.approved:
                        EXECUTE trade
                        UPDATE portfolio
                        RECORD audit_event
                
                // 6. Circuit breaker check
                IF circuit_breaker.should_trip():
                    EMERGENCY_LIQUIDATE all_positions
                    BREAK trading_block
            
            // 7. End-of-block analysis
            strategy_profiler.observe_block_complete(total_fills)
            UPDATE strategy_profile FOR next_block
    
    STEP 4: Performance Analysis
        CALCULATE metrics:
            • Mean_RPB (Return Per Block)
            • Sharpe_Ratio
            • Monthly/Annual_Returns
            • Consistency_Score
        
        DISPLAY results WITH color_coding
        SUGGEST audit_commands FOR verification

OPTIONS:
    --mode: historical|simulation|ai-regime
    --blocks: number of trading blocks (default: 10)
    --regime: normal|volatile|trending
    --track: specific simulation track (1-10)
    --monthly: use monthly blocks instead of 8-hour blocks
```

### **2. INTEGRATED-TEST - New Architecture Testing**
```pseudocode
COMMAND: integrated-test <strategy> [options]
PURPOSE: Test the new strategy-agnostic backend architecture

ALGORITHM:
    STEP 1: Initialize Integration Adapter
        CREATE SentioIntegrationAdapter
        SETUP adaptive_backend_components
    
    STEP 2: Run Integration Tests
        FOR each_test_scenario:
            TEST conflict_prevention
            TEST execution_verification  
            TEST circuit_breaker_functionality
            TEST audit_consistency
        
        REPORT integration_health_status
```

### **3. SYSTEM-HEALTH - Architecture Monitoring**
```pseudocode
COMMAND: system-health
PURPOSE: Monitor overall system architecture health

ALGORITHM:
    CHECK database_connectivity
    VERIFY audit_system_integrity
    TEST strategy_registration
    VALIDATE data_pipeline_health
    REPORT system_status
```

### **4. LIST-STRATEGIES - Strategy Registry**
```pseudocode
COMMAND: list-strategies [options]
PURPOSE: Display all available trading strategies

ALGORITHM:
    SCAN strategy_registry
    FOR each_strategy:
        DISPLAY name, description, parameters
        SHOW compatibility_info
```

### **5. PROBE - System Diagnostics**
```pseudocode
COMMAND: probe
PURPOSE: Deep system diagnostics and component testing

ALGORITHM:
    TEST all_core_components
    VERIFY data_connectivity
    CHECK memory_usage
    VALIDATE configuration_files
    REPORT detailed_diagnostics
```

---

## 🔍 **SENTIO_AUDIT COMMANDS**

### **1. INIT - Database Initialization**
```pseudocode
COMMAND: init
PURPOSE: Initialize audit database schema

ALGORITHM:
    CREATE audit_database
    SETUP tables:
        • audit_runs     // Trading run metadata
        • audit_events   // Individual trade events
        • audit_meta     // System metadata
    CREATE indexes FOR performance
```

### **2. RESET - Database Reset**
```pseudocode
COMMAND: reset --confirm
PURPOSE: Reset audit database (destructive operation)

ALGORITHM:
    IF --confirm_flag_present:
        DELETE audit_database_file
        RECREATE empty_database
        INITIALIZE schema
    ELSE:
        DISPLAY warning_message
```

### **3. NEW-RUN - Start Trading Run**
```pseudocode
COMMAND: new-run --run <id> --strategy <name> --params <file> [options]
PURPOSE: Register new trading run in audit system

ALGORITHM:
    INPUT: run_id, strategy_name, params_file, data_hash
    
    LOAD strategy_parameters FROM params_file
    CALCULATE data_fingerprint
    RECORD run_metadata:
        • run_id, strategy, start_time
        • git_revision, parameters_json
        • data_hash FOR reproducibility
    
    SET as_latest_run_id
```

### **4. LOG - Event Logging**
```pseudocode
COMMAND: log --run <id> --ts <timestamp> --kind <type> [trade_details]
PURPOSE: Log individual trading events

ALGORITHM:
    INPUT: run_id, timestamp, event_kind, trade_details
    
    CREATE audit_event:
        • timestamp, symbol, side, quantity, price
        • pnl_delta, weight, probability
        • reason, notes
    
    CALCULATE cryptographic_hash FOR integrity
    APPEND to_audit_trail
```

### **5. SUMMARIZE - Trading Run Summary**
```pseudocode
COMMAND: summarize [--run <id>]
PURPOSE: Generate comprehensive trading run analysis

ALGORITHM:
    INPUT: run_id (default: latest)
    
    STEP 1: Basic Statistics
        CALCULATE total_trades, total_pnl, duration
        ANALYZE trade_distribution BY instrument
    
    STEP 2: Performance Metrics
        COMPUTE sharpe_ratio, max_drawdown, win_rate
        CALCULATE risk_adjusted_returns
    
    STEP 3: Position Analysis
        TRACK position_changes OVER time
        IDENTIFY largest_positions, turnover_rate
    
    STEP 4: Conflict Detection
        SCAN for_conflicting_positions
        VERIFY position_consistency
    
    DISPLAY formatted_report WITH color_coding
```

### **6. POSITION-HISTORY - Position Tracking**
```pseudocode
COMMAND: position-history [--run <id>] [filters]
PURPOSE: Detailed position change analysis

ALGORITHM:
    INPUT: run_id, symbol_filter, limit
    
    STEP 1: Position Reconstruction
        FOR each_trade_event:
            UPDATE running_position_balance
            CALCULATE average_entry_price
            TRACK unrealized_pnl
    
    STEP 2: Conflict Detection
        IDENTIFY simultaneous_long_and_inverse_positions
        FLAG directional_conflicts (QQQ+SQQQ, TQQQ+PSQ)
    
    STEP 3: Final Verification
        VERIFY final_positions == 0 (if EOD_enabled)
        CHECK cash_balance >= 0
        VALIDATE position_consistency
    
    DISPLAY position_timeline WITH conflict_warnings
```

### **7. TRADE-FLOW - Trade Sequence Analysis**
```pseudocode
COMMAND: trade-flow [--run <id>] [filters]
PURPOSE: Chronological trade sequence analysis

ALGORITHM:
    INPUT: run_id, symbol_filter, show_buy, show_sell, show_hold
    
    STEP 1: Trade Sequence Reconstruction
        ORDER trades BY timestamp
        APPLY symbol_filter IF specified
        FILTER by_trade_type (buy/sell/hold)
    
    STEP 2: Flow Analysis
        IDENTIFY trade_patterns
        DETECT rapid_position_changes
        FLAG unusual_trade_sequences
    
    STEP 3: Validation
        VERIFY trade_consistency
        CHECK for_missing_trades
        VALIDATE cash_flow_logic
    
    DISPLAY chronological_trade_list WITH pattern_highlights
```

### **8. SIGNAL-FLOW - Signal Analysis**
```pseudocode
COMMAND: signal-flow [--run <id>] [filters]
PURPOSE: Strategy signal analysis and decision tracking

ALGORITHM:
    INPUT: run_id, symbol_filter, signal_filters
    
    STEP 1: Signal Reconstruction
        EXTRACT strategy_signals FROM audit_events
        TRACK signal_strength_over_time
        IDENTIFY signal_pattern_changes
    
    STEP 2: Decision Analysis
        CORRELATE signals_to_actual_trades
        IDENTIFY rejected_signals AND reasons
        ANALYZE signal_to_execution_delays
    
    STEP 3: Strategy Performance
        CALCULATE signal_accuracy
        MEASURE signal_consistency
        IDENTIFY optimal_signal_thresholds
    
    DISPLAY signal_timeline WITH decision_outcomes
```

### **9. INTEGRITY - System Health Check**
```pseudocode
COMMAND: integrity [--run <id>]
PURPOSE: Comprehensive system integrity validation

ALGORITHM:
    INPUT: run_id (default: latest)
    
    PRINCIPLE 1: No Negative Cash Balance
        SCAN all_cash_changes
        VERIFY cash_balance >= 0 THROUGHOUT run
        FLAG any_negative_cash_events
    
    PRINCIPLE 2: No Conflicting Positions  
        IDENTIFY simultaneous_long_and_inverse_positions
        CHECK for_directional_conflicts:
            • QQQ/TQQQ vs SQQQ/PSQ = CONFLICT
            • PSQ + SQQQ = OK (both inverse)
        FLAG any_position_conflicts
    
    PRINCIPLE 3: No Negative Short Positions
        VERIFY all_positions >= 0
        CHECK inverse_ETF_usage_instead_of_shorts
        FLAG any_negative_position_quantities
    
    PRINCIPLE 4: Maximum Capital Utilization
        CALCULATE average_capital_utilization
        IDENTIFY suboptimal_cash_deployment
        SUGGEST allocation_improvements
    
    FINAL ASSESSMENT:
        IF critical_violations > 0:
            RETURN failure_exit_code
        ELIF warnings > 0:
            RETURN warning_exit_code  
        ELSE:
            RETURN success_exit_code
```

### **10. LIST - Run History**
```pseudocode
COMMAND: list [--strategy <name>] [--kind <type>]
PURPOSE: List all trading runs with filtering

ALGORITHM:
    INPUT: strategy_filter, kind_filter
    
    QUERY audit_runs_table
    APPLY filters IF specified
    ORDER by_start_time DESC
    
    DISPLAY formatted_table:
        • run_id, strategy, kind
        • start_time, status, notes
        • color_coded_status_indicators
```

### **11. LATEST - Latest Run Info**
```pseudocode
COMMAND: latest
PURPOSE: Show latest run ID and basic info

ALGORITHM:
    QUERY latest_run_id FROM audit_meta
    DISPLAY run_information
    SHOW quick_stats
```

### **12. VERIFY - Cryptographic Verification**
```pseudocode
COMMAND: verify [--run <id>]
PURPOSE: Verify audit trail cryptographic integrity

ALGORITHM:
    INPUT: run_id (default: latest)
    
    FOR each_audit_event:
        RECALCULATE cryptographic_hash
        COMPARE with_stored_hash
        VERIFY chain_integrity
    
    REPORT verification_status
```

---

## 🧠 **CORE ALGORITHM ARCHITECTURE**

### **Strategy-Agnostic Backend Pipeline**
```pseudocode
EXECUTE_BAR_PIPELINE(strategy, market_data, portfolio, timestamp) {
    // GOLDEN RULE: System integrity first, strategy consultation second
    
    STEP 1: System State Verification
        IF circuit_breaker.is_tripped():
            EMERGENCY_LIQUIDATE all_positions
            RETURN empty_decisions
    
    STEP 2: Strategy Signal Generation
        signal = strategy.get_signal(market_data, timestamp)
        strategy_profiler.observe_trade(signal.probability, symbol, timestamp)
    
    STEP 3: Adaptive Allocation
        profile = strategy_profiler.get_current_profile()
        allocations = allocation_manager.get_allocations(signal, profile)
    
    STEP 4: Position Coordination (CONFLICT PREVENTION)
        decisions = position_coordinator.coordinate(allocations, portfolio, timestamp, profile)
        
        // Immediate Conflict Resolution Logic:
        IF portfolio_has_conflicts():
            GENERATE closing_orders FOR all_existing_positions
            REJECT all_new_opening_trades
            RETURN closing_orders_only
    
    STEP 5: Execution Verification
        verified_decisions = execution_verifier.verify_can_execute(decisions)
        // Enforces: one opening trade per bar, unlimited closing trades
    
    STEP 6: Trade Execution
        FOR each_decision IN verified_decisions:
            IF decision.approved:
                EXECUTE trade_with_portfolio_update
                RECORD audit_event WITH signed_quantities
                execution_verifier.mark_trade_executed(decision)
    
    STEP 7: Post-Execution Validation (FIXED: Smart cash reversal)
        IF portfolio.cash < 0:
            is_opening_trade = (trade_qty > 0)  // Positive = BUY = opening
            
            IF is_opening_trade:
                REVERSE opening_trade  // Only reverse trades that consume cash
                LOG "Opening trade reversed due to negative cash"
            ELSE:
                LOG "CRITICAL: Closing trade caused negative cash - deeper bug!"
                // Don't reverse closing trades - they should free up cash
    
    RETURN executed_decisions
}
```

### **Adaptive Strategy Profiler**
```pseudocode
STRATEGY_PROFILER {
    OBSERVE_TRADE(probability, instrument, timestamp) {
        UPDATE recent_trades_history
        CALCULATE signal_volatility
        TRACK trades_per_block
    }
    
    DETECT_TRADING_STYLE() {
        IF trades_per_block > 50:
            RETURN AGGRESSIVE
        ELIF trades_per_block > 20:
            RETURN BURST  
        ELSE:
            RETURN CONSERVATIVE
    }
    
    GET_ADAPTIVE_THRESHOLDS(style) {
        SWITCH style:
            AGGRESSIVE: return {entry_1x: 0.51, entry_3x: 0.55, noise_floor: 0.48}
            BURST:      return {entry_1x: 0.52, entry_3x: 0.58, noise_floor: 0.50}
            CONSERVATIVE: return {entry_1x: 0.55, entry_3x: 0.65, noise_floor: 0.52}
    }
}
```

### **Universal Position Coordinator**
```pseudocode
UNIVERSAL_POSITION_COORDINATOR {
    COORDINATE(allocations, portfolio, timestamp, profile) {
        results = []
        
        // STEP 1: ATOMIC Conflict Resolution (FIXED: Race condition eliminated)
        conflicted_positions = []
        has_conflicts = FALSE
        
        // Atomic check and capture of conflicted positions
        {
            has_long = FALSE
            has_inverse = FALSE
            
            FOR each_position IN portfolio.positions:
                IF has_position(position):  // FIXED: Standardized position check
                    symbol = get_symbol(position)
                    
                    IF symbol IN LONG_ETFS:
                        has_long = TRUE
                    IF symbol IN INVERSE_ETFS:
                        has_inverse = TRUE
                    
                    // Capture ALL positions for potential closure
                    conflicted_positions.ADD(position)
            
            has_conflicts = (has_long AND has_inverse)
        }
        
        IF has_conflicts:
            // Generate closing orders for ALL captured positions atomically
            FOR each_position IN conflicted_positions:
                ADD closing_order_to_results(position)
            
            // Reject ALL incoming allocations
            FOR each_allocation IN allocations:
                ADD rejected_allocation_to_results(allocation, "CONFLICT_RESOLUTION")
            
            RETURN results
        
        // STEP 2: Normal Operation (no conflicts)
        FOR each_allocation IN allocations:
            IF allocation.target_weight == 0:
                // Closing trade - always allow
                ADD approved_allocation_to_results(allocation)
            ELSE:
                // Opening trade - check constraints
                IF opening_trades_this_bar >= 1:
                    ADD rejected_allocation_to_results(allocation, "ONE_TRADE_PER_BAR")
                ELIF would_create_conflict(allocation, portfolio):
                    ADD rejected_allocation_to_results(allocation, "WOULD_CREATE_CONFLICT")
                ELSE:
                    ADD approved_allocation_to_results(allocation)
                    INCREMENT opening_trades_this_bar
        
        RETURN results
    }
    
    CHECK_PORTFOLIO_CONFLICTS(portfolio) {
        has_long = FALSE
        has_inverse = FALSE
        
        FOR each_position IN portfolio.positions:
            IF has_position(position):  // FIXED: Standardized position check
                IF symbol IN {QQQ, TQQQ}:
                    has_long = TRUE
                IF symbol IN {SQQQ, PSQ}:
                    has_inverse = TRUE
        
        RETURN (has_long AND has_inverse)
    }
    
    // FIXED: Standardized position handling functions
    HAS_POSITION(position) {
        RETURN abs(position.qty) > 1e-9
    }
    
    POSITION_EXPOSURE(position) {
        RETURN abs(position.qty)  // Always positive exposure
    }
}
```

### **Audit System Core Logic**
```pseudocode
AUDIT_SYSTEM {
    RECORD_TRADE_EVENT(run_id, timestamp, symbol, side, quantity, price, reason) {
        // Use SIGNED quantities (positive for BUY, negative for SELL)
        signed_qty = (side == "BUY") ? quantity : -quantity
        
        CREATE audit_event:
            run_id, timestamp, symbol, side, signed_qty, price, reason
        
        CALCULATE cryptographic_hash FOR integrity
        INSERT into_audit_events_table
        UPDATE latest_run_id IN audit_meta
    }
    
    CALCULATE_POSITION_HISTORY(run_id) {
        position_balances = {}
        
        FOR each_event IN audit_events WHERE run_id:
            symbol = event.symbol
            signed_qty = event.qty  // Already signed in database
            
            IF symbol NOT IN position_balances:
                position_balances[symbol] = 0
            
            position_balances[symbol] += signed_qty
        
        RETURN position_balances
    }
    
    CHECK_POSITION_CONFLICTS(final_positions) {
        long_etfs = {QQQ, TQQQ}
        inverse_etfs = {SQQQ, PSQ}
        
        has_long = FALSE
        has_inverse = FALSE
        
        FOR symbol, quantity IN final_positions:
            IF quantity > 0:
                IF symbol IN long_etfs:
                    has_long = TRUE
                IF symbol IN inverse_etfs:
                    has_inverse = TRUE
        
        // PSQ + SQQQ is NOT a conflict (both inverse)
        // QQQ + SQQQ IS a conflict (long vs inverse)
        RETURN (has_long AND has_inverse)
    }
}
```

---

## 🔴 **CRITICAL BUG FIXES IMPLEMENTED**

### **1. Race Condition in Conflict Resolution - FIXED**
```pseudocode
// BEFORE (BUGGY):
IF check_portfolio_conflicts(portfolio):
    FOR each_position IN portfolio.positions:  // Portfolio may change during iteration!
        ADD closing_order_to_results(position)

// AFTER (FIXED):
conflicted_positions = []
{
    // Atomic capture of portfolio state
    FOR each_position IN portfolio.positions:
        conflicted_positions.ADD(position)
    has_conflicts = detect_conflicts(conflicted_positions)
}
IF has_conflicts:
    FOR each_position IN conflicted_positions:  // Safe iteration over snapshot
        ADD closing_order_to_results(position)
```

### **2. Cash Reversal Logic Flaw - FIXED**
```pseudocode
// BEFORE (BUGGY):
IF portfolio.cash < 0:
    REVERSE last_trade  // What if last trade was a close that freed up cash?

// AFTER (FIXED):
IF portfolio.cash < 0:
    is_opening_trade = (trade_qty > 0)  // Positive qty = BUY = opening
    
    IF is_opening_trade:
        REVERSE opening_trade  // Only reverse trades that consume cash
    ELSE:
        LOG "CRITICAL: Closing trade caused negative cash - deeper bug!"
        // Don't reverse closing trades - they should free up cash
```

### **3. Signed Quantity Inconsistency - FIXED**
```pseudocode
// BEFORE (INCONSISTENT):
// Audit system uses signed quantities
signed_qty = (side == "BUY") ? quantity : -quantity

// But position coordinator assumes unsigned
IF position.quantity > 0:  // Assumes unsigned

// AFTER (STANDARDIZED):
// Standardized helper functions throughout system
HAS_POSITION(position) {
    RETURN abs(position.qty) > 1e-9  // Consistent position existence check
}

POSITION_EXPOSURE(position) {
    RETURN abs(position.qty)  // Always positive exposure value
}

// Core system handles signed quantities correctly
// All components use standardized helper functions
```

---

## 🎯 **KEY ARCHITECTURAL PRINCIPLES**

### **1. Golden Rule of Execution**
```pseudocode
GOLDEN_RULE {
    // System integrity and risk management rules are checked FIRST
    // Strategy is only consulted if the system allows it
    
    PRIORITY_ORDER:
        1. Circuit_Breaker_Check
        2. Conflict_Resolution  
        3. Execution_Verification
        4. Strategy_Signal_Processing
        5. Trade_Execution
        6. Audit_Recording
}
```

### **2. Immediate Conflict Resolution**
```pseudocode
CONFLICT_RESOLUTION_STRATEGY {
    // No passive "quiet periods" - immediate action
    
    ON_CONFLICT_DETECTED:
        IMMEDIATELY generate_closing_orders FOR all_positions
        REJECT all_new_opening_trades FOR current_bar
        CONTINUE until_portfolio_is_clean
}
```

### **3. Strategy-Agnostic Design**
```pseudocode
STRATEGY_AGNOSTIC_PRINCIPLE {
    // Backend adapts to ANY strategy implementing BaseStrategy interface
    
    ADAPTIVE_COMPONENTS:
        • StrategyProfiler: learns strategy behavior automatically
        • AdaptiveAllocationManager: adjusts thresholds dynamically  
        • UniversalPositionCoordinator: enforces rules universally
        • ExecutionVerifier: validates trades consistently
}
```

### **4. Complete Audit Trail**
```pseudocode
AUDIT_COMPLETENESS {
    // Every position change must be recorded
    
    RECORDED_EVENTS:
        • Strategy signals and decisions
        • Allocation manager outputs
        • Position coordinator approvals/rejections
        • Actual trade executions
        • Emergency circuit breaker actions
        • Final position liquidations
    
    INTEGRITY_VERIFICATION:
        • Cryptographic hashing of all events
        • Cross-validation between StratTest and Audit
        • Real-time consistency checking
}
```

---

## 📊 **SYSTEM METRICS & VALIDATION**

### **Performance Metrics**
```pseudocode
PERFORMANCE_CALCULATION {
    Mean_RPB = (final_equity - initial_equity) / initial_equity / num_blocks * 100
    Sharpe_Ratio = Mean_RPB / StdDev_RPB * sqrt(252/blocks_per_year)
    Monthly_Return = Mean_RPB * blocks_per_month
    Annual_Return = Mean_RPB * blocks_per_year
    Consistency = StdDev_RPB (lower is better)
}
```

### **Integrity Validation**
```pseudocode
INTEGRITY_CHECKS {
    1. No_Negative_Cash: cash_balance >= 0 ALWAYS
    2. No_Conflicting_Positions: NOT (long_ETFs AND inverse_ETFs)
    3. No_Negative_Shorts: all_position_quantities >= 0
    4. Maximum_Capital_Use: optimize capital deployment efficiency
}
```

This pseudo-code overview captures the complete Sentio system architecture, showing how a relatively simple core algorithm (get signal → allocate → coordinate → execute → audit) is wrapped in comprehensive safety systems and adaptive components to handle any trading strategy robustly and safely.
