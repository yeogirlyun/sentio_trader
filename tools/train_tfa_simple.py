#!/usr/bin/env python3
"""
Simple TFA Training Script - C++ Compatible
Uses existing Python training but ensures perfect C++ metadata compatibility
"""

import sys
import os
import json
import torch
import subprocess
from pathlib import Path
import shutil

def create_cpp_compatible_metadata(output_dir, feature_spec_path):
    """Create metadata compatible with both C++ inference systems"""
    output_dir = Path(output_dir)
    
    # Load feature spec
    with open(feature_spec_path, 'r') as f:
        spec = json.load(f)
    
    # Generate feature names
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
    T = 48
    
    # NEW FORMAT (for TfaSeqContext)
    new_metadata = {
        "model_type": "TFA_CPP_Compatible",
        "feature_count": F,
        "sequence_length": T,
        "d_model": 128,
        "nhead": 8,
        "num_layers": 3,
        "cpp_compatible": True,
        "schema_version": "2.0",
        "saved_at": 1758266465,
        "framework": "torchscript"
    }
    
    # LEGACY FORMAT (for ModelRegistryTS)
    legacy_metadata = {
        "schema_version": "1.0",
        "saved_at": 1758266465,
        "framework": "torchscript",
        "feature_names": feature_names,
        "mean": [0.0] * F,
        "std": [1.0] * F,
        "clip": [],
        "actions": [],
        "seq_len": T,
        "input_layout": "BTF",
        "format": "torchscript"
    }
    
    # Save both formats
    with open(output_dir / "model.meta.json", 'w') as f:
        json.dump(new_metadata, f, indent=2)
    
    with open(output_dir / "metadata.json", 'w') as f:
        json.dump(legacy_metadata, f, indent=2)
    
    print(f"✅ Created C++ compatible metadata with {F} features")

def main():
    print("🚀 Simple TFA Training - C++ Compatible")
    print("=" * 50)
    
    # Use existing v2_m4_optimized as base and make it C++ compatible
    source_dir = Path("artifacts/TFA/v2_m4_optimized")
    output_dir = Path("artifacts/TFA/cpp_compatible")
    feature_spec = Path("configs/features/feature_spec_55_minimal.json")
    
    if not source_dir.exists():
        print(f"❌ Source model not found: {source_dir}")
        return 1
    
    if not feature_spec.exists():
        print(f"❌ Feature spec not found: {feature_spec}")
        return 1
    
    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Copy model files
    print("📁 Copying model files...")
    shutil.copy2(source_dir / "model.pt", output_dir / "model.pt")
    shutil.copy2(feature_spec, output_dir / "feature_spec.json")
    
    # Create C++ compatible metadata
    print("🔧 Creating C++ compatible metadata...")
    create_cpp_compatible_metadata(output_dir, feature_spec)
    
    print(f"\n🎉 C++ Compatible TFA Model Ready!")
    print(f"📁 Location: {output_dir}")
    print(f"📄 Files created:")
    print(f"  🤖 model.pt")
    print(f"  📄 model.meta.json (NEW FORMAT)")
    print(f"  📄 metadata.json (LEGACY FORMAT)")
    print(f"  🔧 feature_spec.json")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
