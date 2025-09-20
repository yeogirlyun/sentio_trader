#include "strategy/signal_output.h"
#include "common/utils.h"

#include <sstream>

namespace sentio {

std::string SignalOutput::to_json() const {
    std::map<std::string, std::string> m;
    m["timestamp_ms"] = std::to_string(timestamp_ms);
    m["bar_index"] = std::to_string(bar_index);
    m["symbol"] = symbol;
    m["probability"] = std::to_string(probability);
    m["confidence"] = std::to_string(confidence);
    m["strategy_name"] = strategy_name;
    m["strategy_version"] = strategy_version;
    // Metadata could be nested; for simplicity, omit or flatten as needed.
    return utils::to_json(m);
}

std::string SignalOutput::to_csv() const {
    std::ostringstream os;
    os << timestamp_ms << ','
       << bar_index << ','
       << symbol << ','
       << probability << ','
       << confidence << ','
       << strategy_name << ','
       << strategy_version;
    return os.str();
}

SignalOutput SignalOutput::from_json(const std::string& json_str) {
    SignalOutput s;
    auto m = utils::from_json(json_str);
    if (m.count("timestamp_ms")) s.timestamp_ms = std::stoll(m["timestamp_ms"]);
    if (m.count("bar_index")) s.bar_index = std::stoi(m["bar_index"]);
    if (m.count("symbol")) s.symbol = m["symbol"];
    if (m.count("probability")) s.probability = std::stod(m["probability"]);
    if (m.count("confidence")) s.confidence = std::stod(m["confidence"]);
    if (m.count("strategy_name")) s.strategy_name = m["strategy_name"];
    if (m.count("strategy_version")) s.strategy_version = m["strategy_version"];
    return s;
}

} // namespace sentio


