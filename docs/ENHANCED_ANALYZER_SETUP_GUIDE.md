# Enhanced C++ Analyzer Setup and Usage Guide

## Overview

The Enhanced C++ Analyzer v2.0 is a production-ready AST-based code analysis tool that provides accurate, actionable insights for C++ codebases. Unlike regex-based analyzers, it uses Clang's AST parsing for semantic understanding of C++ constructs.

## Installation Requirements

### 1. System Dependencies

#### macOS (Homebrew)
```bash
# Install LLVM/Clang
brew install llvm

# Add to PATH (add to ~/.zshrc or ~/.bash_profile)
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
export LDFLAGS="-L/opt/homebrew/opt/llvm/lib"
export CPPFLAGS="-I/opt/homebrew/opt/llvm/include"
```

#### Ubuntu/Debian
```bash
# Install Clang and development tools
sudo apt-get update
sudo apt-get install clang-14 libclang-14-dev python3-dev

# Alternative: Install latest LLVM
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 14
```

#### CentOS/RHEL
```bash
# Install EPEL and development tools
sudo yum install epel-release
sudo yum groupinstall "Development Tools"
sudo yum install clang llvm-devel python3-devel
```

### 2. Python Dependencies

```bash
# Install Python bindings for libclang
pip install libclang

# Optional: Install additional analysis libraries
pip install networkx matplotlib seaborn  # For advanced graph analysis
```

### 3. Verify Installation

```bash
# Test clang installation
clang --version

# Test Python bindings
python3 -c "import clang.cindex; print('Clang bindings OK')"
```

## Usage Guide

### Basic Usage

```bash
# Analyze current directory
python3 tools/enhanced_cpp_analyzer_v2.py

# Analyze specific directory
python3 tools/enhanced_cpp_analyzer_v2.py /path/to/cpp/project

# Generate JSON report
python3 tools/enhanced_cpp_analyzer_v2.py --format json --output analysis.json

# Include detailed metrics
python3 tools/enhanced_cpp_analyzer_v2.py --metrics
```

### Advanced Usage

#### Using Compilation Database
```bash
# Generate compile_commands.json (if using CMake)
cd build/
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

# Use compilation database for accurate analysis
python3 tools/enhanced_cpp_analyzer_v2.py --compilation-db build/compile_commands.json
```

#### Filtering by Priority
```bash
# Show only high priority issues
python3 tools/enhanced_cpp_analyzer_v2.py --priority high

# Show high and medium priority issues
python3 tools/enhanced_cpp_analyzer_v2.py --priority medium
```

#### Custom Output Formats
```bash
# Generate multiple output formats
python3 tools/enhanced_cpp_analyzer_v2.py \
    --output analysis_report.txt \
    --format text \
    --metrics

# This generates:
# - analysis_report.txt (main report)
# - analysis_report_metrics.json (detailed metrics)
# - analysis_report_data.json (raw analysis data)
```

## Understanding the Output

### Report Structure

```
ENHANCED C++ CODE ANALYSIS REPORT
Generated: 2025-09-22 14:30:45
Directory: /Users/yeogirlyun/C++/sentio_trader
================================================================================

EXECUTIVE SUMMARY
----------------------------------------
Files Analyzed: 45
Total Functions: 387
Total Classes: 23

Key Findings:
  • High Priority Issues: 8
  • Medium Priority Issues: 23
  • Low Priority Issues: 15

🚨 HIGH PRIORITY ISSUES (Immediate Action Required)
--------------------------------------------------
  [HIGH_COMPLEXITY] evaluate_signal
    Location: src/backend/backend_component.cpp:234
    Consider breaking down this function (complexity: 18)

  [GOD_CLASS] BackendComponent
    Location: src/backend/backend_component.cpp:45
    Consider splitting this class (Single Responsibility Principle)
```

### Issue Types and Priorities

#### High Priority (🔴 Fix Immediately)
- **HIGH_COMPLEXITY**: Functions with cyclomatic complexity > 20
- **GOD_CLASS**: Classes with > 30 methods or > 20 member variables
- **SECURITY_UNSAFE_FUNCTION**: Use of unsafe C functions (strcpy, sprintf, etc.)
- **PERFORMANCE_ALLOCATION_IN_LOOP**: Memory allocation inside loops

#### Medium Priority (🟡 Address in Next Sprint)
- **LONG_FUNCTION**: Functions > 100 lines
- **TOO_MANY_PARAMETERS**: Functions with > 5 parameters
- **LARGE_CLASS**: Classes with > 15 methods or > 8 variables
- **PERFORMANCE_EXPENSIVE_COPY**: Unnecessary object copies

#### Low Priority (🟢 Routine Maintenance)
- **SINGLETON_PATTERN**: Singleton anti-pattern detection
- **UNUSED_FUNCTION**: Potentially unused functions (private scope)

### Duplicate Code Analysis

```
🔍 DUPLICATE CODE ANALYSIS
----------------------------------------

Exact Duplicates: 3 groups
  Group 1: 2 identical functions
    - calculate_fees (src/backend/backend_component.cpp:456)
    - calculate_fees (src/backend/portfolio_manager.cpp:123)

Structural Duplicates: 5 groups
  (Same structure, different variable names)
  Group 1: 3 similar functions

Semantic Duplicates: 2 groups
  (Similar functionality, different implementation)
```

### Architectural Analysis

```
🏗️ ARCHITECTURAL VIOLATIONS
----------------------------------------
  Lower layer 'backend' depends on higher layer 'ui'

🔄 CIRCULAR DEPENDENCIES
----------------------------------------
  Cycle: backend_component -> portfolio_manager -> backend_component
```

## Integration with Development Workflow

### 1. Pre-commit Hook

Create `.git/hooks/pre-commit`:
```bash
#!/bin/bash
echo "Running enhanced C++ analysis..."
python3 tools/enhanced_cpp_analyzer_v2.py --priority high --output /tmp/analysis.txt

# Check if high priority issues exist
if grep -q "HIGH PRIORITY: [1-9]" /tmp/analysis.txt; then
    echo "❌ High priority code quality issues found!"
    echo "Run: python3 tools/enhanced_cpp_analyzer_v2.py --priority high"
    exit 1
fi

echo "✅ Code quality check passed"
```

### 2. CI/CD Integration

#### GitHub Actions
```yaml
name: Code Quality Analysis
on: [push, pull_request]

jobs:
  analyze:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Install dependencies
      run: |
        sudo apt-get install clang-14 libclang-14-dev
        pip install libclang
    - name: Run analysis
      run: |
        python3 tools/enhanced_cpp_analyzer_v2.py --format json --output analysis.json
    - name: Upload results
      uses: actions/upload-artifact@v2
      with:
        name: analysis-results
        path: analysis.json
```

### 3. IDE Integration

#### VS Code Settings
Add to `.vscode/tasks.json`:
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "C++ Analysis",
            "type": "shell",
            "command": "python3",
            "args": [
                "tools/enhanced_cpp_analyzer_v2.py",
                "--priority", "high",
                "--output", "analysis_report.txt"
            ],
            "group": "build",
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "panel": "shared"
            }
        }
    ]
}
```

## Troubleshooting

### Common Issues

#### 1. "libclang not found"
```bash
# Check clang installation
which clang
clang --version

# Set library path manually
export CLANG_LIBRARY_PATH="/usr/lib/llvm-14/lib"
```

#### 2. "Parse errors in file"
- Ensure all include paths are specified
- Use compilation database for accurate parsing
- Check for C++17 compatibility

#### 3. "Too many false positives"
- Use compilation database for better accuracy
- Update include paths in the analyzer
- Consider project-specific customizations

### Performance Optimization

#### For Large Codebases (>100k LOC)
```bash
# Use parallel processing (if available)
python3 tools/enhanced_cpp_analyzer_v2.py --jobs 4

# Exclude generated files
python3 tools/enhanced_cpp_analyzer_v2.py --exclude "build/*,*_generated.h"

# Focus on specific modules
python3 tools/enhanced_cpp_analyzer_v2.py src/backend/ src/strategy/
```

## Customization

### Project-Specific Rules

Create `analysis_config.json`:
```json
{
    "complexity_thresholds": {
        "high_priority": 25,
        "medium_priority": 15
    },
    "class_size_limits": {
        "god_class_methods": 35,
        "large_class_methods": 20
    },
    "exclude_patterns": [
        "test_*.cpp",
        "*_mock.h",
        "third_party/*"
    ],
    "custom_rules": {
        "trading_specific": {
            "max_position_params": 3,
            "require_risk_checks": true
        }
    }
}
```

### Extending the Analyzer

```python
# Add custom analysis rules
class TradingSystemAnalyzer(EnhancedCppAnalyzer):
    def analyze_trading_function(self, cursor, file_path: str):
        """Custom analysis for trading functions"""
        func_name = cursor.spelling
        
        # Check for risk management
        if 'execute_trade' in func_name.lower():
            has_risk_check = self.check_for_risk_validation(cursor)
            if not has_risk_check:
                self.issues['high_priority'].append({
                    'type': 'missing_risk_check',
                    'function': func_name,
                    'file': file_path,
                    'line': cursor.location.line,
                    'recommendation': 'Add risk validation before trade execution'
                })
```

## Best Practices

### 1. Regular Analysis Schedule
- Run full analysis weekly
- Run high-priority checks on every commit
- Include in code review process

### 2. Incremental Improvement
- Focus on high-priority issues first
- Set monthly technical debt reduction goals
- Track metrics over time

### 3. Team Adoption
- Provide training on report interpretation
- Establish coding standards based on findings
- Create refactoring guidelines

## Comparison with Other Tools

| Feature | Enhanced Analyzer | SonarQube | PVS-Studio | Clang-Tidy |
|---------|------------------|-----------|------------|------------|
| AST-based Analysis | ✅ | ✅ | ✅ | ✅ |
| Duplicate Detection | ✅ (4 levels) | ✅ (basic) | ❌ | ❌ |
| Refactoring Suggestions | ✅ (with code) | ✅ (basic) | ❌ | ✅ (basic) |
| Architectural Analysis | ✅ | ✅ | ❌ | ❌ |
| Custom Rules | ✅ (Python) | ✅ (Java) | ❌ | ✅ (C++) |
| Cost | Free | $150k+/year | $1k+/seat | Free |
| Setup Complexity | Low | High | Medium | Low |

## Support and Maintenance

### Getting Help
1. Check this documentation first
2. Review the troubleshooting section
3. Examine the source code for customization examples
4. Create issues for bugs or feature requests

### Contributing
1. Follow the existing code style
2. Add tests for new analysis rules
3. Update documentation for new features
4. Submit pull requests with clear descriptions

---

**Next Steps**: Start with basic analysis, gradually integrate into your workflow, and customize based on your project's specific needs. The analyzer will help maintain code quality and reduce technical debt over time.
