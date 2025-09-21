#!/usr/bin/env python3
"""
C++ Compatible TFA Training Script
Trains TFA models with perfect C++ compatibility by ensuring identical feature generation
"""

import sys
import os
import argparse
import json
import numpy as np
import torch
import torch.nn as nn
from pathlib import Path
import subprocess
import tempfile
import shutil

# Add sentio_trainer to path
sys.path.append('sentio_trainer')
from trainers.tfa_multi_dataset import TFAMultiRegimeTrainer

def ensure_cpp_compatible_metadata(model_dir):
    """Ensure metadata is compatible with both C++ inference systems"""
    model_dir = Path(model_dir)
    
    # Load feature spec to get accurate feature info
    feature_spec_path = model_dir / "feature_spec.json"
    if not feature_spec_path.exists():
        raise FileNotFoundError(f"Feature spec not found: {feature_spec_path}")
    
    with open(feature_spec_path, 'r') as f:
        spec = json.load(f)
    
    # Generate feature names from spec
    feature_names = []
    for f in spec["features"]:
        if "name" in f:
            feature_names.append(f["name"])
        else:
            op = f["op"]
            src = f.get("source", "")
            w = str(f.get("window", ""))
            k = str(f.get("k", ""))
            feature_names.append(f"{op}_{src}_{w}_{k}")
    
    F = len(feature_names)
    T = 48  # Sequence length
    
    # Create NEW FORMAT model.meta.json (for TfaSeqContext)
    new_metadata = {
        "model_type": "TFA_CPP_Compatible",
        "feature_count": F,
        "sequence_length": T,
        "d_model": 128,
        "nhead": 8,
        "num_layers": 3,
        "training_bars": 0,  # Will be updated during training
        "training_time_sec": 0.0,
        "epochs_completed": 0,
        "best_val_loss": float('inf'),
        "regimes_trained": [],
        "cpp_compatible": True,
        "schema_version": "2.0",
        "saved_at": int(torch.tensor(0).item()),  # Will be updated
        "framework": "torchscript"
    }
    
    # Create LEGACY FORMAT metadata.json (for ModelRegistryTS)
    legacy_metadata = {
        "schema_version": "1.0",
        "saved_at": int(torch.tensor(0).item()),
        "framework": "torchscript",
        "feature_names": feature_names,
        "mean": [0.0] * F,    # Model has built-in normalization
        "std": [1.0] * F,     # Model has built-in normalization
        "clip": [],
        "actions": [],
        "seq_len": T,
        "input_layout": "BTF",
        "format": "torchscript"
    }
    
    return new_metadata, legacy_metadata

def train_cpp_compatible_tfa(args):
    """Train TFA model with C++ compatibility"""
    print("🚀 C++ Compatible TFA Training")
    print("=" * 60)
    
    # Ensure output directory exists
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Copy feature spec to output directory
    feature_spec_src = Path(args.feature_spec)
    feature_spec_dst = output_dir / "feature_spec.json"
    shutil.copy2(feature_spec_src, feature_spec_dst)
    
    print(f"📁 Output directory: {output_dir}")
    print(f"🔧 Feature spec: {args.feature_spec}")
    print(f"📊 Data: {args.data}")
    
    # Prepare metadata templates
    new_metadata, legacy_metadata = ensure_cpp_compatible_metadata(output_dir)
    
    # Configure trainer for C++ compatibility
    config = {
        "model": {
            "d_model": 128,
            "nhead": 8,
            "num_layers": 3,
            "sequence_length": 48,
            "dropout": 0.1
        },
        "training": {
            "epochs": args.epochs,
            "batch_size": args.batch_size,
            "learning_rate": args.learning_rate,
            "patience": 10,
            "device": "mps" if torch.backends.mps.is_available() else "cpu"
        },
        "data": {
            "train_split": 0.8,
            "val_split": 0.1,
            "test_split": 0.1
        }
    }
    
    print(f"⚙️  Configuration:")
    print(f"  📈 Epochs: {args.epochs}")
    print(f"  📦 Batch Size: {args.batch_size}")
    print(f"  📊 Learning Rate: {args.learning_rate}")
    print(f"  🖥️  Device: {config['training']['device']}")
    
    # Create trainer
    trainer = TFAMultiRegimeTrainer(config)
    
    # Prepare datasets
    datasets = []
    if args.multi_regime:
        # Multi-regime training
        base_datasets = [
            ("data/equities/QQQ_RTH_NH.csv", "historic_real", 1.0),
        ]
        
        # Add synthetic datasets if available
        for i in range(1, 10):
            synthetic_path = f"data/future_qqq/future_qqq_track_{i:02d}.csv"
            if Path(synthetic_path).exists():
                regime = "normal" if i <= 3 else "volatile" if i <= 6 else "bear"
                weight = 0.8 if regime == "normal" else 1.2 if regime == "volatile" else 1.5
                datasets.append((synthetic_path, f"{regime}_{i}", weight))
        
        datasets = base_datasets + datasets
    else:
        # Single dataset training
        datasets = [(args.data, "single_dataset", 1.0)]
    
    print(f"📊 Training datasets: {len(datasets)}")
    for path, regime, weight in datasets:
        if Path(path).exists():
            print(f"  ✅ {regime}: {path} (weight: {weight})")
        else:
            print(f"  ❌ {regime}: {path} (missing)")
    
    # Start training
    print("\n🚀 Starting training...")
    try:
        # Train the model
        results = trainer.train_multi_regime(
            datasets=datasets,
            feature_spec_path=str(feature_spec_src),
            output_dir=str(output_dir)
        )
        
        # Update metadata with training results
        new_metadata.update({
            "training_bars": results.get("total_bars", 0),
            "training_time_sec": results.get("training_time", 0.0),
            "epochs_completed": results.get("epochs", 0),
            "best_val_loss": results.get("best_val_loss", float('inf')),
            "regimes_trained": [regime for _, regime, _ in datasets],
            "saved_at": int(torch.tensor(0).item())
        })
        
        legacy_metadata.update({
            "saved_at": int(torch.tensor(0).item())
        })
        
        # Save both metadata formats
        with open(output_dir / "model.meta.json", 'w') as f:
            json.dump(new_metadata, f, indent=2)
        
        with open(output_dir / "metadata.json", 'w') as f:
            json.dump(legacy_metadata, f, indent=2)
        
        print("\n🎉 Training completed successfully!")
        print(f"📊 Results:")
        print(f"  🏆 Best validation loss: {results.get('best_val_loss', 'N/A')}")
        print(f"  📈 Epochs completed: {results.get('epochs', 'N/A')}")
        print(f"  ⏱️  Training time: {results.get('training_time', 0.0):.1f}s")
        
        print(f"\n✅ Model files created:")
        print(f"  🤖 TorchScript: {output_dir}/model.pt")
        print(f"  📄 New Metadata: {output_dir}/model.meta.json")
        print(f"  📄 Legacy Metadata: {output_dir}/metadata.json")
        print(f"  🔧 Feature Spec: {output_dir}/feature_spec.json")
        
        return True
        
    except Exception as e:
        print(f"❌ Training failed: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description="C++ Compatible TFA Training")
    parser.add_argument("--data", required=True, help="Path to training data CSV")
    parser.add_argument("--feature-spec", required=True, help="Path to feature specification JSON")
    parser.add_argument("--output", default="artifacts/TFA/cpp_compatible", help="Output directory")
    parser.add_argument("--epochs", type=int, default=100, help="Number of training epochs")
    parser.add_argument("--batch-size", type=int, default=32, help="Batch size")
    parser.add_argument("--learning-rate", type=float, default=0.001, help="Learning rate")
    parser.add_argument("--multi-regime", action="store_true", help="Enable multi-regime training")
    
    args = parser.parse_args()
    
    # Validate inputs
    if not Path(args.data).exists():
        print(f"❌ Data file not found: {args.data}")
        return 1
    
    if not Path(args.feature_spec).exists():
        print(f"❌ Feature spec not found: {args.feature_spec}")
        return 1
    
    # Run training
    success = train_cpp_compatible_tfa(args)
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
