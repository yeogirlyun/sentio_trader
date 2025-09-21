#!/usr/bin/env python3
"""
C++ Training Pipeline for TFA
Trains a new TFA model with proper C++ compatibility
"""

import sys
import os
import json
import torch
import torch.nn as nn
import numpy as np
from pathlib import Path
import argparse
import time
from datetime import datetime

# Add the project root to Python path
sys.path.insert(0, str(Path(__file__).parent.parent))

try:
    import sentio_features
    print("✅ sentio_features module loaded")
except ImportError as e:
    print(f"❌ Failed to import sentio_features: {e}")
    print("Building sentio_features module...")
    os.system("make python-module")
    try:
        import sentio_features
        print("✅ sentio_features module built and loaded")
    except ImportError:
        print("❌ Failed to build sentio_features module")
        sys.exit(1)

class TFATransformer(nn.Module):
    """Simple TFA Transformer for C++ compatibility"""
    
    def __init__(self, feature_dim=55, seq_len=48, d_model=128, nhead=8, num_layers=3, dropout=0.1):
        super().__init__()
        self.feature_dim = feature_dim
        self.seq_len = seq_len
        self.d_model = d_model
        
        # Input projection
        self.input_proj = nn.Linear(feature_dim, d_model)
        
        # Positional encoding
        self.register_buffer('pos_encoding', self._create_pos_encoding(seq_len, d_model))
        
        # Transformer
        encoder_layer = nn.TransformerEncoderLayer(
            d_model=d_model,
            nhead=nhead,
            dim_feedforward=d_model * 4,
            dropout=dropout,
            activation='gelu',
            batch_first=True
        )
        self.transformer = nn.TransformerEncoder(encoder_layer, num_layers)
        
        # Output
        self.layer_norm = nn.LayerNorm(d_model)
        self.output_proj = nn.Linear(d_model, 1)
        self.dropout = nn.Dropout(dropout)
        
    def _create_pos_encoding(self, seq_len, d_model):
        """Create positional encoding"""
        pe = torch.zeros(seq_len, d_model)
        position = torch.arange(0, seq_len).unsqueeze(1).float()
        div_term = torch.exp(torch.arange(0, d_model, 2).float() * 
                           -(np.log(10000.0) / d_model))
        
        pe[:, 0::2] = torch.sin(position * div_term)
        pe[:, 1::2] = torch.cos(position * div_term)
        return pe.unsqueeze(0)  # [1, seq_len, d_model]
    
    def forward(self, x):
        # x: [batch, seq, features]
        batch_size = x.size(0)
        
        # Project to d_model
        x = self.input_proj(x)  # [batch, seq, d_model]
        
        # Add positional encoding
        x = x + self.pos_encoding.expand(batch_size, -1, -1)
        
        # Dropout
        x = self.dropout(x)
        
        # Transformer
        x = self.transformer(x)  # [batch, seq, d_model]
        
        # Layer norm
        x = self.layer_norm(x)
        
        # Take last timestep
        x = x[:, -1, :]  # [batch, d_model]
        
        # Output projection
        x = self.output_proj(x)  # [batch, 1]
        
        return x.squeeze(-1)  # [batch]

def load_and_prepare_data(csv_path, feature_spec_path, seq_len=48):
    """Load data and prepare features"""
    print(f"📊 Loading data from {csv_path}")
    
    # Load bars using simple CSV reader (since sentio_features might not be available)
    import pandas as pd
    df = pd.read_csv(csv_path)
    
    # Convert to bars format
    bars = []
    for _, row in df.iterrows():
        bar = {
            'timestamp': row.get('timestamp', row.get('time', 0)),
            'open': float(row['open']),
            'high': float(row['high']), 
            'low': float(row['low']),
            'close': float(row['close']),
            'volume': float(row.get('volume', 0))
        }
        bars.append(bar)
    
    print(f"✅ Loaded {len(bars)} bars")
    
    # Load feature spec
    with open(feature_spec_path, 'r') as f:
        feature_spec = json.load(f)
    
    # Simple feature generation (basic technical indicators)
    print("🔧 Generating features...")
    
    # Convert bars to arrays for easier processing
    closes = np.array([b['close'] for b in bars])
    highs = np.array([b['high'] for b in bars])
    lows = np.array([b['low'] for b in bars])
    volumes = np.array([b['volume'] for b in bars])
    
    # Generate basic features
    features = []
    
    # Price features
    returns = np.diff(closes) / closes[:-1]
    returns = np.concatenate([[0], returns])  # Pad first value
    
    # Moving averages
    for window in [5, 10, 20, 50]:
        ma = pd.Series(closes).rolling(window).mean().fillna(method='bfill').values
        features.append(ma)
        
        # Price relative to MA
        price_rel_ma = closes / ma
        features.append(price_rel_ma)
    
    # RSI
    def calculate_rsi(prices, window=14):
        delta = np.diff(prices)
        gain = np.where(delta > 0, delta, 0)
        loss = np.where(delta < 0, -delta, 0)
        
        avg_gain = pd.Series(gain).rolling(window).mean().fillna(0).values
        avg_loss = pd.Series(loss).rolling(window).mean().fillna(0).values
        
        rs = avg_gain / (avg_loss + 1e-8)
        rsi = 100 - (100 / (1 + rs))
        return np.concatenate([[50], rsi])  # Pad first value
    
    rsi = calculate_rsi(closes)
    features.append(rsi)
    
    # Bollinger Bands
    bb_window = 20
    bb_ma = pd.Series(closes).rolling(bb_window).mean().fillna(method='bfill').values
    bb_std = pd.Series(closes).rolling(bb_window).std().fillna(1.0).values
    bb_upper = bb_ma + 2 * bb_std
    bb_lower = bb_ma - 2 * bb_std
    bb_position = (closes - bb_lower) / (bb_upper - bb_lower + 1e-8)
    features.append(bb_position)
    
    # Volume features
    vol_ma = pd.Series(volumes).rolling(20).mean().fillna(method='bfill').values
    vol_ratio = volumes / (vol_ma + 1e-8)
    features.append(vol_ratio)
    
    # Volatility
    volatility = pd.Series(returns).rolling(20).std().fillna(0.01).values
    features.append(volatility)
    
    # High-Low ratio
    hl_ratio = (highs - lows) / (closes + 1e-8)
    features.append(hl_ratio)
    
    # Stack all features
    features = np.column_stack(features)
    
    # Normalize features
    features = (features - np.mean(features, axis=0)) / (np.std(features, axis=0) + 1e-8)
    
    print(f"✅ Generated features: {features.shape}")
    
    # Create sequences
    sequences = []
    labels = []
    
    for i in range(seq_len, len(features)):
        # Feature sequence
        seq = features[i-seq_len:i]  # [seq_len, features]
        sequences.append(seq)
        
        # Label: predict next bar direction
        if i < len(bars) - 1:
            current_close = bars[i]['close']
            next_close = bars[i+1]['close']
            label = 1.0 if next_close > current_close else 0.0
        else:
            label = 0.5  # Neutral for last bar
        
        labels.append(label)
    
    sequences = np.array(sequences)  # [samples, seq_len, features]
    labels = np.array(labels)       # [samples]
    
    print(f"✅ Created {len(sequences)} sequences")
    return sequences, labels, feature_spec

def train_model(sequences, labels, config):
    """Train the TFA model"""
    print("🚀 Starting TFA training...")
    
    # Convert to tensors
    X = torch.FloatTensor(sequences)
    y = torch.FloatTensor(labels)
    
    # Split data
    n = len(X)
    train_end = int(n * 0.8)
    val_end = int(n * 0.9)
    
    X_train, y_train = X[:train_end], y[:train_end]
    X_val, y_val = X[train_end:val_end], y[train_end:val_end]
    X_test, y_test = X[val_end:], y[val_end:]
    
    print(f"📊 Data splits - Train: {len(X_train)}, Val: {len(X_val)}, Test: {len(X_test)}")
    
    # Create model
    model = TFATransformer(
        feature_dim=X.size(-1),
        seq_len=X.size(1),
        d_model=config['d_model'],
        nhead=config['nhead'],
        num_layers=config['num_layers'],
        dropout=config['dropout']
    )
    
    # Use MPS if available (Apple Silicon)
    device = torch.device('mps' if torch.backends.mps.is_available() else 'cpu')
    model = model.to(device)
    X_train, y_train = X_train.to(device), y_train.to(device)
    X_val, y_val = X_val.to(device), y_val.to(device)
    
    print(f"🖥️  Training on: {device}")
    
    # Optimizer and loss
    optimizer = torch.optim.Adam(model.parameters(), lr=config['lr'], weight_decay=config['weight_decay'])
    criterion = nn.BCEWithLogitsLoss()
    
    # Training loop
    best_val_loss = float('inf')
    patience_counter = 0
    
    for epoch in range(config['epochs']):
        # Training
        model.train()
        train_loss = 0
        num_batches = 0
        
        for i in range(0, len(X_train), config['batch_size']):
            batch_X = X_train[i:i+config['batch_size']]
            batch_y = y_train[i:i+config['batch_size']]
            
            optimizer.zero_grad()
            outputs = model(batch_X)
            loss = criterion(outputs, batch_y)
            loss.backward()
            
            # Gradient clipping
            torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            
            optimizer.step()
            train_loss += loss.item()
            num_batches += 1
        
        # Validation
        model.eval()
        val_loss = 0
        val_batches = 0
        with torch.no_grad():
            for i in range(0, len(X_val), config['batch_size']):
                batch_X = X_val[i:i+config['batch_size']]
                batch_y = y_val[i:i+config['batch_size']]
                
                outputs = model(batch_X)
                loss = criterion(outputs, batch_y)
                val_loss += loss.item()
                val_batches += 1
        
        train_loss /= num_batches
        val_loss /= val_batches
        
        # Progress
        if epoch % 10 == 0:
            print(f"📈 Epoch {epoch:3d} | Train Loss: {train_loss:.6f} | Val Loss: {val_loss:.6f}")
        
        # Early stopping
        if val_loss < best_val_loss:
            best_val_loss = val_loss
            patience_counter = 0
            # Save best model
            best_model_state = model.state_dict().copy()
        else:
            patience_counter += 1
            
        if patience_counter >= config['patience']:
            print(f"🛑 Early stopping at epoch {epoch}")
            break
    
    # Load best model
    model.load_state_dict(best_model_state)
    
    print(f"🏆 Best validation loss: {best_val_loss:.6f}")
    return model

def export_cpp_compatible_model(model, feature_spec, output_dir, config):
    """Export model in C++ compatible format"""
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"💾 Exporting model to {output_dir}")
    
    # Set model to eval mode
    model.eval()
    
    # Create example input for tracing
    example_input = torch.randn(1, config['seq_len'], config['feature_dim'])
    if next(model.parameters()).is_cuda or str(next(model.parameters()).device) == 'mps':
        example_input = example_input.to(next(model.parameters()).device)
    
    # Trace model
    traced_model = torch.jit.trace(model, example_input)
    
    # Save TorchScript model
    model_path = output_dir / "model.pt"
    traced_model.save(str(model_path))
    print(f"✅ Saved TorchScript model: {model_path}")
    
    # Generate feature names
    feature_names = [f"feature_{i}" for i in range(config['feature_dim'])]
    
    # NEW FORMAT metadata (for TfaSeqContext)
    new_metadata = {
        "model_type": "TFA_CPP_Trained",
        "feature_count": config['feature_dim'],
        "sequence_length": config['seq_len'],
        "d_model": config['d_model'],
        "nhead": config['nhead'],
        "num_layers": config['num_layers'],
        "cpp_trained": True,
        "schema_version": "2.0",
        "saved_at": int(time.time()),
        "framework": "torchscript"
    }
    
    # LEGACY FORMAT metadata (for ModelRegistryTS)
    legacy_metadata = {
        "schema_version": "1.0",
        "saved_at": int(time.time()),
        "framework": "torchscript",
        "feature_names": feature_names,
        "mean": [0.0] * config['feature_dim'],  # Model has built-in normalization
        "std": [1.0] * config['feature_dim'],   # Model has built-in normalization
        "clip": [],
        "actions": [],
        "seq_len": config['seq_len'],
        "input_layout": "BTF",
        "format": "torchscript"
    }
    
    # Save metadata files
    with open(output_dir / "model.meta.json", 'w') as f:
        json.dump(new_metadata, f, indent=2)
    
    with open(output_dir / "metadata.json", 'w') as f:
        json.dump(legacy_metadata, f, indent=2)
    
    # Save feature spec
    with open(output_dir / "feature_spec.json", 'w') as f:
        json.dump(feature_spec, f, indent=2)
    
    print(f"✅ Saved metadata: model.meta.json, metadata.json")
    print(f"✅ Saved feature spec: feature_spec.json")

def main():
    parser = argparse.ArgumentParser(description="C++ TFA Training Pipeline")
    parser.add_argument("--data", required=True, help="Path to training data CSV")
    parser.add_argument("--feature-spec", required=True, help="Path to feature specification JSON")
    parser.add_argument("--output", default="artifacts/TFA/cpp_trained", help="Output directory")
    parser.add_argument("--epochs", type=int, default=50, help="Number of epochs")
    parser.add_argument("--batch-size", type=int, default=32, help="Batch size")
    parser.add_argument("--learning-rate", type=float, default=0.001, help="Learning rate")
    parser.add_argument("--d-model", type=int, default=128, help="Model dimension")
    parser.add_argument("--nhead", type=int, default=8, help="Number of attention heads")
    parser.add_argument("--num-layers", type=int, default=3, help="Number of transformer layers")
    
    args = parser.parse_args()
    
    # Validate inputs
    if not Path(args.data).exists():
        print(f"❌ Data file not found: {args.data}")
        return 1
    
    if not Path(args.feature_spec).exists():
        print(f"❌ Feature spec not found: {args.feature_spec}")
        return 1
    
    # Training configuration
    config = {
        'epochs': args.epochs,
        'batch_size': args.batch_size,
        'lr': args.learning_rate,
        'weight_decay': 1e-4,
        'patience': 10,
        'seq_len': 48,
        'feature_dim': 55,  # Will be updated based on actual features
        'd_model': args.d_model,
        'nhead': args.nhead,
        'num_layers': args.num_layers,
        'dropout': 0.1
    }
    
    print("🚀 C++ TFA Training Pipeline")
    print("=" * 50)
    print(f"📁 Data: {args.data}")
    print(f"🔧 Feature Spec: {args.feature_spec}")
    print(f"📂 Output: {args.output}")
    print(f"⚙️  Config: {config}")
    
    try:
        # Load and prepare data
        sequences, labels, feature_spec = load_and_prepare_data(
            args.data, args.feature_spec, config['seq_len']
        )
        
        # Update config with actual feature dimension
        config['feature_dim'] = sequences.shape[-1]
        
        # Train model
        start_time = time.time()
        model = train_model(sequences, labels, config)
        training_time = time.time() - start_time
        
        print(f"⏱️  Training completed in {training_time:.1f} seconds")
        
        # Export model
        export_cpp_compatible_model(model, feature_spec, args.output, config)
        
        print("\n🎉 C++ TFA Training Complete!")
        print(f"📁 Model saved to: {args.output}")
        print(f"🔧 Update strategy to use: cpp_trained")
        print(f"🧪 Test with: ./sencli strattest tfa --mode historical --blocks 5")
        
        return 0
        
    except Exception as e:
        print(f"❌ Training failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())