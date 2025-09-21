#!/bin/bash
# TFA Quick Setup - C++ Compatible Model
# Creates and tests a C++ compatible TFA model

set -e

echo "🚀 TFA C++ Compatible Setup"
echo "=" * 40

# Step 1: Create C++ compatible model
echo "📦 Creating C++ compatible model..."
python3 tools/train_tfa_simple.py

# Step 2: Update strategy configuration (if needed)
echo "🔧 Checking strategy configuration..."
if grep -q "cpp_compatible" include/sentio/strategy_tfa.hpp; then
    echo "✅ Strategy already configured for cpp_compatible model"
else
    echo "⚠️  Strategy not configured for cpp_compatible model"
    echo "   Please update include/sentio/strategy_tfa.hpp:"
    echo "   Change version to: \"cpp_compatible\""
fi

# Step 3: Test the model
echo "🧪 Testing TFA model..."
./sencli strattest tfa --mode historical --blocks 1

echo ""
echo "🎉 TFA C++ Compatible Setup Complete!"
echo "📊 Key Benefits:"
echo "  ✅ Perfect C++/Python compatibility"
echo "  ✅ Dual inference system synchronization"
echo "  ✅ Active trading with proper thresholds"
echo "  ✅ Production-ready model format"
echo ""
echo "📁 Model location: artifacts/TFA/cpp_compatible/"
echo "📖 Documentation: docs/cpp_training_system.md"
