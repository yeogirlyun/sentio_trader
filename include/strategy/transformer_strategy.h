#pragma once

#include "strategy/ml_strategy_base.h"
#include "strategy/transformer_model.h"
#include "features/unified_feature_engine.h"
#include "common/feature_sequence_manager.h" // NEW: For proper sequence management
#include "common/types.h"
#include <torch/torch.h>
#include <memory>

namespace sentio {

// =============================================================================
// Module: strategy/transformer_strategy.h (Rewritten)
// Purpose: Defines the inference-only Transformer strategy.
//
// Core Idea:
// This is a clean, high-performance implementation for production inference.
// It relies entirely on the model and metadata produced by the C++ trainer,
// ensuring perfect consistency. All previous hacks and flawed logic have been
// removed.
// =============================================================================

class TransformerStrategy : public MLStrategyBase {
public:
    struct Config {
        std::string model_path;
        std::string metadata_path;
        double confidence_threshold = 0.5; // Act on signals with >50% confidence
    };

    explicit TransformerStrategy(const StrategyConfig& base_config, const Config& tfm_config);
    ~TransformerStrategy() override = default;

    bool initialize();
    std::string get_name() const { return "Transformer_v2"; }

protected:
    SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    void update_indicators(const Bar& bar) override;
    
    // MLStrategyBase pure virtual methods
    void apply_metadata_config(const json_utils::JsonValue& metadata) override;
    std::string get_model_path() const override;
    std::string get_metadata_path() const override;
    size_t get_sequence_length() const override;
    int get_feature_count() const override;

private:
    bool load_metadata();
    
    Config config_;
    TransformerModelConfig model_config_;

    // ✅ UPDATED: Load PyTorch model directly (not TorchScript due to API limitations)
    std::unique_ptr<TransformerModel> pytorch_model_{nullptr};

    // Use the unified feature engine for consistency
    std::unique_ptr<features::UnifiedFeatureEngine> feature_engine_{nullptr};
    
    // NEW: Proper sequence management to fix the critical temporal bug
    std::unique_ptr<FeatureSequenceManager> sequence_manager_{nullptr};
    
    // Feature normalization parameters loaded from metadata
    torch::Tensor feature_means_;
    torch::Tensor feature_stds_;
};

} // namespace sentio
