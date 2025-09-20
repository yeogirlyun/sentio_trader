#include "common/utils.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace sentio {
namespace utils {

// --------------------------------- Helpers ----------------------------------
namespace {
    // Trim whitespace from both ends of a string
    static inline std::string trim(const std::string& s) {
        const char* ws = " \t\n\r\f\v";
        const auto start = s.find_first_not_of(ws);
        if (start == std::string::npos) return "";
        const auto end = s.find_last_not_of(ws);
        return s.substr(start, end - start + 1);
    }
}

// ----------------------------- File I/O utilities ----------------------------
std::vector<Bar> read_csv_data(const std::string& path) {
    std::vector<Bar> bars;
    std::ifstream file(path);
    if (!file.is_open()) {
        return bars;
    }

    std::string line;
    // Skip header
    std::getline(file, line);
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string item;
        Bar b{};

        // symbol
        std::getline(ss, item, ',');
        b.symbol = trim(item);
        // timestamp_ms
        std::getline(ss, item, ',');
        b.timestamp_ms = std::stoll(trim(item));
        // open
        std::getline(ss, item, ',');
        b.open = std::stod(trim(item));
        // high
        std::getline(ss, item, ',');
        b.high = std::stod(trim(item));
        // low
        std::getline(ss, item, ',');
        b.low = std::stod(trim(item));
        // close
        std::getline(ss, item, ',');
        b.close = std::stod(trim(item));
        // volume
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
    // Extremely small parser for a flat string map {"k":"v",...}
    std::map<std::string, std::string> m;
    std::string s = json_str;
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); }), s.end());
    if (s.size() < 2 || s.front() != '{' || s.back() != '}') return m;
    s = s.substr(1, s.size() - 2);
    std::stringstream ss(s);
    std::string pair;
    while (std::getline(ss, pair, ',')) {
        auto colon = pair.find(':');
        if (colon == std::string::npos) continue;
        auto key = pair.substr(0, colon);
        auto val = pair.substr(colon + 1);
        if (key.size() >= 2 && key.front() == '"' && key.back() == '"') key = key.substr(1, key.size() - 2);
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
        m[key] = val;
    }
    return m;
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
    auto ts = current_timestamp_str();
    auto hash = calculate_sha256(prefix + ts).substr(0, 8);
    // Format YYYYMMDDHHMMSS from ts
    std::string compact = ts;
    compact.erase(std::remove(compact.begin(), compact.end(), '-'), compact.end());
    compact.erase(std::remove(compact.begin(), compact.end(), ' '), compact.end());
    compact.erase(std::remove(compact.begin(), compact.end(), ':'), compact.end());
    return prefix + "-" + compact + "-" + hash;
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

} // namespace utils
} // namespace sentio


