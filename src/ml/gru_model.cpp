#include "ml/gru_model.h"
#include <iostream>
#include <filesystem>

namespace sentio {
namespace ml {

AdvancedGRUModelImpl::AdvancedGRUModelImpl(const GRUConfig& config) : config_(config) {
    initialize_components();
}

void AdvancedGRUModelImpl::initialize_components() {
    // Feature encoding layer
    feature_encoder_ = register_module("feature_encoder", 
        torch::nn::Linear(config_.feature_dim, config_.hidden_dim));
    feature_norm_ = register_module("feature_norm", 
        torch::nn::LayerNorm(torch::nn::LayerNormOptions({config_.hidden_dim})));
    feature_dropout_ = register_module("feature_dropout", 
        torch::nn::Dropout(config_.dropout));
    
    // Multi-layer GRU with layer normalization
    gru_layers_.reserve(config_.num_layers);
    layer_norms_.reserve(config_.num_layers);
    
    for (int i = 0; i < config_.num_layers; ++i) {
        auto gru = torch::nn::GRU(torch::nn::GRUOptions(config_.hidden_dim, config_.hidden_dim)
            .batch_first(true)
            .dropout(i < config_.num_layers - 1 ? config_.dropout : 0.0));
        
        gru_layers_.push_back(register_module("gru_" + std::to_string(i), gru));
        
        auto norm = torch::nn::LayerNorm(torch::nn::LayerNormOptions({config_.hidden_dim}));
        layer_norms_.push_back(register_module("norm_" + std::to_string(i), norm));
    }
    
    // Multi-head attention
    attention_ = register_module("attention", 
        torch::nn::MultiheadAttention(torch::nn::MultiheadAttentionOptions(config_.hidden_dim, config_.num_heads)
            .dropout(config_.dropout)));
    attention_norm_ = register_module("attention_norm", 
        torch::nn::LayerNorm(torch::nn::LayerNormOptions({config_.hidden_dim})));
    
    // Multi-task prediction heads
    if (config_.enable_multi_task) {
        direction_head_ = register_module("direction_head", 
            create_prediction_head(1, false, false));  // Raw logits for BCEWithLogitsLoss
        magnitude_head_ = register_module("magnitude_head", 
            create_prediction_head(1, false, false, true)); // ReLU output
        confidence_head_ = register_module("confidence_head", 
            create_prediction_head(1, true, false));  // Sigmoid output
        volatility_head_ = register_module("volatility_head", 
            create_prediction_head(1, false, false, true)); // ReLU output
        regime_head_ = register_module("regime_head", 
            create_prediction_head(4, false, true));  // Softmax output (4 regimes)
    } else {
        // Single task - just direction
        direction_head_ = register_module("direction_head", 
            create_prediction_head(1, false, false));  // Raw logits for BCEWithLogitsLoss
    }
}

torch::nn::Sequential AdvancedGRUModelImpl::create_prediction_head(int output_dim, bool use_sigmoid, bool use_softmax, bool use_relu) {
    auto head = torch::nn::Sequential();
    
    // First layer
    head->push_back(torch::nn::Linear(config_.hidden_dim, config_.hidden_dim / 2));
    head->push_back(torch::nn::ReLU());
    head->push_back(torch::nn::Dropout(config_.dropout));
    
    // Output layer
    head->push_back(torch::nn::Linear(config_.hidden_dim / 2, output_dim));
    
    // Activation
    if (use_sigmoid) {
        head->push_back(torch::nn::Sigmoid());
    } else if (use_softmax) {
        head->push_back(torch::nn::Softmax(torch::nn::SoftmaxOptions(-1)));
    } else if (use_relu) {
        head->push_back(torch::nn::ReLU()); // For magnitude and volatility
    }
    // No activation for raw logits (direction)
    
    return head;
}

GRUOutput AdvancedGRUModelImpl::forward(const torch::Tensor& input) {
    // Input shape: [batch_size, sequence_length, feature_dim]
    auto x = input;
    
    // Feature encoding
    x = feature_encoder_(x);
    x = feature_norm_(x);
    x = torch::relu(x);
    x = feature_dropout_(x);
    
    // Multi-layer GRU with residual connections
    for (size_t i = 0; i < gru_layers_.size(); ++i) {
        auto residual = x;
        
        // GRU forward pass
        auto gru_output = gru_layers_[i](x);
        x = std::get<0>(gru_output); // Get output tensor, ignore hidden state
        
        // Layer normalization
        x = layer_norms_[i](x);
        
        // Residual connection (skip first layer to allow dimension matching)
        if (i > 0) {
            x = x + residual;
        }
    }
    
    // Multi-head attention
    auto attention_output = attention_(x, x, x);
    auto attended = std::get<0>(attention_output); // Get output tensor, ignore attention weights
    
    // Attention residual connection and normalization
    attended = attention_norm_(attended + x);
    
    // Get last timestep output for prediction
    auto last_timestep = attended.select(1, -1); // [batch_size, hidden_dim]
    
    // Generate predictions
    GRUOutput output;
    
    if (config_.enable_multi_task) {
        output.direction = direction_head_->forward(last_timestep);
        output.magnitude = magnitude_head_->forward(last_timestep);
        output.confidence = confidence_head_->forward(last_timestep);
        output.volatility = volatility_head_->forward(last_timestep);
        output.regime = regime_head_->forward(last_timestep);
    } else {
        output.direction = direction_head_->forward(last_timestep);
        // Set other outputs to neutral values
        auto batch_size = input.size(0);
        output.magnitude = torch::zeros({batch_size, 1});
        output.confidence = torch::full({batch_size, 1}, 0.5);
        output.volatility = torch::full({batch_size, 1}, 0.1);
        output.regime = torch::full({batch_size, 4}, 0.25);
    }
    
    return output;
}

size_t AdvancedGRUModelImpl::get_parameter_count() const {
    size_t count = 0;
    for (const auto& param : parameters()) {
        count += param.numel();
    }
    return count;
}

void AdvancedGRUModelImpl::save_model(const std::string& path) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    torch::save(shared_from_this(), path);
    std::cout << "Model saved to: " << path << std::endl;
}

void AdvancedGRUModelImpl::load_model(const std::string& path) {
    try {
        torch::serialize::InputArchive archive;
        archive.load_from(path);
        load(archive);
        
        // Verify weights are loaded
        auto params = parameters();
        if (params.size() == 0) {
            throw std::runtime_error("No parameters loaded");
        }
        
        // Check for non-zero weights
        bool has_nonzero = false;
        for (const auto& p : params) {
            if (torch::any(p != 0).item<bool>()) {
                has_nonzero = true;
                break;
            }
        }
        if (!has_nonzero) {
            throw std::runtime_error("All parameters are zero");
        }
        
        std::cout << "Model loaded successfully with " << params.size() 
                  << " parameter tensors from: " << path << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load model from " + path + ": " + e.what());
    }
}

} // namespace ml
} // namespace sentio
