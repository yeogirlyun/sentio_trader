#include "strategy/sigor_config.h"
#include "common/utils.h"
#include <fstream>

namespace sentio {

SigorConfig SigorConfig::defaults() {
    return SigorConfig{};
}

SigorConfig SigorConfig::from_file(const std::string& path) {
    SigorConfig c = SigorConfig::defaults();
    // Minimal JSON loader using utils::from_json (flat map)
    // If the file can't be read via this helper, just return defaults.
    // Users can extend to YAML/JSON libs later.
    // Read file contents
    std::ifstream in(path);
    if (!in.is_open()) return c;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto m = utils::from_json(content);
    auto getd = [&](const char* k, double& dst){ if (m.count(k)) dst = std::stod(m[k]); };
    auto geti = [&](const char* k, int& dst){ if (m.count(k)) dst = std::stoi(m[k]); };

    getd("k", c.k);
    getd("w_boll", c.w_boll); getd("w_rsi", c.w_rsi); getd("w_mom", c.w_mom);
    getd("w_vwap", c.w_vwap); getd("w_orb", c.w_orb); getd("w_ofi", c.w_ofi); getd("w_vol", c.w_vol);
    geti("win_boll", c.win_boll); geti("win_rsi", c.win_rsi); geti("win_mom", c.win_mom);
    geti("win_vwap", c.win_vwap); geti("orb_opening_bars", c.orb_opening_bars);
    return c;
}

} // namespace sentio


