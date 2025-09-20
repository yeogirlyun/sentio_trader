#pragma once

// =============================================================================
// Module: backend/audit_component.h
// Purpose: Post-run analysis of trades and portfolio performance metrics.
//
// Core idea:
// - Consume executed trades and produce risk/performance analytics such as
//   Sharpe ratio, max drawdown, win rate, and conflict detection summaries.
// =============================================================================

#include <vector>
#include <string>
#include <map>

namespace sentio {

struct TradeSummary {
    int total_trades = 0;
    int wins = 0;
    int losses = 0;
    double win_rate = 0.0;
    double sharpe = 0.0;
    double max_drawdown = 0.0;
};

class AuditComponent {
public:
    TradeSummary analyze_equity_curve(const std::vector<double>& equity_curve) const;
};

} // namespace sentio


