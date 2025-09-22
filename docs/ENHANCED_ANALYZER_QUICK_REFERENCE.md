# Enhanced C++ Analyzer - Quick Reference Card

## 🚀 Quick Start (30 seconds)

```bash
# 1. Install dependencies
pip install libclang

# 2. Run basic analysis
python enhanced_cpp_analyzer.py

# 3. View results
cat enhanced_cpp_analysis.txt
```

## 📋 Common Commands

| Task | Command |
|------|---------|
| **Basic analysis** | `python enhanced_cpp_analyzer.py` |
| **High priority only** | `python enhanced_cpp_analyzer.py --priority high` |
| **JSON output** | `python enhanced_cpp_analyzer.py --format json` |
| **With metrics** | `python enhanced_cpp_analyzer.py --metrics` |
| **Specific directory** | `python enhanced_cpp_analyzer.py /path/to/code` |
| **Use compile DB** | `python enhanced_cpp_analyzer.py --compilation-db compile_commands.json` |

## 🎯 Priority Levels

| Priority | Icon | When to Fix | Examples |
|----------|------|-------------|----------|
| **High** | 🔴 | Immediately | Security issues, CC>20, God classes |
| **Medium** | 🟡 | Next sprint | CC 11-20, Long functions, Many params |
| **Low** | 🟢 | Maintenance | CC 6-10, Small duplicates, Style |

## 📊 Complexity Guidelines

| Complexity | Risk | Action |
|------------|------|--------|
| **1-5** | ✅ Low | Good |
| **6-10** | 🟡 Medium | Monitor |
| **11-20** | 🟠 High | Refactor soon |
| **>20** | 🔴 Critical | Refactor now |

## 🔍 Issue Types Quick Guide

### Security Issues
- `UNSAFE_FUNCTION`: strcpy, sprintf, gets
- `BUFFER_OVERFLOW`: Unchecked memcpy
- `INPUT_VALIDATION`: Missing bounds checks

### Performance Issues
- `ALLOCATION_IN_LOOP`: new/malloc in loops
- `EXPENSIVE_COPY`: Large objects passed by value
- `REPEATED_CALCULATION`: Expensive calls in loops

### Design Issues
- `GOD_CLASS`: >30 methods or >20 variables
- `HIGH_COMPLEXITY`: Cyclomatic complexity >20
- `CIRCULAR_DEPENDENCY`: Module cycles

## 🛠️ Quick Fixes

### Extract Method (High Complexity)
```cpp
// BEFORE: Complex function
void processOrder(Order& order) {
    // 150 lines of mixed logic
}

// AFTER: Extracted methods
void processOrder(Order& order) {
    validateOrder(order);
    calculatePricing(order);
    executeOrder(order);
}
```

### Parameter Object (Too Many Params)
```cpp
// BEFORE: Many parameters
void createOrder(string symbol, double price, int quantity, 
                string type, bool urgent, double stopLoss);

// AFTER: Parameter object
struct OrderParams {
    string symbol;
    double price;
    int quantity;
    string type;
    bool urgent;
    double stopLoss;
};
void createOrder(const OrderParams& params);
```

### Extract Utility (Duplicates)
```cpp
// BEFORE: Duplicate validation in multiple files
bool validatePrice(double price) { /* same logic */ }

// AFTER: Shared utility
class ValidationUtils {
public:
    static bool validatePrice(double price);
    static bool validateQuantity(int qty);
};
```

## 🔧 Installation Troubleshooting

| Problem | Solution |
|---------|----------|
| **libclang not found** | `export LLVM_LIBRARY_PATH=/usr/lib/llvm-14/lib` |
| **Parse errors** | Use `--compilation-db compile_commands.json` |
| **Memory issues** | Analyze smaller chunks or increase system memory |
| **Slow analysis** | Use compilation database, exclude build dirs |

## 📈 CI/CD Integration Snippets

### Pre-commit Hook
```bash
#!/bin/bash
python enhanced_cpp_analyzer.py --priority high --output /tmp/analysis.txt
if grep -q "High Priority Issues: [1-9]" /tmp/analysis.txt; then
    echo "❌ High priority issues found!"
    exit 1
fi
echo "✅ Code quality check passed"
```

### GitHub Actions (Minimal)
```yaml
- name: C++ Analysis
  run: |
    pip install libclang
    python enhanced_cpp_analyzer.py --priority high
```

## 📋 Report Sections Quick Guide

| Section | What It Shows | Action Items |
|---------|---------------|--------------|
| **Executive Summary** | Overview stats | Get the big picture |
| **High Priority Issues** | Critical problems | Fix immediately |
| **Duplicate Analysis** | Code duplication | Consolidate logic |
| **Unused Functions** | Dead code | Remove after verification |
| **Circular Dependencies** | Architecture issues | Refactor dependencies |
| **Refactoring Recommendations** | Specific suggestions | Follow the guidance |

## 🎯 Typical Workflow

1. **Initial Run**: `python enhanced_cpp_analyzer.py --metrics`
2. **Review Report**: Focus on High Priority section
3. **Fix Critical Issues**: Security, complexity, god classes
4. **Address Duplicates**: Extract common utilities
5. **Clean Up**: Remove unused functions
6. **Re-run**: Verify improvements
7. **Integrate**: Add to CI/CD pipeline

## 💡 Pro Tips

- **Start small**: Fix high priority issues first
- **Use compilation DB**: Much more accurate parsing
- **Track progress**: Save reports to compare over time
- **Automate**: Integrate into your development workflow
- **Customize**: Adjust thresholds for your team's standards

## 🆘 Emergency Fixes

### Critical Security Issue Found
```bash
# 1. Find all unsafe functions immediately
grep -r "strcpy\|sprintf\|gets" src/
# 2. Replace with safe alternatives
# strcpy → strncpy or std::string
# sprintf → snprintf
# gets → fgets
```

### Build Breaking Due to Circular Dependencies
```bash
# 1. Identify the cycle
python enhanced_cpp_analyzer.py --format json | jq '.circular_dependencies'
# 2. Break the cycle by extracting interfaces
# 3. Use forward declarations
# 4. Apply dependency inversion
```

## 📞 Getting Help

1. **Check this quick reference** first
2. **Review the full setup guide** for detailed instructions
3. **Look at error messages** - they're usually informative
4. **Verify prerequisites** are properly installed
5. **Try with compilation database** for parsing issues

---
**Remember**: The analyzer is a tool to help you write better code. Start with high priority issues and gradually improve your codebase quality over time.
