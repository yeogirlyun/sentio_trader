#include "backend/audit_component.h"
#include "common/utils.h"

namespace sentio {

TradeSummary AuditComponent::analyze_equity_curve(const std::vector<double>& equity_curve) const {
    TradeSummary s;
    if (equity_curve.size() < 2) return s;

    // Approximate returns from equity curve deltas
    std::vector<double> returns;
    returns.reserve(equity_curve.size() - 1);
    for (size_t i = 1; i < equity_curve.size(); ++i) {
        double prev = equity_curve[i - 1];
        double curr = equity_curve[i];
        if (prev != 0.0) {
            returns.push_back((curr - prev) / prev);
        }
    }

    s.sharpe = utils::calculate_sharpe_ratio(returns);
    s.max_drawdown = utils::calculate_max_drawdown(equity_curve);
    return s;
}

} // namespace sentio


