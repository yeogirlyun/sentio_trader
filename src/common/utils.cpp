#include "common/utils.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <filesystem>

// =============================================================================
// Module: common/utils.cpp
// Purpose: Implementation of utility functions for file I/O, time handling,
//          JSON parsing, hashing, and mathematical calculations.
//
// This module provides the concrete implementations for all utility functions
// declared in utils.h. Each section handles a specific domain of functionality
// to keep the codebase modular and maintainable.
// =============================================================================

namespace sentio {
namespace utils {

// --------------------------------- Helpers ----------------------------------
namespace {
    /// Helper function to remove leading and trailing whitespace from strings
    /// Used internally by CSV parsing and JSON processing functions
    static inline std::string trim(const std::string& s) {
        const char* ws = " \t\n\r\f\v";
        const auto start = s.find_first_not_of(ws);
        if (start == std::string::npos) return "";
        const auto end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }
}

// ----------------------------- File I/O utilities ----------------------------

/// Reads OHLCV market data from CSV files with automatic format detection
/// 
/// This function handles two CSV formats:
/// 1. QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume (symbol extracted from filename)
/// 2. Standard format: symbol,timestamp_ms,open,high,low,close,volume
/// 
/// The function automatically detects the format by examining the header row
/// and processes the data accordingly, ensuring compatibility with different
/// data sources while maintaining a consistent Bar output format.
std::vector<Bar> read_csv_data(const std::string& path) {
    std::vector<Bar> bars;
    std::ifstream file(path);
    
    // Early return if file cannot be opened
    if (!file.is_open()) {
        return bars;
    }

    std::string line;
    
    // Read and analyze header to determine CSV format
    std::getline(file, line);
    bool is_qqq_format = (line.find("ts_utc") != std::string::npos); 
    bool is_standard_format = (line.find("symbol") != std::string::npos);
    
    // For QQQ format, extract symbol from filename since it's not in the CSV
    std::string default_symbol = "UNKNOWN";
    if (is_qqq_format) {
        size_t last_slash = path.find_last_of("/\\");
        std::string filename = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;
        
        // Pattern matching for common ETF symbols
        if (filename.find("QQQ") != std::string::npos) default_symbol = "QQQ";
        else if (filename.find("SQQQ") != std::string::npos) default_symbol = "SQQQ";
        else if (filename.find("TQQQ") != std::string::npos) default_symbol = "TQQQ";
    }

    // Process each data row according to the detected format
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string item;
        Bar b{};

        // Parse timestamp and symbol based on detected format
        if (is_qqq_format) {
            // QQQ format: ts_utc,ts_nyt_epoch,open,high,low,close,volume
            b.symbol = default_symbol;
            
            // Skip ts_utc column (ISO timestamp string)
            std::getline(ss, item, ',');
            
            // Use ts_nyt_epoch as timestamp (Unix seconds -> convert to milliseconds)
            std::getline(ss, item, ',');
            b.timestamp_ms = std::stoll(trim(item)) * 1000;
            
        } else if (is_standard_format) {
            // Standard format: symbol,timestamp_ms,open,high,low,close,volume
            std::getline(ss, item, ',');
            b.symbol = trim(item);
            
            std::getline(ss, item, ',');
            b.timestamp_ms = std::stoll(trim(item));
            
        } else {
            // Fallback for unknown formats: assume first column is symbol
            std::getline(ss, item, ',');
            b.symbol = trim(item);
            std::getline(ss, item, ',');
            b.timestamp_ms = std::stoll(trim(item));
        }

        // Parse OHLCV data (same format across all CSV types)
        std::getline(ss, item, ',');
        b.open = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.high = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.low = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.close = std::stod(trim(item));
        
        std::getline(ss, item, ',');
        b.volume = std::stod(trim(item));

        bars.push_back(b);
    }

    return bars;
}

bool write_jsonl(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (const auto& l : lines) {
        out << l << '\n';
    }
    return true;
}

bool write_csv(const std::string& path, const std::vector<std::vector<std::string>>& data) {
    std::ofstream out(path);
    if (!out.is_open()) return false;
    for (const auto& row : data) {
        for (size_t i = 0; i < row.size(); ++i) {
            out << row[i];
            if (i + 1 < row.size()) out << ',';
        }
        out << '\n';
    }
    return true;
}

// ------------------------------ Time utilities -------------------------------
int64_t timestamp_to_ms(const std::string& timestamp_str) {
    // Minimal parser for "YYYY-MM-DD HH:MM:SS" -> epoch ms
    std::tm tm{};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto time_c = timegm(&tm); // UTC
    return static_cast<int64_t>(time_c) * 1000;
}

std::string ms_to_timestamp(int64_t ms) {
    std::time_t t = static_cast<std::time_t>(ms / 1000);
    std::tm* gmt = gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmt);
    return std::string(buf);
}

std::string current_timestamp_str() {
    std::time_t now = std::time(nullptr);
    std::tm* gmt = gmtime(&now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmt);
    return std::string(buf);
}

// ------------------------------ JSON utilities -------------------------------
std::string to_json(const std::map<std::string, std::string>& data) {
    std::ostringstream os;
    os << '{';
    bool first = true;
    for (const auto& [k, v] : data) {
        if (!first) os << ',';
        first = false;
        os << '"' << k << '"' << ':' << '"' << v << '"';
    }
    os << '}';
    return os.str();
}

std::map<std::string, std::string> from_json(const std::string& json_str) {
    // Minimal robust parser for a flat string map {"k":"v",...} that respects quotes
    std::map<std::string, std::string> out;
    if (json_str.size() < 2 || json_str.front() != '{' || json_str.back() != '}') return out;
    const std::string s = json_str.substr(1, json_str.size() - 2);

    // Split into top-level pairs by commas not inside quotes
    std::vector<std::string> pairs;
    std::string current;
    bool in_quotes = false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"') {
            // toggle quotes unless escaped
            bool escaped = (i > 0 && s[i-1] == '\\');
            if (!escaped) in_quotes = !in_quotes;
            current.push_back(c);
        } else if (c == ',' && !in_quotes) {
            pairs.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) pairs.push_back(current);

    auto trim_ws = [](const std::string& str){
        size_t a = 0, b = str.size();
        while (a < b && std::isspace(static_cast<unsigned char>(str[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(str[b-1]))) --b;
        return str.substr(a, b - a);
    };

    for (auto& p : pairs) {
        std::string pair = trim_ws(p);
        // find colon not inside quotes
        size_t colon_pos = std::string::npos;
        in_quotes = false;
        for (size_t i = 0; i < pair.size(); ++i) {
            char c = pair[i];
            if (c == '"') {
                bool escaped = (i > 0 && pair[i-1] == '\\');
                if (!escaped) in_quotes = !in_quotes;
            } else if (c == ':' && !in_quotes) {
                colon_pos = i; break;
            }
        }
        if (colon_pos == std::string::npos) continue;
        std::string key = trim_ws(pair.substr(0, colon_pos));
        std::string val = trim_ws(pair.substr(colon_pos + 1));
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"') key = key.substr(1, key.size() - 2);
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
        out[key] = val;
    }
    return out;
}

// -------------------------------- Hash utilities -----------------------------
// Simple SHA-256 based on std::hash fallback: for production, replace with a
// real SHA-256; we simulate deterministic short id by hashing and hex-encoding.
std::string calculate_sha256(const std::string& data) {
    // Not a real SHA-256; placeholder stable hash for demonstration.
    std::hash<std::string> h;
    auto v = h(data);
    std::ostringstream os;
    os << std::hex << std::setw(16) << std::setfill('0') << v;
    std::string hex = os.str();
    // Repeat to reach 32 hex chars
    return hex + hex;
}

std::string generate_run_id(const std::string& prefix) {
    // Produce an 8-digit numeric ID. Use time + hash for low collision chance.
    (void)prefix; // unused but kept for signature stability
    auto now = std::time(nullptr);
    std::string seed = std::to_string(static_cast<long long>(now));
    std::hash<std::string> h;
    unsigned long long v = static_cast<unsigned long long>(h(seed));
    unsigned int id = static_cast<unsigned int>(v % 100000000ULL);
    std::ostringstream os;
    os << std::setw(8) << std::setfill('0') << id;
    return os.str();
}

// -------------------------------- Math utilities -----------------------------
double calculate_sharpe_ratio(const std::vector<double>& returns, double risk_free_rate) {
    if (returns.empty()) return 0.0;
    double mean = 0.0;
    for (double r : returns) mean += r;
    mean /= static_cast<double>(returns.size());
    double variance = 0.0;
    for (double r : returns) variance += (r - mean) * (r - mean);
    variance /= static_cast<double>(returns.size());
    double stddev = std::sqrt(variance);
    if (stddev == 0.0) return 0.0;
    return (mean - risk_free_rate) / stddev;
}

double calculate_max_drawdown(const std::vector<double>& equity_curve) {
    double peak = -std::numeric_limits<double>::infinity();
    double max_dd = 0.0;
    for (double e : equity_curve) {
        peak = std::max(peak, e);
        max_dd = std::max(max_dd, (peak - e) / std::max(1.0, peak));
    }
    return max_dd;
}

// -------------------------------- Logging utilities --------------------------
namespace {
    static inline std::string log_dir() {
        return std::string("logs");
    }
    static inline void ensure_log_dir() {
        std::error_code ec;
        std::filesystem::create_directories(log_dir(), ec);
    }
    static inline std::string iso_now() {
        std::time_t now = std::time(nullptr);
        std::tm* gmt = gmtime(&now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmt);
        return std::string(buf);
    }
}

void log_debug(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/debug.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " DEBUG common:utils:0 - " << message << '\n';
}

void log_info(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/app.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " INFO common:utils:0 - " << message << '\n';
}

void log_warning(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/app.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " WARNING common:utils:0 - " << message << '\n';
}

void log_error(const std::string& message) {
    ensure_log_dir();
    std::ofstream out(log_dir() + "/errors.log", std::ios::app);
    if (!out.is_open()) return;
    out << iso_now() << " ERROR common:utils:0 - " << message << '\n';
}

bool would_instruments_conflict(const std::string& proposed, const std::string& existing) {
    // Consolidated conflict detection logic (removes duplicate code)
    static const std::map<std::string, std::vector<std::string>> conflicts = {
        {"TQQQ", {"SQQQ", "PSQ"}},
        {"SQQQ", {"TQQQ", "QQQ"}},
        {"PSQ",  {"TQQQ", "QQQ"}},
        {"QQQ",  {"SQQQ", "PSQ"}}
    };
    
    auto it = conflicts.find(proposed);
    if (it != conflicts.end()) {
        return std::find(it->second.begin(), it->second.end(), existing) != it->second.end();
    }
    
    return false;
}

// -------------------------------- CLI utilities -------------------------------

/// Parse command line arguments supporting both "--name value" and "--name=value" formats
/// 
/// This function provides flexible command-line argument parsing that supports:
/// - Space-separated format: --name value
/// - Equals-separated format: --name=value
/// 
/// @param argc Number of command line arguments
/// @param argv Array of command line argument strings
/// @param name The argument name to search for (including --)
/// @param def Default value to return if argument not found
/// @return The argument value if found, otherwise the default value
std::string get_arg(int argc, char** argv, const std::string& name, const std::string& def) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == name) {
            // Handle "--name value" format
            if (i + 1 < argc) {
                std::string next = argv[i + 1];
                if (!next.empty() && next[0] != '-') return next;
            }
        } else if (arg.rfind(name + "=", 0) == 0) {
            // Handle "--name=value" format
            return arg.substr(name.size() + 1);
        }
    }
    return def;
}

} // namespace utils
} // namespace sentio


