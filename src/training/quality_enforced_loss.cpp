#include "training/quality_enforced_loss.h"

namespace sentio::training {

QualityEnforcedLoss::LossComponents QualityEnforcedLoss::compute_loss(torch::Tensor predictions, torch::Tensor log_var, torch::Tensor targets) {
    LossComponents components;
    
    // Ensure consistent shapes and flatten all tensors
    auto pred_flat = predictions.flatten();
    auto log_var_flat = log_var.flatten();
    auto targets_flat = targets.flatten();
    
    // Ensure all tensors have the same size
    int64_t batch_size = pred_flat.size(0);
    if (log_var_flat.size(0) != batch_size) {
        log_var_flat = log_var_flat.expand({batch_size});
    }
    if (targets_flat.size(0) != batch_size) {
        targets_flat = targets_flat.expand({batch_size});
    }
    
    // 1. Base prediction loss (Huber for robustness)
    components.prediction_loss = torch::huber_loss(pred_flat, targets_flat, at::Reduction::Mean, /*delta=*/0.1) * base_weight_;
    
    // 2. Anti-collapse diversity enforcement
    torch::Tensor pred_std = torch::std(pred_flat, /*dim=*/0, /*unbiased=*/false, /*keepdim=*/false);
    torch::Tensor diversity_target = torch::tensor(0.1, pred_std.options());
    components.diversity_bonus = -torch::abs(pred_std - diversity_target) * diversity_weight_;
    
    // 3. Confidence must vary with prediction quality
    torch::Tensor pred_errors = torch::abs(pred_flat - targets_flat);
    torch::Tensor target_log_var = torch::log(pred_errors + 1e-4); // Increased epsilon for stability
    components.confidence_penalty = torch::mse_loss(log_var_flat, target_log_var, at::Reduction::Mean) * confidence_weight_;
    
    // 4. Edge enforcement - reward predictions away from neutral (0.0)
    torch::Tensor distance_from_neutral = torch::abs(pred_flat);
    components.edge_reward = torch::mean(distance_from_neutral) * edge_weight_;
    
    // 5. Total loss: We *subtract* bonuses/rewards.
    components.total = components.prediction_loss + components.confidence_penalty + components.diversity_bonus - components.edge_reward;
    
    // Statistics for monitoring
    components.prediction_std = pred_std.item<double>();
    components.mean_prediction = torch::mean(pred_flat).item<double>();
    components.mean_confidence = torch::mean(1.0 / (1.0 + torch::exp(log_var_flat))).item<double>(); // Proper confidence calc
    components.edge_strength = torch::mean(distance_from_neutral).item<double>();
    
    return components;
}

// ✅ NEW IMPLEMENTATION: The missing dynamic weight adjustment logic.
void QualityEnforcedLoss::adjust_weights_for_epoch(int epoch, const LossComponents& last_components) {
    std::cout << "\n[Epoch " << epoch << " Quality Check & Weight Adjustment]\n";
    
    // If prediction diversity is collapsing, strongly increase pressure to diversify.
    if (last_components.prediction_std < 0.02) {
        diversity_weight_ *= 1.5;
        edge_weight_ *= 1.2;
        std::cout << "  -> Low diversity detected! Increasing diversity_weight to " << diversity_weight_ 
                  << " and edge_weight to " << edge_weight_ << "\n";
    } else {
        // Gradually decay pressure if diversity is healthy
        diversity_weight_ = std::max(0.1, diversity_weight_ * 0.95);
    }

    // If confidence is too low, the model might be struggling. Ease the penalty.
    if (last_components.mean_confidence < 0.3) {
        confidence_weight_ *= 0.9;
        std::cout << "  -> Low mean confidence. Reducing confidence_weight to " << confidence_weight_ << "\n";
    }

    std::cout << "[End of Adjustment]\n";
}

// ✅ NEW IMPLEMENTATION: The missing logging function for better monitoring.
void QualityEnforcedLoss::print_loss_breakdown(const LossComponents& components) const {
    std::cout << std::fixed << std::setprecision(5)
              << "  Total Loss: " << components.total.item<double>()
              << " | Pred Loss: " << components.prediction_loss.item<double>()
              << " | Conf Pen: " << components.confidence_penalty.item<double>()
              << " | Div Bonus: " << components.diversity_bonus.item<double>()
              << " | Edge Reward: " << components.edge_reward.item<double>()
              << " || Stats -> Pred Std: " << components.prediction_std
              << " | Mean Conf: " << components.mean_confidence
              << std::endl;
}

} // namespace sentio::training
