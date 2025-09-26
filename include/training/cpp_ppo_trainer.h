#pragma once

/**
 * @file cpp_ppo_trainer.h
 * @brief Comprehensive C++ PPO Training Module for Sentio Trading System
 * 
 * This module implements complete PPO training in C++ to eliminate feature mismatch
 * issues and ensure perfect consistency between training and inference.
 * 
 * Key Features:
 * - Exact Kochi feature calculation matching Python implementation
 * - Native C++ PPO training with proper action masking
 * - TorchScript export for seamless inference integration
 * - Performance-optimized training pipeline
 * - Comprehensive validation and testing
 * 
 * Based on lessons from ML Integration Guide:
 * - Uses torch.jit.script() for proper export (not trace)
 * - Implements FastFeatureCalculator for O(1) performance
 * - Ensures sigmoid activation for logit-to-probability conversion
 * - Maintains exact feature engineering consistency
 * - Includes comprehensive error handling and validation
 */

#include <torch/torch.h>
#include <torch/script.h>
#include "common/types.h"
#include "features/unified_feature_engine.h"  // 🔧 CONSOLIDATED: Use unified 91-feature engine
#include <vector>
#include <memory>
#include <string>
#include <map>

namespace sentio {
namespace training {

/**
 * @brief Configuration for C++ PPO Training
 */
struct CppPpoTrainerConfig {
    // Data parameters
    std::string dataset_path = "data/equities/QQQ_RTH_NH.csv";
    std::string output_dir = "artifacts/PPO/cpp_trained";
    
    // Model architecture
    int window_size = 30;           // Observation window (matching Kochi)
    int feature_dim = 71;           // Kochi feature dimension
    int hidden_dim = 256;           // Actor/Critic hidden dimension
    int num_layers = 2;             // Number of hidden layers
    
    // Training parameters
    int num_epochs = 100;           // Training epochs
    int batch_size = 64;            // Batch size for training
    double learning_rate = 1e-4;    // REDUCED learning rate for stability
    double gamma = 0.99;            // Discount factor
    double lambda_gae = 0.95;       // GAE parameter
    double clip_ratio = 0.2;        // PPO clip ratio
    double entropy_coef = 0.05;     // INCREASED entropy for more exploration
    double value_coef = 0.5;        // Value loss coefficient
    
    // Environment parameters
    double transaction_fee = 0.001; // Transaction fee (0.1%)
    double reward_scaling = 1.0;    // Reward scaling factor
    int max_episode_length = 5000;  // INCREASED episode length for proper training
    
    // Validation parameters
    double train_split = 0.8;       // Training data split
    int validation_frequency = 10;  // Validate every N epochs
    
    // Performance parameters
    bool enable_debug = true;       // Enable debug output
    bool enable_validation = true;  // Enable validation during training
    int num_threads = 4;            // Number of threads for data loading
};

/**
 * @brief PPO Actor-Critic Network
 * 
 * Implements the PPO policy and value networks with proper action masking support.
 * Architecture matches successful Kochi implementation but with C++ native training.
 */
class PPOActorCritic : public torch::nn::Module {
public:
    explicit PPOActorCritic(int input_dim, int hidden_dim, int action_dim, int num_layers = 2);
    
    struct PPOOutput {
        torch::Tensor action_logits;    // Raw action logits
        torch::Tensor action_probs;     // Action probabilities (after softmax)
        torch::Tensor state_values;     // State value estimates
        torch::Tensor log_probs;        // Log probabilities of actions
    };
    
    /**
     * @brief Forward pass through actor-critic network
     * @param observations Batch of observations [batch, window, features]
     * @param actions Optional actions for log prob calculation
     * @param action_masks Optional action masks [batch, num_actions]
     * @return PPO output structure
     */
    PPOOutput forward(const torch::Tensor& observations, 
                     const torch::Tensor& actions = torch::Tensor(),
                     const torch::Tensor& action_masks = torch::Tensor());
    
    /**
     * @brief Get action distribution for sampling
     */
    torch::Tensor get_action_distribution(const torch::Tensor& observations,
                                         const torch::Tensor& action_masks = torch::Tensor());
    
    /**
     * @brief Get critic head for gradient clipping access
     */
    torch::nn::Sequential& get_critic_head() { return critic_head_; }

private:
    // Shared feature extractor
    torch::nn::Sequential shared_net_;
    
    // Actor head (policy)
    torch::nn::Sequential actor_head_;
    
    // Critic head (value function)
    torch::nn::Sequential critic_head_;
    
    int input_dim_;
    int hidden_dim_;
    int action_dim_;
};

/**
 * @brief Trading Environment for PPO Training
 * 
 * Implements a complete trading environment that matches Kochi's environment
 * exactly, ensuring perfect consistency between training and inference.
 */
class TradingEnvironment {
public:
    struct EnvironmentConfig {
        double initial_balance = 100000.0;
        double transaction_fee = 0.001;
        double reward_scaling = 1.0;
        int window_size = 30;
        int max_episode_length = 1000;
        bool enable_action_masking = true;
    };
    
    enum class Action {
        SELL = 0,
        HOLD = 1,
        BUY = 2
    };
    
    enum class PositionState {
        HOLD = 0,
        LONG = 1,
        SHORT = 2
    };
    
    struct EnvironmentState {
        torch::Tensor observation;      // Current observation
        torch::Tensor action_mask;      // Valid actions mask
        double reward;                  // Step reward
        bool done;                      // Episode finished
        std::map<std::string, double> info;  // Additional info
    };
    
    explicit TradingEnvironment(const std::vector<Bar>& market_data);
    explicit TradingEnvironment(const std::vector<Bar>& market_data, 
                               const EnvironmentConfig& config);
    
    /**
     * @brief Reset environment to initial state
     */
    EnvironmentState reset();
    
    /**
     * @brief Execute action and return new state
     */
    EnvironmentState step(Action action);
    
    /**
     * @brief Get current observation
     */
    torch::Tensor get_observation() const;
    
    /**
     * @brief Get valid action mask
     */
    torch::Tensor get_action_mask() const;
    
    /**
     * @brief Check if episode is done
     */
    bool is_done() const;

private:
    std::vector<Bar> market_data_;
    EnvironmentConfig config_;
    std::unique_ptr<features::UnifiedFeatureEngine> feature_engine_;  // 🔧 CONSOLIDATED: Use unified 91-feature engine
    
    // Environment state
    int current_step_;
    int episode_length_;
    PositionState position_state_;
    double entry_price_;
    double cash_balance_;
    double total_equity_;
    int total_trades_;
    
    // Observation buffer
    std::deque<torch::Tensor> observation_buffer_;
    
    // Helper methods
    double calculate_reward(Action action);
    void update_position(Action action);
    torch::Tensor create_observation();
    torch::Tensor create_action_mask() const;
    void reset_state();
};

/**
 * @brief PPO Experience Buffer
 * 
 * Stores experiences for PPO training with proper GAE calculation.
 */
class PPOBuffer {
public:
    explicit PPOBuffer(int buffer_size, int obs_dim, int action_dim);
    
    /**
     * @brief Add experience to buffer
     */
    void add_experience(const torch::Tensor& obs,
                       const torch::Tensor& action,
                       const torch::Tensor& log_prob,
                       const torch::Tensor& value,
                       double reward,
                       bool done,
                       const torch::Tensor& action_mask = torch::Tensor());
    
    /**
     * @brief Calculate advantages using GAE
     */
    void calculate_advantages(double gamma, double lambda_gae, const torch::Tensor& next_value);
    
    /**
     * @brief Get training batch
     */
    struct TrainingBatch {
        torch::Tensor observations;
        torch::Tensor actions;
        torch::Tensor old_log_probs;
        torch::Tensor advantages;
        torch::Tensor returns;
        torch::Tensor values;
        torch::Tensor action_masks;
    };
    
    TrainingBatch get_batch();
    
    /**
     * @brief Clear buffer
     */
    void clear();
    
    /**
     * @brief Check if buffer is ready for training
     */
    bool is_ready() const;

private:
    int buffer_size_;
    int current_size_;
    
    // Experience storage
    torch::Tensor observations_;
    torch::Tensor actions_;
    torch::Tensor log_probs_;
    torch::Tensor values_;
    torch::Tensor rewards_;
    torch::Tensor dones_;
    torch::Tensor action_masks_;
    
    // Calculated values
    torch::Tensor advantages_;
    torch::Tensor returns_;
};

/**
 * @brief Main C++ PPO Trainer
 * 
 * Implements complete PPO training pipeline with all optimizations from
 * the ML Integration Guide to ensure maximum performance and reliability.
 */
class CppPpoTrainer {
public:
    explicit CppPpoTrainer(const CppPpoTrainerConfig& config = CppPpoTrainerConfig{});
    ~CppPpoTrainer() = default;
    
    /**
     * @brief Initialize trainer with data loading and validation
     */
    bool initialize();
    
    /**
     * @brief Run complete training process
     */
    bool train();
    
    /**
     * @brief Validate trained model
     */
    bool validate();
    
    /**
     * @brief Export model to TorchScript format
     */
    bool export_model();
    
    /**
     * @brief Get training statistics
     */
    struct TrainingStats {
        std::vector<double> episode_rewards;
        std::vector<double> policy_losses;
        std::vector<double> value_losses;
        std::vector<double> entropy_losses;
        std::vector<double> validation_rewards;
        double best_validation_reward = -std::numeric_limits<double>::infinity();
        int best_epoch = 0;
    };
    
    const TrainingStats& get_stats() const { return stats_; }

private:
    CppPpoTrainerConfig config_;
    
    // Model components
    std::shared_ptr<PPOActorCritic> model_;
    std::shared_ptr<torch::optim::Adam> optimizer_;
    
    // Data
    std::vector<Bar> train_data_;
    std::vector<Bar> validation_data_;
    
    // Training components
    std::unique_ptr<TradingEnvironment> train_env_;
    std::unique_ptr<TradingEnvironment> val_env_;
    std::unique_ptr<PPOBuffer> buffer_;
    
    // Statistics
    TrainingStats stats_;
    
    // Private methods
    bool load_data();
    void split_data();
    bool setup_model();
    bool setup_optimizer();
    
    // Training loop
    bool run_training_epoch(int epoch);
    double collect_experiences();
    void update_policy();
    double run_validation();
    
    // Loss calculations
    torch::Tensor calculate_policy_loss(const PPOBuffer::TrainingBatch& batch);
    torch::Tensor calculate_value_loss(const PPOBuffer::TrainingBatch& batch);
    torch::Tensor calculate_entropy_loss(const PPOBuffer::TrainingBatch& batch);
    
    // Export helpers
    bool create_export_wrapper();
    bool save_metadata();
    bool validate_export();
    
    // Utility methods
    void log_training_progress(int epoch, double reward, double policy_loss, 
                              double value_loss, double entropy_loss);
    void save_checkpoint(int epoch);
    bool load_checkpoint(const std::string& path);
};

/**
 * @brief TorchScript Export Wrapper
 * 
 * Creates a clean wrapper for TorchScript export that matches the interface
 * expected by the C++ PPO strategy implementation.
 */
class PPOExportWrapper : public torch::nn::Module {
public:
    explicit PPOExportWrapper(std::shared_ptr<PPOActorCritic> model);
    
    /**
     * @brief Forward method for TorchScript export
     * @param observations Flattened observations [batch, window_size * feature_dim]
     * @param action_masks Action masks [batch, num_actions]
     * @return Action logits [batch, num_actions]
     */
    torch::Tensor forward(const torch::Tensor& observations,
                         const torch::Tensor& action_masks = torch::Tensor());

private:
    std::shared_ptr<PPOActorCritic> model_;
    int window_size_;
    int feature_dim_;
};

} // namespace training
} // namespace sentio
