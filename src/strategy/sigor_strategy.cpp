#include "strategy/sigor_strategy.h"

#include <cmath>
#include <algorithm>
#include <limits>

namespace sentio {

SigorStrategy::SigorStrategy(const StrategyConfig& config)
    : StrategyComponent(config) {}

SignalOutput SigorStrategy::generate_signal(const Bar& bar, int bar_index) {
    // Compute detector probabilities
    double p1 = prob_bollinger_(bar);
    double p2 = prob_rsi_14_();
    double p3 = prob_momentum_(10, 50.0);
    double p4 = prob_vwap_reversion_(20);
    double p5 = prob_orb_daily_(30);
    double p6 = prob_ofi_proxy_(bar);
    double p7 = prob_volume_surge_scaled_(20);

    double p_final = aggregate_probability(p1, p2, p3, p4, p5, p6, p7);
    double c_final = calculate_confidence(p1, p2, p3, p4, p5, p6, p7);

    SignalOutput s;
    s.timestamp_ms = bar.timestamp_ms;
    s.bar_index = bar_index;
    s.symbol = bar.symbol;
    s.probability = p_final;
    s.confidence = c_final;
    s.metadata["warmup_complete"] = is_warmed_up() ? "true" : "false";
    s.metadata["detectors"] = "boll,rsi,mom,vwap,orb,ofi,vol";
    return s;
}

void SigorStrategy::update_indicators(const Bar& bar) {
    StrategyComponent::update_indicators(bar);
    closes_.push_back(bar.close);
    highs_.push_back(bar.high);
    lows_.push_back(bar.low);
    volumes_.push_back(bar.volume);
    timestamps_.push_back(bar.timestamp_ms);
    if (closes_.size() > 1) {
        double delta = closes_[closes_.size() - 1] - closes_[closes_.size() - 2];
        gains_.push_back(std::max(0.0, delta));
        losses_.push_back(std::max(0.0, -delta));
    } else {
        gains_.push_back(0.0);
        losses_.push_back(0.0);
    }
    // Keep buffers bounded
    const size_t cap = 2048;
    auto trim = [cap](auto& vec){ if (vec.size() > cap) vec.erase(vec.begin(), vec.begin() + (vec.size() - cap)); };
    trim(closes_); trim(highs_); trim(lows_); trim(volumes_); trim(timestamps_); trim(gains_); trim(losses_);
}

bool SigorStrategy::is_warmed_up() const {
    return StrategyComponent::is_warmed_up();
}

// ------------------------------ Detectors ------------------------------------
double SigorStrategy::prob_bollinger_(const Bar& bar) const {
    const int w = 20;
    if (static_cast<int>(closes_.size()) < w) return 0.5;
    double mean = compute_sma(closes_, w);
    double sd = compute_stddev(closes_, w, mean);
    if (sd <= 1e-12) return 0.5;
    double z = (bar.close - mean) / sd;
    return clamp01(0.5 + 0.5 * std::tanh(z / 2.0));
}

double SigorStrategy::prob_rsi_14_() const {
    const int w = 14;
    if (static_cast<int>(gains_.size()) < w + 1) return 0.5;
    double rsi = compute_rsi(w); // 0..100
    return clamp01((rsi - 50.0) / 100.0 * 1.0 + 0.5); // scale around 0.5
}

double SigorStrategy::prob_momentum_(int window, double scale) const {
    if (window <= 0 || static_cast<int>(closes_.size()) <= window) return 0.5;
    double curr = closes_.back();
    double prev = closes_[closes_.size() - static_cast<size_t>(window) - 1];
    if (prev <= 1e-12) return 0.5;
    double ret = (curr - prev) / prev;
    return clamp01(0.5 + 0.5 * std::tanh(ret * scale));
}

double SigorStrategy::prob_vwap_reversion_(int window) const {
    if (window <= 0 || static_cast<int>(closes_.size()) < window) return 0.5;
    double num = 0.0, den = 0.0;
    for (int i = static_cast<int>(closes_.size()) - window; i < static_cast<int>(closes_.size()); ++i) {
        double tp = (highs_[i] + lows_[i] + closes_[i]) / 3.0;
        double v = volumes_[i];
        num += tp * v;
        den += v;
    }
    if (den <= 1e-12) return 0.5;
    double vwap = num / den;
    double z = (closes_.back() - vwap) / std::max(1e-8, std::fabs(vwap));
    return clamp01(0.5 - 0.5 * std::tanh(z)); // above VWAP -> mean-revert bias
}

double SigorStrategy::prob_orb_daily_(int opening_window_bars) const {
    if (timestamps_.empty()) return 0.5;
    // Compute day bucket from epoch days
    int64_t day = timestamps_.back() / 86400000LL;
    // Find start index of current day
    int start = static_cast<int>(timestamps_.size()) - 1;
    while (start > 0 && (timestamps_[static_cast<size_t>(start - 1)] / 86400000LL) == day) {
        --start;
    }
    int end_open = std::min(static_cast<int>(timestamps_.size()), start + opening_window_bars);
    double hi = -std::numeric_limits<double>::infinity();
    double lo =  std::numeric_limits<double>::infinity();
    for (int i = start; i < end_open; ++i) {
        hi = std::max(hi, highs_[static_cast<size_t>(i)]);
        lo = std::min(lo, lows_[static_cast<size_t>(i)]);
    }
    if (!std::isfinite(hi) || !std::isfinite(lo)) return 0.5;
    double c = closes_.back();
    if (c > hi) return 0.7;     // breakout long bias
    if (c < lo) return 0.3;     // breakout short bias
    return 0.5;                  // inside range
}

double SigorStrategy::prob_ofi_proxy_(const Bar& bar) const {
    // Proxy OFI using bar geometry: (close-open)/(high-low) weighted by volume
    double range = std::max(1e-8, bar.high - bar.low);
    double ofi = ((bar.close - bar.open) / range) * std::tanh(bar.volume / 1e6);
    return clamp01(0.5 + 0.25 * ofi); // small influence
}

double SigorStrategy::prob_volume_surge_scaled_(int window) const {
    if (window <= 0 || static_cast<int>(volumes_.size()) < window) return 0.5;
    double v_now = volumes_.back();
    double v_ma = compute_sma(volumes_, window);
    if (v_ma <= 1e-12) return 0.5;
    double ratio = v_now / v_ma; // >1 indicates surge
    double adj = std::tanh((ratio - 1.0) * 1.0); // [-1,1]
    // Scale towards current momentum side
    double p_m = prob_momentum_(10, 50.0);
    double dir = (p_m >= 0.5) ? 1.0 : -1.0;
    return clamp01(0.5 + 0.25 * adj * dir);
}

double SigorStrategy::aggregate_probability(double p1, double p2, double p3,
                                            double p4, double p5, double p6, double p7) const {
    // Option A: Log-odds fusion with weights and sharpness k
    const double probs[7] = {p1, p2, p3, p4, p5, p6, p7};
    const double ws[7] = {cfg_.w_boll, cfg_.w_rsi, cfg_.w_mom, cfg_.w_vwap, cfg_.w_orb, cfg_.w_ofi, cfg_.w_vol};
    double num = 0.0, den = 0.0;
    for (int i = 0; i < 7; ++i) {
        double p = std::clamp(probs[i], 1e-6, 1.0 - 1e-6);
        double l = std::log(p / (1.0 - p));
        num += ws[i] * l;
        den += ws[i];
    }
    double L = (den > 1e-12) ? (num / den) : 0.0;
    double k = cfg_.k;
    double P = 1.0 / (1.0 + std::exp(-k * L));
    return P;
}

double SigorStrategy::calculate_confidence(double p1, double p2, double p3,
                                           double p4, double p5, double p6, double p7) const {
    double arr[7] = {p1, p2, p3, p4, p5, p6, p7};
    int long_votes = 0, short_votes = 0;
    double max_strength = 0.0;
    for (double p : arr) {
        if (p > 0.5) ++long_votes; else if (p < 0.5) ++short_votes;
        max_strength = std::max(max_strength, std::fabs(p - 0.5));
    }
    double agreement = std::max(long_votes, short_votes) / 7.0; // 0..1
    return clamp01(0.4 + 0.6 * std::max(agreement, max_strength));
}

// ------------------------------ Helpers --------------------------------------
double SigorStrategy::compute_sma(const std::vector<double>& v, int window) const {
    if (window <= 0 || static_cast<int>(v.size()) < window) return 0.0;
    double sum = 0.0;
    for (int i = static_cast<int>(v.size()) - window; i < static_cast<int>(v.size()); ++i) sum += v[static_cast<size_t>(i)];
    return sum / static_cast<double>(window);
}

double SigorStrategy::compute_stddev(const std::vector<double>& v, int window, double mean) const {
    if (window <= 0 || static_cast<int>(v.size()) < window) return 0.0;
    double acc = 0.0;
    for (int i = static_cast<int>(v.size()) - window; i < static_cast<int>(v.size()); ++i) {
        double d = v[static_cast<size_t>(i)] - mean;
        acc += d * d;
    }
    return std::sqrt(acc / static_cast<double>(window));
}

double SigorStrategy::compute_rsi(int window) const {
    if (window <= 0 || static_cast<int>(gains_.size()) < window + 1) return 50.0;
    double avg_gain = 0.0, avg_loss = 0.0;
    for (int i = static_cast<int>(gains_.size()) - window; i < static_cast<int>(gains_.size()); ++i) {
        avg_gain += gains_[static_cast<size_t>(i)];
        avg_loss += losses_[static_cast<size_t>(i)];
    }
    avg_gain /= static_cast<double>(window);
    avg_loss /= static_cast<double>(window);
    if (avg_loss <= 1e-12) return 100.0;
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

} // namespace sentio


