#pragma once

#include <torch/torch.h>
#include <memory>
#include <string>
#include <vector>

namespace sentio {
namespace ml {

/**
 * @brief Configuration for Advanced GRU Model
 */
struct GRUConfig {
    // Model architecture
    int feature_dim = 53;           ///< Number of input features
    int sequence_length = 64;       ///< Input sequence length
    int hidden_dim = 128;           ///< GRU hidden dimension
    int num_layers = 3;             ///< Number of GRU layers
    int num_heads = 8;              ///< Multi-head attention heads
    float dropout = 0.1f;           ///< Dropout rate
    
    // Multi-task outputs
    bool enable_multi_task = true;  ///< Enable multi-task learning
    
    // Training parameters
    int batch_size = 64;
    int epochs = 50;
    float learning_rate = 0.001f;
    float weight_decay = 1e-4f;
    float grad_clip = 1.0f;
    
    // Data parameters
    float train_split = 0.8f;
    float val_split = 0.2f;
    
    // Early stopping
    int patience = 15;
    float min_delta = 1e-4f;
    
    // Output
    std::string output_dir = "artifacts/GRU/cpp_trained";
    bool save_checkpoints = true;
    int checkpoint_frequency = 10;
};

/**
 * @brief Multi-task output structure for GRU predictions
 */
struct GRUOutput {
    torch::Tensor direction;    ///< Buy/sell probability [0,1]
    torch::Tensor magnitude;    ///< Expected return magnitude
    torch::Tensor confidence;   ///< Model confidence [0,1]
    torch::Tensor volatility;   ///< Expected volatility
    torch::Tensor regime;       ///< Market regime probabilities [4 classes]
};

/**
 * @brief Advanced GRU Model Implementation
 * 
 * This class implements a sophisticated GRU-based trading model with:
 * - Multi-layer GRU with residual connections
 * - Multi-head attention mechanism
 * - Multi-task learning outputs
 * - Layer normalization and dropout
 * - Optimized for financial time series prediction
 */
class AdvancedGRUModelImpl : public torch::nn::Module {
public:
    /**
     * @brief Constructor
     * @param config Model configuration
     */
    explicit AdvancedGRUModelImpl(const GRUConfig& config);
    
    /**
     * @brief Forward pass
     * @param input Input tensor [batch_size, sequence_length, feature_dim]
     * @return GRUOutput Multi-task predictions
     */
    GRUOutput forward(const torch::Tensor& input);
    
    /**
     * @brief Get model configuration
     */
    const GRUConfig& get_config() const { return config_; }
    
    /**
     * @brief Get parameter count
     */
    size_t get_parameter_count() const;
    
    /**
     * @brief Save model state
     */
    void save_model(const std::string& path);
    
    /**
     * @brief Load model state
     */
    void load_model(const std::string& path);

private:
    GRUConfig config_;
    
    // Feature encoding
    torch::nn::Linear feature_encoder_{nullptr};
    torch::nn::LayerNorm feature_norm_{nullptr};
    torch::nn::Dropout feature_dropout_{nullptr};
    
    // Multi-layer GRU
    std::vector<torch::nn::GRU> gru_layers_;
    std::vector<torch::nn::LayerNorm> layer_norms_;
    
    // Multi-head attention
    torch::nn::MultiheadAttention attention_{nullptr};
    torch::nn::LayerNorm attention_norm_{nullptr};
    
    // Multi-task prediction heads
    torch::nn::Sequential direction_head_{nullptr};
    torch::nn::Sequential magnitude_head_{nullptr};
    torch::nn::Sequential confidence_head_{nullptr};
    torch::nn::Sequential volatility_head_{nullptr};
    torch::nn::Sequential regime_head_{nullptr};
    
    /**
     * @brief Initialize model components
     */
    void initialize_components();
    
    /**
     * @brief Create prediction head
     */
    torch::nn::Sequential create_prediction_head(int output_dim, bool use_sigmoid = false, bool use_softmax = false, bool use_relu = false);
};

// Convenience typedef
TORCH_MODULE(AdvancedGRUModel);

} // namespace ml
} // namespace sentio
