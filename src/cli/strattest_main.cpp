#include "strategy/strategy_component.h"
#include "strategy/sigor_strategy.h"
#include "strategy/sigor_config.h"
#include "common/utils.h"

#include <iostream>
#include <string>
#include <map>
#include <filesystem>

// =============================================================================
// Executable: strattest
// Purpose: Minimal CLI to run a strategy over a dataset and export signals.
//
// Usage (bare minimum):
//   strattest [--dataset PATH] [--out PATH] [--strategy NAME] [--format jsonl|csv]
// Defaults:
//   --dataset=data/equities/QQQ_RTH_NH.csv
//   --out auto: <strategy>_signals_<YYYYMMDD_HHMMSS>.(jsonl|csv)
//
// Core flow:
// - Parse flags
// - Instantiate strategy (default: Sigor)
// - Process dataset and export signals
// =============================================================================

namespace {
    // Use common get_arg function from utils
    using sentio::utils::get_arg;

    // Convert "YYYY-MM-DD HH:MM:SS" -> "YYYYMMDD_HHMMSS"
    std::string compact_timestamp(const std::string& ts) {
        std::string s = ts;
        for (char& c : s) {
            if (c == '-') c = '\0';
            else if (c == ' ') c = '_';
            else if (c == ':') c = '\0';
        }
        std::string out;
        out.reserve(s.size());
        for (char c : s) { if (c != '\0') out.push_back(c); }
        return out;
    }
}

int main(int argc, char** argv) {
    std::string dataset = get_arg(argc, argv, "--dataset");
    std::string out = get_arg(argc, argv, "--out");
    const std::string strategy = get_arg(argc, argv, "--strategy", "sigor");
    const std::string format = get_arg(argc, argv, "--format", "jsonl");
    const std::string cfg_path = get_arg(argc, argv, "--config", "");

    if (dataset.empty()) {
        dataset = "data/equities/QQQ_RTH_NH.csv";
    }
    if (out.empty()) {
        std::string ts = sentio::utils::current_timestamp_str();
        std::string ts_compact = compact_timestamp(ts);
        std::string base = strategy.empty() ? std::string("strategy") : strategy;
        std::string ext = (format == "csv") ? ".csv" : ".jsonl";
        // ensure directory exists
        std::error_code ec;
        std::filesystem::create_directories("data/signals", ec);
        out = std::string("data/signals/") + base + "_signals_" + ts_compact + ext;
    }

    sentio::StrategyComponent::StrategyConfig cfg;
    cfg.name = strategy;
    cfg.version = "0.1";
    cfg.warmup_bars = 20; // minimal warmup for example

    std::unique_ptr<sentio::StrategyComponent> strat;
    // For now, support only Sigor; can add more mappings later
    auto sigor = std::make_unique<sentio::SigorStrategy>(cfg);
    if (!cfg_path.empty()) {
        auto scfg = sentio::SigorConfig::defaults();
        scfg = sentio::SigorConfig::from_file(cfg_path);
        sigor->set_config(scfg);
    }
    strat = std::move(sigor);

    auto signals = strat->process_dataset(dataset, cfg.name, {});
    // Inject market data path into metadata for each signal
    for (auto& s : signals) {
        s.metadata["market_data_path"] = dataset;
    }
    bool ok = strat->export_signals(signals, out, format);
    if (!ok) {
        std::cerr << "ERROR: failed to export signals to " << out << "\n";
        return 2;
    }

    std::cout << "Exported " << signals.size() << " signals to " << out << " (" << format << ")\n";
    return 0;
}


