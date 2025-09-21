#include "backend/position_state_machine.h"
#include "backend/adaptive_trading_mechanism.h"
#include "common/utils.h"
#include <iostream>
#include <iomanip>
#include <vector>

using namespace sentio;

// Helper function to print a transition in a formatted way.
void print_transition(const PositionStateMachine::StateTransition& t) {
    std::cout << std::left << std::setw(12) << PositionStateMachine::state_to_string(t.current_state)
              << " | " << std::left << std::setw(12) << PositionStateMachine::signal_type_to_string(t.signal_type)
              << " | " << std::left << std::setw(30) << t.optimal_action
              << " | " << std::left << std::setw(12) << PositionStateMachine::state_to_string(t.target_state)
              << " | " << std::left << std::setw(8) << std::fixed << std::setprecision(3) << t.expected_return
              << " | " << std::left << std::setw(6) << std::fixed << std::setprecision(2) << t.risk_score
              << " | " << t.theoretical_basis << std::endl;
}

void print_header() {
    std::cout << std::left << std::setw(12) << "Current State"
              << " | " << std::left << std::setw(12) << "Signal Type"
              << " | " << std::left << std::setw(30) << "Optimal Action"
              << " | " << std::left << std::setw(12) << "Target State"
              << " | " << std::left << std::setw(8) << "Exp.Ret"
              << " | " << std::left << std::setw(6) << "Risk"
              << " | " << "Theoretical Basis" << std::endl;
    std::cout << std::string(130, '-') << std::endl;
}

int main() {
    std::cout << "🚀 Position State Machine Demonstration" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    PositionStateMachine psm;
    MarketState market_conditions; // Default market conditions
    market_conditions.volatility = 0.2;
    market_conditions.trend_strength = 0.1;
    market_conditions.volume_ratio = 1.0;

    std::cout << "\n--- Testing All 32 State Transition Scenarios ---" << std::endl;
    print_header();

    // Test scenarios organized by portfolio state
    std::vector<std::pair<std::string, PortfolioState>> test_portfolios;
    
    // 1. CASH_ONLY Portfolio
    PortfolioState portfolio_cash;
    test_portfolios.push_back({"CASH_ONLY", portfolio_cash});
    
    // 2. QQQ_ONLY Portfolio
    PortfolioState portfolio_qqq;
    portfolio_qqq.positions["QQQ"] = {"QQQ", 100.0};
    test_portfolios.push_back({"QQQ_ONLY", portfolio_qqq});
    
    // 3. TQQQ_ONLY Portfolio
    PortfolioState portfolio_tqqq;
    portfolio_tqqq.positions["TQQQ"] = {"TQQQ", 50.0};
    test_portfolios.push_back({"TQQQ_ONLY", portfolio_tqqq});
    
    // 4. PSQ_ONLY Portfolio
    PortfolioState portfolio_psq;
    portfolio_psq.positions["PSQ"] = {"PSQ", 200.0};
    test_portfolios.push_back({"PSQ_ONLY", portfolio_psq});
    
    // 5. SQQQ_ONLY Portfolio
    PortfolioState portfolio_sqqq;
    portfolio_sqqq.positions["SQQQ"] = {"SQQQ", 75.0};
    test_portfolios.push_back({"SQQQ_ONLY", portfolio_sqqq});
    
    // 6. QQQ_TQQQ Portfolio
    PortfolioState portfolio_qqq_tqqq;
    portfolio_qqq_tqqq.positions["QQQ"] = {"QQQ", 100.0};
    portfolio_qqq_tqqq.positions["TQQQ"] = {"TQQQ", 50.0};
    test_portfolios.push_back({"QQQ_TQQQ", portfolio_qqq_tqqq});
    
    // 7. PSQ_SQQQ Portfolio
    PortfolioState portfolio_psq_sqqq;
    portfolio_psq_sqqq.positions["PSQ"] = {"PSQ", 200.0};
    portfolio_psq_sqqq.positions["SQQQ"] = {"SQQQ", 75.0};
    test_portfolios.push_back({"PSQ_SQQQ", portfolio_psq_sqqq});
    
    // 8. INVALID Portfolio (conflicting positions)
    PortfolioState portfolio_invalid;
    portfolio_invalid.positions["QQQ"] = {"QQQ", 100.0};
    portfolio_invalid.positions["SQQQ"] = {"SQQQ", 50.0};
    test_portfolios.push_back({"INVALID", portfolio_invalid});

    // Test signal probabilities for each signal type
    std::vector<std::pair<std::string, double>> test_signals = {
        {"STRONG_BUY", 0.80},   // > 0.55 + 0.15 = 0.70
        {"WEAK_BUY", 0.60},     // > 0.55 but < 0.70
        {"WEAK_SELL", 0.35},    // < 0.45 but > 0.30
        {"STRONG_SELL", 0.20}   // < 0.45 - 0.15 = 0.30
    };

    // Test each portfolio state with each signal type
    for (const auto& [portfolio_name, portfolio] : test_portfolios) {
        std::cout << "\n--- " << portfolio_name << " State Transitions ---" << std::endl;
        
        for (const auto& [signal_name, signal_prob] : test_signals) {
            SignalOutput signal;
            signal.probability = signal_prob;
            signal.confidence = 0.8;
            signal.symbol = "QQQ";
            signal.strategy_name = "PSM_Demo";
            
            auto transition = psm.get_optimal_transition(portfolio, signal, market_conditions);
            print_transition(transition);
        }
    }

    // Test state-aware thresholds
    std::cout << "\n--- State-Aware Threshold Testing ---" << std::endl;
    std::cout << std::left << std::setw(15) << "Portfolio State"
              << " | " << std::left << std::setw(12) << "Base Buy"
              << " | " << std::left << std::setw(12) << "Base Sell"
              << " | " << std::left << std::setw(12) << "Adj. Buy"
              << " | " << std::left << std::setw(12) << "Adj. Sell"
              << " | " << "Adjustment Reason" << std::endl;
    std::cout << std::string(100, '-') << std::endl;

    double base_buy = 0.60;
    double base_sell = 0.40;
    
    std::vector<PositionStateMachine::State> states = {
        PositionStateMachine::State::CASH_ONLY,
        PositionStateMachine::State::QQQ_ONLY,
        PositionStateMachine::State::TQQQ_ONLY,
        PositionStateMachine::State::PSQ_ONLY,
        PositionStateMachine::State::SQQQ_ONLY,
        PositionStateMachine::State::QQQ_TQQQ,
        PositionStateMachine::State::PSQ_SQQQ,
        PositionStateMachine::State::INVALID
    };

    for (auto state : states) {
        auto [adj_buy, adj_sell] = psm.get_state_aware_thresholds(base_buy, base_sell, state);
        
        std::string reason;
        if (state == PositionStateMachine::State::CASH_ONLY) {
            reason = "More aggressive for deployment";
        } else if (state == PositionStateMachine::State::QQQ_TQQQ || 
                   state == PositionStateMachine::State::PSQ_SQQQ) {
            reason = "Conservative for leveraged";
        } else if (state == PositionStateMachine::State::TQQQ_ONLY || 
                   state == PositionStateMachine::State::SQQQ_ONLY) {
            reason = "Very conservative for high leverage";
        } else if (state == PositionStateMachine::State::INVALID) {
            reason = "Emergency conservative";
        } else {
            reason = "Standard adjustment";
        }
        
        std::cout << std::left << std::setw(15) << PositionStateMachine::state_to_string(state)
                  << " | " << std::left << std::setw(12) << std::fixed << std::setprecision(3) << base_buy
                  << " | " << std::left << std::setw(12) << std::fixed << std::setprecision(3) << base_sell
                  << " | " << std::left << std::setw(12) << std::fixed << std::setprecision(3) << adj_buy
                  << " | " << std::left << std::setw(12) << std::fixed << std::setprecision(3) << adj_sell
                  << " | " << reason << std::endl;
    }

    // Test transition validation
    std::cout << "\n--- Transition Validation Testing ---" << std::endl;
    
    // Test a high-risk transition (should be rejected)
    PositionStateMachine::StateTransition high_risk_transition = {
        PositionStateMachine::State::CASH_ONLY,
        PositionStateMachine::SignalType::STRONG_BUY,
        PositionStateMachine::State::TQQQ_ONLY,
        "High risk test",
        "Testing validation",
        0.15, 0.95, 0.8  // Very high risk score
    };
    
    bool valid = psm.validate_transition(high_risk_transition, portfolio_cash, 50000.0);
    std::cout << "High risk transition (risk=0.95): " << (valid ? "ACCEPTED" : "REJECTED") << std::endl;
    
    // Test a low confidence transition (should be rejected)
    PositionStateMachine::StateTransition low_confidence_transition = {
        PositionStateMachine::State::CASH_ONLY,
        PositionStateMachine::SignalType::WEAK_BUY,
        PositionStateMachine::State::QQQ_ONLY,
        "Low confidence test",
        "Testing validation",
        0.05, 0.3, 0.2  // Very low confidence
    };
    
    valid = psm.validate_transition(low_confidence_transition, portfolio_cash, 50000.0);
    std::cout << "Low confidence transition (conf=0.2): " << (valid ? "ACCEPTED" : "REJECTED") << std::endl;
    
    // Test a valid transition (should be accepted)
    PositionStateMachine::StateTransition valid_transition = {
        PositionStateMachine::State::CASH_ONLY,
        PositionStateMachine::SignalType::STRONG_BUY,
        PositionStateMachine::State::TQQQ_ONLY,
        "Valid test",
        "Testing validation",
        0.15, 0.6, 0.8  // Reasonable risk and confidence
    };
    
    valid = psm.validate_transition(valid_transition, portfolio_cash, 50000.0);
    std::cout << "Valid transition (risk=0.6, conf=0.8): " << (valid ? "ACCEPTED" : "REJECTED") << std::endl;

    std::cout << "\n✅ Position State Machine Demonstration Complete!" << std::endl;
    std::cout << "\n📊 Summary:" << std::endl;
    std::cout << "• Tested all 32 state transition scenarios" << std::endl;
    std::cout << "• Demonstrated state-aware threshold adjustments" << std::endl;
    std::cout << "• Validated transition risk management" << std::endl;
    std::cout << "• Ready for integration with AdaptivePortfolioManager" << std::endl;

    return 0;
}
