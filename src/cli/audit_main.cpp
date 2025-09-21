#include "backend/audit_component.h"
#include "common/utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

// =============================================================================
// Executable: audit
// Purpose: Minimal CLI to compute performance metrics from an equity curve CSV.
//
// Usage:
//   audit --equity PATH
// Input CSV format:
//   equity (single column, with header)
// =============================================================================

namespace {
    // Use common get_arg function from utils
    using sentio::utils::get_arg;
}

int main(int argc, char** argv) {
    const std::string equity_path = get_arg(argc, argv, "--equity");
    if (equity_path.empty()) {
        std::cerr << "ERROR: --equity is required.\n";
        std::cerr << "Usage: audit --equity PATH\n";
        return 1;
    }

    std::ifstream in(equity_path);
    if (!in.is_open()) {
        std::cerr << "ERROR: cannot open " << equity_path << "\n";
        return 2;
    }

    std::string line;
    // Skip header
    std::getline(in, line);
    std::vector<double> equity;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        equity.push_back(std::stod(line));
    }

    sentio::AuditComponent auditor;
    auto summary = auditor.analyze_equity_curve(equity);

    std::cout << "Sharpe: " << summary.sharpe << "\n";
    std::cout << "Max Drawdown: " << summary.max_drawdown << "\n";
    return 0;
}


