#pragma once

#include "ml/gru_model.h"
#include "common/types.h"
#include <torch/torch.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace sentio {
namespace training {

/**
 * @brief Training metrics for monitoring progress
 */
struct TrainingMetrics {
    double total_loss = 0.0;
    double direction_loss = 0.0;
    double magnitude_loss = 0.0;
    double confidence_loss = 0.0;
    double volatility_loss = 0.0;
    double regime_loss = 0.0;
    double accuracy = 0.0;
    int epoch = 0;
    bool is_validation = false;
};

/**
 * @brief Data sample for training
 */
struct TrainingSample {
    torch::Tensor features;     ///< [sequence_length, feature_dim]
    torch::Tensor direction;    ///< Target direction (0 or 1)
    torch::Tensor magnitude;    ///< Target magnitude
    torch::Tensor confidence;   ///< Target confidence
    torch::Tensor volatility;   ///< Target volatility
    torch::Tensor regime;       ///< Target regime (0-3)
};

/**
 * @brief Advanced GRU Trainer
 * 
 * This class handles the complete training pipeline for the GRU model:
 * - Data loading and preprocessing
 * - Feature engineering (53 comprehensive features)
 * - Multi-task loss computation
 * - Training loop with early stopping
 * - Model evaluation and validation
 * - Model export and metadata generation
 */
class GRUTrainer {
public:
    /**
     * @brief Constructor
     * @param config Training configuration
     */
    explicit GRUTrainer(const ml::GRUConfig& config);
    
    /**
     * @brief Load market data from CSV file
     * @param csv_path Path to market data CSV
     * @return Success status
     */
    bool load_data(const std::string& csv_path);
    
    /**
     * @brief Prepare training data with feature engineering
     * @return Success status
     */
    bool prepare_training_data();
    
    /**
     * @brief Train the model
     * @return Success status
     */
    bool train();
    
    /**
     * @brief Evaluate model on validation set
     * @return Validation metrics
     */
    TrainingMetrics evaluate();
    
    /**
     * @brief Export trained model and metadata
     * @return Success status
     */
    bool export_model();
    
    /**
     * @brief Get current model
     */
    std::shared_ptr<ml::AdvancedGRUModelImpl> get_model() { return model_; }
    
    /**
     * @brief Set progress callback
     */
    void set_progress_callback(std::function<void(const TrainingMetrics&)> callback) {
        progress_callback_ = callback;
    }

private:
    ml::GRUConfig config_;
    std::shared_ptr<ml::AdvancedGRUModelImpl> model_;
    std::unique_ptr<torch::optim::AdamW> optimizer_;
    // Note: CosineAnnealingLR not available in all LibTorch versions
    // std::unique_ptr<torch::optim::CosineAnnealingLR> scheduler_;
    
    // Data
    std::vector<Bar> raw_data_;
    std::vector<TrainingSample> train_samples_;
    std::vector<TrainingSample> val_samples_;
    
    // Feature engineering
    std::vector<std::string> feature_names_;
    std::vector<double> feature_means_;
    std::vector<double> feature_scales_;
    
    // Training state
    std::vector<TrainingMetrics> train_history_;
    std::vector<TrainingMetrics> val_history_;
    double best_val_loss_ = std::numeric_limits<double>::max();
    int patience_counter_ = 0;
    
    // Callback
    std::function<void(const TrainingMetrics&)> progress_callback_;
    
    /**
     * @brief Create comprehensive features from market data
     * @param bars Input market data
     * @return Feature matrix [num_samples, feature_dim]
     */
    torch::Tensor create_comprehensive_features(const std::vector<Bar>& bars);
    
    /**
     * @brief Create multi-task targets
     * @param bars Input market data
     * @return Target tensors
     */
    std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor> 
    create_multi_task_targets(const std::vector<Bar>& bars);
    
    /**
     * @brief Create training sequences
     * @param features Feature matrix
     * @param targets Target tensors
     */
    void create_training_sequences(const torch::Tensor& features,
                                   const std::tuple<torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor, torch::Tensor>& targets);
    
    /**
     * @brief Compute multi-objective loss
     * @param predictions Model predictions
     * @param targets Ground truth targets
     * @return Loss components
     */
    std::tuple<torch::Tensor, TrainingMetrics> compute_multi_objective_loss(
        const ml::GRUOutput& predictions, const TrainingSample& targets);
    
    /**
     * @brief Train one epoch
     * @return Training metrics
     */
    TrainingMetrics train_epoch();
    
    /**
     * @brief Validate one epoch
     * @return Validation metrics
     */
    TrainingMetrics validate_epoch();
    
    /**
     * @brief Save model checkpoint
     */
    void save_checkpoint(int epoch);
    
    /**
     * @brief Calculate technical indicators
     */
    double calculate_sma(const std::vector<double>& data, int period, int index) const;
    double calculate_rsi(const std::vector<double>& prices, int period, int index) const;
    double calculate_bollinger_position(const std::vector<double>& prices, int period, int index) const;
    std::tuple<double, double, double> calculate_macd(const std::vector<double>& prices, int index) const;
};

} // namespace training
} // namespace sentio
