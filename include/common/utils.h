#pragma once

// =============================================================================
// Module: common/utils.h
// Purpose: Shared utility functions for file I/O, time, JSON, hashing, math.
//
// Core idea:
// - Keep convenience helpers centralized and reusable across components.
// - Implementations avoid global state and side effects beyond their scope.
// - JSON helpers are intentionally minimal to avoid heavy deps by default.
// =============================================================================

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <map>
#include <cstdint>
#include "types.h"

namespace sentio {
namespace utils {

// ----------------------------- File I/O utilities ----------------------------
// Read OHLCV bars from a CSV file with header. Expected columns:
// symbol,timestamp_ms,open,high,low,close,volume
std::vector<Bar> read_csv_data(const std::string& path);

// Write one JSON object per line (JSON Lines format)
bool write_jsonl(const std::string& path, const std::vector<std::string>& lines);

// Write a CSV file from a 2D string matrix (first row typically headers)
bool write_csv(const std::string& path, const std::vector<std::vector<std::string>>& data);

// ------------------------------ Time utilities -------------------------------
// Parse ISO-like timestamp (YYYY-MM-DD HH:MM:SS) into milliseconds since epoch
int64_t timestamp_to_ms(const std::string& timestamp_str);

// Convert milliseconds since epoch to formatted timestamp string
std::string ms_to_timestamp(int64_t ms);

// Current wall-clock timestamp in ISO-like string
std::string current_timestamp_str();

// ------------------------------ JSON utilities -------------------------------
// Very small helpers for flat string maps (not a general JSON parser)
std::string to_json(const std::map<std::string, std::string>& data);
std::map<std::string, std::string> from_json(const std::string& json_str);

// -------------------------------- Hash utilities -----------------------------
// SHA-256 of input data as lowercase hex string (self-contained implementation)
std::string calculate_sha256(const std::string& data);

// Generate a run id like: <prefix>-YYYYMMDDHHMMSS-<8charhash>
std::string generate_run_id(const std::string& prefix);

// -------------------------------- Math utilities -----------------------------
double calculate_sharpe_ratio(const std::vector<double>& returns, double risk_free_rate = 0.0);
double calculate_max_drawdown(const std::vector<double>& equity_curve);

} // namespace utils
} // namespace sentio


