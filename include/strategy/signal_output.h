#pragma once

// =============================================================================
// Module: strategy/signal_output.h
// Purpose: Defines the portable signal record emitted by strategies.
//
// Core idea:
// - A signal is the strategy's probabilistic opinion about the next action,
//   decoupled from execution. It can be serialized to JSON/CSV and consumed by
//   the backend for trade decisions or by audit tools for analysis.
// =============================================================================

#include <string>
#include <map>
#include <cstdint>

namespace sentio {

struct SignalOutput {
    int64_t timestamp_ms = 0;
    int bar_index = 0;
    std::string symbol;
    double probability = 0.0;       // 0.0 to 1.0
    double confidence = 0.0;        // 0.0 to 1.0
    std::string strategy_name;
    std::string strategy_version;
    std::map<std::string, std::string> metadata;

    std::string to_json() const;    // serialize to JSON object string
    std::string to_csv() const;     // serialize to CSV line (no header)
    static SignalOutput from_json(const std::string& json_str);
};

} // namespace sentio


