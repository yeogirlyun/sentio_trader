# Sentio Trader Project Rules

**Established:** 2025-01-27  
**Status:** Mandatory for all AI models and contributors

---

## 🎯 **CRITICAL TESTING RULE: Real Market Data Only**

### **Mandatory Testing Standard**
**ALL testing must use real market data unless explicitly instructed otherwise.**

- **Default Data**: `data/polygon_QQQ_1m.feather` (real QQQ 1-minute bars)
- **Rationale**: Synthetic data provides misleading results and doesn't reflect real market patterns
- **Exception**: Only use synthetic data when explicitly requested for specific testing scenarios
- **Impact**: Ensures all performance measurements reflect actual trading conditions and market microstructure

**This rule supersedes all other testing preferences and must be followed for all PPO training, feature testing, and performance validation.**

---

## 🚫 **CRITICAL CODE MANAGEMENT RULE: No Duplicate Source Modules**

### **Mandatory Code Standards**
**NO duplicate source modules, files, or code blocks are allowed in the project.**

- **Direct Edits Only**: Make improvements by directly editing existing files
- **No Duplicate Files**: Never create `file_v2.py`, `file_new.cpp`, or similar duplicates
- **Git Version Control**: Use git for versioning, not different file names
- **Single Source of Truth**: Maintain one canonical version of each component
- **No Code Duplication**: Consolidate functionality into existing modules

**Examples of PROHIBITED practices:**
- ❌ `align_bars_4.py` (should edit `align_bars.py` directly)
- ❌ `strategy_ire_v2.cpp` (should edit `strategy_ire.cpp` directly)
- ❌ Copy-paste code blocks between files

**This rule ensures clean architecture, maintainability, and prevents code divergence.**

---



#### ❌ **FORBIDDEN Actions**
- **CREATE** new testing modules, scripts, or frameworks
- **DUPLICATE** testing functionality that exists in unified tester
- **BYPASS** the canonical testing interface
- **IGNORE** the unified tester capabilities assessment

#### ⚠️ **CONDITIONAL Actions (Require Permission)**

**IF** the unified tester lacks required functionality:

1. **STOP** and **ASK PERMISSION** before creating any new tester
2. **EXPLAIN** exactly what functionality is missing
3. **PROPOSE** adding the feature to the canonical unified tester
4. **WAIT** for explicit authorization before proceeding

**Example Permission Request:**
```
The unified tester doesn't support [SPECIFIC_FEATURE]. 
I need to [SPECIFIC_REQUIREMENT] for [USER_TASK].
Should I:
A) Add this feature to sentio_unified/?
B) Create a temporary wrapper around UnifiedTester?
C) Use an alternative approach?
```

#### 🔄 **Temporary Tester Protocol (If Authorized)**

**IF** explicitly authorized to create a temporary tester:

1. **CREATE** in `/temp/` directory with clear naming: `temp_[purpose]_[date].py`
2. **DOCUMENT** why unified tester was insufficient
3. **IMPLEMENT** as a wrapper around `UnifiedTester` when possible
4. **USE** for the specific task only
5. **DELETE** immediately after task completion
6. **PROPOSE** integration into unified tester for future use

**Example Temporary Tester:**
```python
# temp/temp_monte_carlo_wrapper_20250902.py
from sentio_unified import UnifiedTester, TesterConfig

def monte_carlo_test(strategy, data, scenarios=1000):
    """Temporary Monte Carlo wrapper - DELETE after use"""
    results = []
    for i in range(scenarios):
        # Resample data, run UnifiedTester, collect results
        tester = UnifiedTester(f"mc_{i}", strategy, {}, TesterConfig())
        result = tester.run_backtest(resampled_data)
        results.append(result)
    return analyze_results(results)
```

### **🏗️ Unified Tester Capabilities Reference**

#### **Core Testing Functions**
- ✅ **Individual Strategy Testing**: Single strategy validation with comprehensive metrics
- ✅ **Batch Strategy Testing**: Test multiple strategies simultaneously  
- ✅ **Performance Analysis**: Monthly return, Sharpe ratio, drawdown, trade statistics
- ✅ **Audit Trail Integration**: Full signal persistence with deterministic replay
- ✅ **Multiple Execution Modes**: close, next_open, next_close
- ✅ **Cost Modeling**: Realistic fees and slippage simulation
- ✅ **Portfolio Management**: Cash, position, leverage validation
- ✅ **Multi-Asset Support**: QQQ, BTC, TSLA, and custom symbols
- ✅ **Data Format Support**: CSV, Feather, with timezone handling
- ✅ **Validation & Verification**: Trade legality, cash checks, leverage limits

#### **Advanced Features**
- ✅ **Strategy Registration**: Plugin system for new strategies
- ✅ **Configuration Management**: JSON-based strategy parameters
- ✅ **CLI Interface**: Complete command-line testing suite
- ✅ **Programmatic API**: Python interface for custom workflows
- ✅ **Output Formats**: JSON, Markdown, CSV reporting
- ✅ **Database Integration**: PostgreSQL audit trail with metrics

#### **Extensibility Points**
- ✅ **Strategy Interface**: Clean `on_bar()` method for any algorithm
- ✅ **Execution Models**: Pluggable execution timing and logic
- ✅ **Cost Models**: Configurable fee and slippage calculations
- ✅ **Validation Rules**: Extensible portfolio and trade validation
- ✅ **Metrics Calculation**: Expandable performance metrics suite

### **🎯 Common Use Cases → Unified Tester Solutions**

| **Use Case** | **Old Approach** | **New Approach** |
|--------------|------------------|------------------|
| **Strategy Performance** | `comprehensive_strategy_performance_test.py` | `sentio_unified.cli backtest` |
| **Walk-Forward Testing** | `walk_forward_testing_v1.py` | Wrap `UnifiedTester` in time loop |
| **Monte Carlo Analysis** | `monte_carlo_tester.py` | Wrap `UnifiedTester` in scenario loop |
| **System Health Check** | Multiple health testers | `sanity_check.py --test-all-strategies` |
| **Strategy Validation** | Various compatibility testers | `sentio_unified.cli list-available-strategies` |
| **Benchmark Comparison** | `benchmark_tester.py` | Run multiple strategies with `UnifiedTester` |
| **Parameter Optimization** | Custom optimization scripts | Wrap `UnifiedTester` in Optuna loop |

### **🚨 Violation Consequences**

**Creating unauthorized testers results in:**
- ❌ **Code Review Rejection** - Pull requests will be rejected
- ❌ **Technical Debt** - Increases maintenance burden
- ❌ **Inconsistency** - Breaks unified testing architecture
- ❌ **Audit Trail Loss** - Missing signal persistence and validation
- ❌ **Performance Issues** - Unoptimized, duplicate implementations

### **✅ Compliance Checklist**

Before any testing work, AI models must confirm:

- [ ] **Reviewed** unified tester documentation and capabilities
- [ ] **Verified** unified tester supports required functionality
- [ ] **Attempted** to solve the task using existing unified tester features
- [ ] **Requested permission** if additional functionality is needed
- [ ] **Used** canonical testing interface for all testing operations
- [ ] **Avoided** creating duplicate or competing testing frameworks

### **📚 Required Reading**

All AI models must be familiar with:
1. **`sentio_unified/README.md`** - Usage guide and examples
2. **`sentio_unified/cli.py`** - Complete CLI reference
3. **`req_requests/UNIFIED_TESTER_REQUIREMENTS_ANALYSIS.md`** - Architecture specification
4. **`tools/sanity_check.py`** - System health testing capabilities

### **🎯 Success Metrics**

- **Zero** unauthorized testing modules created
- **100%** testing tasks solved with unified tester
- **Consistent** testing interfaces across all use cases
- **Complete** audit trail for all strategy testing
- **Maintainable** testing architecture with no duplication

---

## 📋 Documentation Policy

### **CRITICAL RULE: Two-Folder Documentation System**

Sentio Trader maintains **EXACTLY TWO** documentation locations:

1. **`docs/` folder** - All regular documentation including architecture, usage guides, requirements, and bug reports
2. **`megadocs/` folder** - Comprehensive mega-documents generated by `tools/create_mega_document.py`

**Key Documents:**
- **`docs/architecture.md`** - Complete system architecture and technical details
- **`docs/readme.md`** - Installation, usage, and user guide

### **Mandatory Documentation Rules**

#### ✅ **ALLOWED Actions**
- **UPDATE** existing content in `docs/ARCHITECTURE.md`
- **UPDATE** existing content in `docs/README.md`
- **REPLACE** outdated information with current codebase reflection
- **ENHANCE** existing sections with new features

#### ❌ **FORBIDDEN Actions**
- **CREATE** new `.md` files anywhere in the project
- **CREATE** additional README files in subdirectories
- **CREATE** separate architecture documents
- **CREATE** feature-specific documentation files
- **LEAVE** outdated information in documentation

### **CRITICAL RULE: No Unsolicited Document Creation**

**AI models must NEVER create any new documents unless explicitly instructed by the user.**

#### ✅ **ALLOWED Document Creation**
- **Source code files** (.py, .js, .ts, etc.) - Required to implement user instructions
- **Configuration files** (.json, .yaml, .toml) - Required for functionality
- **Test files** - When explicitly requested for testing
- **Temporary working files** - Must be cleaned up after completion

#### ❌ **FORBIDDEN Document Creation**
- **Documentation files** (.md, .rst, .txt) - Unless explicitly requested
- **Summary documents** - Never create implementation summaries or completion reports
- **Report files** - Unless specifically instructed to create reports
- **Analysis documents** - Unless explicitly requested
- **README files** - Never create additional README files

### **Bug Reports and Requirement Documents Policy**

#### **Temporary Documents (bug_reports/, req_requests/)**
- **Creation**: Only when explicitly instructed by user
- **Updates**: Only when explicitly instructed by user
- **Status**: Temporary files, not committed to git, periodically removed
- **Purpose**: Specific analysis tasks when requested

#### **Default Behavior**
- **DO NOT** create bug reports unless specifically asked
- **DO NOT** update existing bug reports unless specifically asked
- **DO NOT** create requirement documents unless specifically asked
- **DO NOT** create any summary or analysis documents unless specifically asked

### **Documentation Update Requirements**

When making code changes, AI models **MUST**:

1. **Update Architecture**: Reflect changes in `docs/ARCHITECTURE.md`
2. **Update Usage**: Reflect changes in `docs/README.md`
3. **Remove Outdated**: Delete obsolete information
4. **Keep Current**: Ensure documentation matches current codebase
5. **No New Files**: Never create additional documentation files

---

## 🏗️ Architecture Rules

### **CRITICAL ARCHITECTURAL CONTRACT (Mandatory)**

**ALL backend systems (runner, router, sizer) MUST be strategy-agnostic and follow these principles:**

#### **🔒 RULE 1: Strategy-Agnostic Backend**
- **Runner**: Must work with ANY strategy implementing BaseStrategy interface
- **Router**: Must work with ANY RouterCfg from ANY strategy  
- **Sizer**: Must work with ANY strategy configuration
- **NO** strategy names, types, or strategy-specific logic in core systems
- **ALL** strategy behavior controlled via BaseStrategy virtual methods

#### **🔒 RULE 2: BaseStrategy API Completeness**
- **ALL** strategy behavior expressed through BaseStrategy virtual methods
- **Extension Pattern**: Add virtual methods to BaseStrategy, NOT core system modifications
- **Feature Flags**: Use boolean flags like `requires_dynamic_allocation()` for optional behaviors
- **Configuration Objects**: Strategy preferences via config structs (RouterCfg, etc.)

#### **🔒 RULE 3: Profit Maximization Mandate**
- **100% Capital Deployment**: Always use full available capital
- **Maximum Leverage**: Use leveraged instruments (TQQQ, SQQQ, PSQ) for strong signals
- **No Artificial Limits**: Remove any constraints that limit capital deployment or leverage
- **Position Integrity**: Never allow negative positions or conflicting long/short positions

#### **🔒 RULE 4: Architectural Enforcement**
```cpp
// ✅ CORRECT: Strategy controls behavior via virtual methods
if (strategy->requires_dynamic_allocation()) {
    // Use dynamic allocation path
} else {
    // Use legacy router path
}

// ❌ WRONG: Strategy-specific logic in runner
if (strategy->get_name() == "tfa") {
    // TFA-specific logic - FORBIDDEN
}
```

#### **🔒 RULE 5: Extension Protocol**
**BEFORE** modifying runner/router/sizer, ask:
1. Can this be achieved by extending BaseStrategy API?
2. Is this change strategy-agnostic?
3. Does this maintain backward compatibility?
4. Will this work for all current and future strategies?

**If ANY answer is "No", extend BaseStrategy instead.**

#### **🔒 RULE 6: Code Review Checklist**
- [ ] **No strategy names** in runner/router/sizer code
- [ ] **All behavior** controlled via BaseStrategy virtual methods
- [ ] **Backward compatible** with existing strategies
- [ ] **100% capital deployment** maintained
- [ ] **Maximum leverage** for strong signals
- [ ] **Position integrity** preserved (no negative/conflicting positions)

### **System Architecture Principles**

1. **Multi-Algorithm Engine**: All trading logic goes through the multi-algorithm system
2. **Kafka Messaging**: All inter-service communication uses Kafka
3. **Event-Driven**: Asynchronous, non-blocking operations
4. **GUI Integration**: All features must have GUI controls
5. **Performance Tracking**: All algorithms must have performance monitoring
6. **Strategy-Agnostic Core**: Backend systems work with any BaseStrategy implementation

### **Code Organization**

```
Sentio/
├── docs/                    # ONLY these two files allowed
│   ├── ARCHITECTURE.md      # Technical architecture
│   └── README.md           # User guide and installation
├── services/               # Core trading services
├── ui/                     # GUI components
├── models/                 # AI/ML models
├── core/                   # System controllers
├── data/                   # Data management
├── training/               # Algorithm training
├── backtesting/           # Performance testing
└── analysis/              # Results storage (no docs)
```

### **Component Integration Rules**

1. **New Algorithms**: Must integrate with `multi_algorithm_signal_processor.py`
2. **GUI Features**: Must integrate with existing tab system
3. **Performance Tracking**: Must provide real-time metrics
4. **Configuration**: Must use existing config system
5. **Logging**: Must use centralized logging system (see Logging Policy below)

---

## 📁 Project Directory Structure

### **Consolidated Repository Structure**

Sentio Trader uses a **single root repository** with specialized trainer subdirectories:

```
Sentio/                          # Main project root
├── main.py                      # Main application entry point
├── algorithms/                  # Trading strategies
├── core/                       # Core system components
├── ui/                         # User interface
├── services/                   # Backend services
├── ppo_trainer/                # PPO reinforcement learning trainer
│   ├── train_ppo.py            # Enhanced PPO training script
│   ├── data/                   # Training data
│   ├── models/                 # Trained models
│   └── logs/                   # Training logs
└── [future_trainer]/           # Future ML/NN trainers (abc_trainer, xyz_trainer)
```

### **Trainer Integration Rules**

#### ✅ **ALLOWED Actions**
- **CONSOLIDATE** all ML trainers under main Sentio directory
- **CREATE** new trainer subdirectories (e.g., `abc_trainer`, `xyz_trainer`)
- **MAINTAIN** separate data and model directories within each trainer
- **ENSURE** both `main.py` and trainer scripts compile independently

#### ❌ **FORBIDDEN Actions**
- **CREATE** separate repositories for trainers
- **SCATTER** trainer files across multiple root directories
- **BREAK** compilation compatibility between main Sentio and trainers
- **DUPLICATE** common functionality across trainers

---

## 🤖 AI Model Guidelines

### **When Working on Sentio Trader**

#### **Documentation Policy (MANDATORY)**
```python
# After any code changes, ALWAYS update:
1. docs/architecture.md - Technical changes
2. docs/readme.md - User-facing changes

# ALLOWED documentation locations:
- docs/ folder - Regular documentation (requirements, usage guides, bug reports)
- megadocs/ folder - Mega documents generated by create_mega_document.py

# NEVER create (unless explicitly instructed):
- Documentation outside docs/ or megadocs/ folders
- Additional README files in subdirectories
- Scattered documentation files
- Summary or completion reports
- Any documentation files whatsoever

# ONLY create when explicitly requested:
- Bug reports (using create_mega_document.py)
- Requirement documents (using create_mega_document.py)
- Analysis documents (using create_mega_document.py)
- Any other documentation
```

#### **Default Response Behavior**
```python
# When completing tasks:
✅ DO: Provide brief summary in chat
✅ DO: Create/modify source code as needed
✅ DO: Update the two permanent docs if needed

❌ DON'T: Create implementation summaries
❌ DON'T: Create completion reports
❌ DON'T: Create any .md files
❌ DON'T: Update existing bug reports
❌ DON'T: Create analysis documents
```

#### **Command Execution Policy (Explicit Instruction Required)**
```python
# When the USER explicitly instructs to run programs or shell commands:
✅ DO: Execute the command immediately within the chat session's shell
✅ DO: Use non-interactive flags (e.g., --yes) and avoid prompts
✅ DO: Pipe paged output to cat to prevent blocking (e.g., ... | cat)
✅ DO: Run long-lived processes in background when appropriate
✅ DO: Capture and report stdout/stderr and exit codes in chat
✅ DO: Prefer project root absolute paths

# Safety & scope
✅ DO: Limit execution to the project workspace and intended tools
❌ DON'T: Execute destructive/system-wide commands unless explicitly stated
❌ DON'T: Require interactive input mid-run; fail fast and report instead

# This policy supersedes prior guidance about not running commands in chat
# when explicit user instruction to execute is given.
```

#### **Code Changes**
```python
# Follow these patterns:
1. Integrate with existing multi-algorithm system
2. Add GUI controls for new features
3. Implement performance tracking
4. Use existing Kafka messaging
5. Follow established error handling
```

#### **Testing Requirements**
```python
# Before completing work:
1. Test GUI integration
2. Verify algorithm performance tracking
3. Confirm Kafka message flow
4. Validate configuration system
5. Update both documentation files
```

---

## 🚫 Code Duplication Prevention Rules

### **CRITICAL PRINCIPLE: No Duplicate Modules**

**Code duplication is pure evil and must be avoided at all costs.**

#### **File Naming Rules (MANDATORY)**

##### ✅ **ALLOWED Naming Patterns**
```python
# Descriptive, specific names that indicate exact purpose:
ppo_trainer.py          # PPO training system
market_data_producer.py # Market data production
risk_manager.py         # Risk management
signal_processor.py     # Signal processing
chart_widget.py         # Chart display widget
```

##### ❌ **FORBIDDEN Naming Patterns**
```python
# Vague adjectives that create confusion:
advanced_*.py          # What makes it "advanced"?
enhanced_*.py          # Enhanced compared to what?
optimized_*.py         # All code should be optimized
improved_*.py          # Improved from what version?
fixed_*.py             # Fixes should overwrite, not duplicate
v2_*.py, v3_*.py       # Version numbers in filenames
final_*.py             # Nothing is ever truly final
new_*.py               # Everything was new once
better_*.py            # Subjective and meaningless
```

#### **Module Evolution Rules**

##### **Rule 1: Overwrite, Don't Duplicate**
```python
# WRONG: Creating new versions
ppo_trainer.py          # Original
advanced_ppo_trainer.py # ❌ FORBIDDEN - creates confusion
enhanced_ppo_trainer.py # ❌ FORBIDDEN - which one to use?

# RIGHT: Evolve in place
ppo_trainer.py          # ✅ Single source of truth
# When improving: edit ppo_trainer.py directly
```

##### **Rule 2: Specific Names for Different Behavior**
```python
# WRONG: Vague adjectives
signal_processor.py         # Original
advanced_signal_processor.py # ❌ What's "advanced"?

# RIGHT: Specific characteristics
signal_processor.py         # General signal processing
momentum_signal_processor.py # ✅ Momentum-based signals
ml_signal_processor.py      # ✅ Machine learning signals
```

##### **Rule 3: Temporary Files Must Be Cleaned**
```python
# During development, temporary files are acceptable:
debug_*.py              # For debugging only
test_*.py               # For testing only
temp_*.py               # For temporary work

# But MUST be removed before completion:
rm debug_*.py           # Clean up when done
rm test_*.py            # Remove temporary tests
rm temp_*.py            # Delete temporary files
```

#### **Implementation Guidelines**

##### **When Improving Existing Code:**
1. **Edit the original file directly**
2. **Do NOT create new versions with adjectives**
3. **Use git for version history, not filenames**
4. **Test thoroughly before overwriting**
5. **Update imports if class names change**

##### **When Adding New Functionality:**
1. **Ask: Is this truly different behavior?**
2. **If same purpose: enhance existing file**
3. **If different purpose: use specific descriptive name**
4. **Never use vague adjectives like "advanced" or "enhanced"**

##### **Examples of Correct Evolution:**

```python
# Scenario: Improving PPO trainer
# WRONG:
ppo_trainer.py              # Original
advanced_ppo_trainer.py     # ❌ Creates confusion

# RIGHT:
ppo_trainer.py              # ✅ Evolved in place

# Scenario: Adding different signal processing
# WRONG:
signal_processor.py         # Original  
enhanced_signal_processor.py # ❌ Vague adjective

# RIGHT:
signal_processor.py         # Base processor
momentum_signal_processor.py # ✅ Specific: momentum-based
mean_reversion_processor.py  # ✅ Specific: mean reversion
```

#### **Enforcement Rules**

##### **AI Models MUST:**
1. **Check for existing similar files before creating new ones**
2. **Use specific, descriptive names that indicate exact purpose**
3. **Never use vague adjectives (advanced, enhanced, optimized, etc.)**
4. **Overwrite existing files when improving functionality**
5. **Remove temporary/debug files after completion**
6. **Update all imports when renaming files**

##### **Automatic Violations:**
Any file with these patterns will be **automatically rejected**:
- `*advanced*`
- `*enhanced*`
- `*optimized*`
- `*improved*`
- `*fixed*`
- `*v2*`, `*v3*`, etc.
- `*final*`
- `*new*`
- `*better*`

#### **Code Review Checklist**

Before completing any work, verify:
- [ ] No duplicate modules with similar functionality
- [ ] No vague adjectives in filenames
- [ ] All temporary/debug files removed
- [ ] Imports updated for any renamed files
- [ ] Single source of truth for each functionality
- [ ] File names clearly indicate specific purpose
- [ ] **Run duplicate detection scan: `python3 tools/dupdef_scan.py --fail-on-issues`**

#### **Automated Duplicate Detection**

**MANDATORY:** All code changes must pass the duplicate definition scanner:

```bash
# Run before committing any code:
python3 tools/dupdef_scan.py --fail-on-issues

# For detailed report:
python3 tools/dupdef_scan.py --out duplicate_report.txt

# For JSON output:
python3 tools/dupdef_scan.py --json --out duplicate_report.json
```

**The scanner detects:**
- Duplicate class names across files
- Duplicate method names within classes
- Duplicate functions within modules
- Overload groups without implementations
- Syntax errors

**Zero tolerance policy:** Any duplicates found must be resolved before code completion.

#### **Real-World Example: PPO Cleanup (August 2025)**

**Problem:** PPO codebase had accumulated 20+ duplicate files:
```
❌ BEFORE (Confusing mess):
advanced_ppo_trainer.py
enhanced_ppo_network.py
sentio_ppo_integration.py
train_ppo_fixed.py
train_ppo_fixed_final.py
train_ppo_10_percent_monthly.py
train_ppo_optimized.py
apply_ppo_fixes_immediately.py
debug_ppo_rewards.py
test_ppo_fixes.py
... and 10+ more duplicates
```

**Solution:** Applied these rules rigorously:
```
✅ AFTER (Clean, clear):
models/ppo_trainer.py      # Single PPO training system
models/ppo_network.py      # Single neural network
models/ppo_integration.py  # Single integration module
models/ppo_trading_agent.py # Base agent system
train_ppo.py               # Single training script
```

**Result:** 
- 20+ files reduced to 5 essential files
- Zero confusion about which file to use
- All functionality preserved and improved
- Clean, maintainable codebase

**This is the standard all future development must follow.**

---

## 📁 File Management Rules

### **Documentation Cleanup**

The following files have been **REMOVED** and should **NEVER** be recreated:

#### **Removed Files**
- `INTEGRATION_COMPLETE.md`
- `ALGORITHM_COMPARISON_GUIDE.md`
- `README_ENHANCED.md`
- All files in `analysis/reports/*.md`
- All files in `docs/financeai/*.md`
- `ui/README_*.md`
- `req_requests/*.md`
- `tools/*.md`
- `entity/README.md`
- `trader-bot/README.md`
- `trader-bot/overview.md`

#### **Cleanup Commands**
```bash
# These files are removed and should not be recreated
rm -f INTEGRATION_COMPLETE.md
rm -f ALGORITHM_COMPARISON_GUIDE.md
rm -f README_ENHANCED.md
rm -rf analysis/reports/*.md
rm -rf docs/financeai/
rm -f ui/README_*.md
rm -f req_requests/*.md
rm -f tools/GUI_AND_MODEL_REQUIREMENTS.md
rm -f tools/FINANCEAI_MEGA_DOC.md
rm -f entity/README.md
rm -f trader-bot/README.md
rm -f trader-bot/overview.md
```

### **Allowed Non-Documentation Files**

These files are **PERMITTED** and serve specific functions:
- `PROJECT_RULES.md` (this file - project governance)
- `requirements*.txt` (dependency management)
- `config/*.yaml` (configuration files)
- `config/*.json` (configuration files)
- `.env.example` (environment template)

### **Git Repository Exclusions**

The following directories and files are **TEMPORARY** and must **NEVER** be committed to git:

#### **Excluded Directories**
- `bug_reports/` - Temporary bug documentation (periodically removed)
- `req_requests/` - Temporary requirement documents (periodically removed)
- `analysis/reports/` - Temporary analysis outputs
- `logs/` - Runtime logs and temporary data

#### **Excluded File Patterns**
- `*_REPORT.md` - Temporary reports
- `*_REQUIREMENTS.md` - Temporary requirements
- `*_BUG_*.md` - Bug report documents
- `*_MEGA_*.md` - Mega document outputs
- `debug_*.py` - Debug scripts (remove after use)
- `temp_*.py` - Temporary scripts (remove after use)
- `test_*.py` - Temporary test scripts (remove after use)

#### **Git Commit Rules**

**AI models MUST:**
1. **Never commit files in `bug_reports/` or `req_requests/` directories**
2. **Never commit temporary documentation files**
3. **Remove temporary files before committing**
4. **Only commit permanent code and the two allowed documentation files**

**Example of correct git workflow:**
```bash
# WRONG - includes temporary docs
git add bug_reports/ req_requests/ *.md

# RIGHT - only permanent code
git add algorithms/ models/ ui/ services/
git add docs/ARCHITECTURE.md docs/README.md  # Only these docs allowed
```

---

## 📄 Mega Document Generation Rules

**CRITICAL**: AI models must **NEVER** create mega documents unless explicitly instructed by the user.

### **When to Create Mega Documents**

**ONLY create mega documents when:**
- User explicitly requests a bug report
- User explicitly requests a requirement document
- User explicitly requests analysis documentation
- User specifically asks for comprehensive documentation

**NEVER create mega documents:**
- As part of completing a task
- To summarize work performed
- To document implementation details
- Without explicit user instruction

### **Mandatory Mega Document Creation Process**

**When explicitly requested, ALWAYS follow this exact process:**

#### **Step 1: Create Temporary Directory Structure**
```bash
# Create temporary directory for organizing relevant files
mkdir -p temp_mega_doc
```

#### **Step 2: Copy ONLY Relevant Source Modules**
```bash
# Copy ONLY the source modules directly related to the analysis
# Example for a backend bug analysis:
cp src/strategy_profiler.cpp temp_mega_doc/
cp src/adaptive_allocation_manager.cpp temp_mega_doc/
cp include/sentio/strategy_profiler.hpp temp_mega_doc/
cp include/sentio/adaptive_allocation_manager.hpp temp_mega_doc/
# ... only relevant files
```

#### **Step 3: Copy Bug Report or Requirement Document**
```bash
# Copy the bug report or requirement document to temp directory
cp BUG_REPORT_NAME.md temp_mega_doc/
# OR
cp REQUIREMENT_DOCUMENT_NAME.md temp_mega_doc/
```

#### **Step 4: Generate Mega Document**
```bash
# Use create_mega_document.py with the temp directory
python tools/create_mega_document.py \
  --directories temp_mega_doc \
  --title "Descriptive Title" \
  --description "Detailed description of the analysis" \
  --output megadocs/MEGA_DOCUMENT_NAME.md \
  --include-bug-report \
  --bug-report-file temp_mega_doc/BUG_REPORT_NAME.md
```

#### **Step 5: Clean Up Temporary Directory**
```bash
# Remove temporary directory after mega document creation
rm -rf temp_mega_doc
```

### **Critical Rules for File Selection**

**ONLY include files that are directly relevant to the analysis topic:**

#### ✅ **INCLUDE Files When Relevant**
- **Core Implementation**: Files directly implementing the analyzed functionality
- **Interface Headers**: Header files defining the interfaces being analyzed
- **Bug Report/Requirement**: The specific document that triggered the mega doc creation
- **Test Files**: Only tests directly related to the analyzed functionality
- **Configuration**: Only config files directly affecting the analyzed components

#### ❌ **EXCLUDE Files Always**
- **Unrelated Modules**: Files not connected to the analysis topic
- **Third-Party Libraries**: External dependencies and vendor code
- **Build Files**: Makefiles, CMake files, build scripts
- **Documentation**: General docs not specific to the analysis
- **Test Data**: Large data files, test datasets, sample files
- **Generated Files**: Auto-generated code, compiled binaries

### **Example Mega Document Creation Processes**

#### **Example 1: Backend Bug Analysis**
```bash
# Step 1: Create temp directory
mkdir -p temp_mega_doc

# Step 2: Copy only relevant backend files
cp src/strategy_profiler.cpp temp_mega_doc/
cp src/adaptive_allocation_manager.cpp temp_mega_doc/
cp src/universal_position_coordinator.cpp temp_mega_doc/
cp include/sentio/strategy_profiler.hpp temp_mega_doc/
cp include/sentio/adaptive_allocation_manager.hpp temp_mega_doc/
cp include/sentio/universal_position_coordinator.hpp temp_mega_doc/

# Step 3: Copy bug report
cp BACKEND_BUG_REPORT.md temp_mega_doc/

# Step 4: Generate mega document
python tools/create_mega_document.py \
  --directories temp_mega_doc \
  --title "Backend Critical Bug Analysis" \
  --description "Analysis of strategy-agnostic backend issues" \
  --output megadocs/BACKEND_BUG_ANALYSIS_MEGA_DOC.md \
  --include-bug-report \
  --bug-report-file temp_mega_doc/BACKEND_BUG_REPORT.md

# Step 5: Clean up
rm -rf temp_mega_doc
```

#### **Example 2: Strategy Enhancement Request**
```bash
# Step 1: Create temp directory
mkdir -p temp_mega_doc

# Step 2: Copy only relevant strategy files
cp src/strategy_signal_or.cpp temp_mega_doc/
cp include/sentio/strategy_signal_or.hpp temp_mega_doc/
cp include/sentio/detectors/rsi_detector.hpp temp_mega_doc/
cp include/sentio/detectors/bollinger_detector.hpp temp_mega_doc/

# Step 3: Copy requirement document
cp STRATEGY_ENHANCEMENT_REQUIREMENTS.md temp_mega_doc/

# Step 4: Generate mega document
python tools/create_mega_document.py \
  --directories temp_mega_doc \
  --title "Strategy Enhancement Analysis" \
  --description "Analysis for new strategy detector integration" \
  --output megadocs/STRATEGY_ENHANCEMENT_MEGA_DOC.md \
  --include-bug-report \
  --bug-report-file temp_mega_doc/STRATEGY_ENHANCEMENT_REQUIREMENTS.md

# Step 5: Clean up
rm -rf temp_mega_doc
```


### **Size Guidelines and Best Practices**

#### **Target Specifications**
- **Target Size**: 200-500 KB maximum for focused analysis
- **File Count**: 5-15 relevant files maximum (not 182 files!)
- **Content Focus**: Only files directly related to the analysis topic

#### **Benefits of Using the Temporary Directory Process**

1. **Focused Analysis**: Only relevant files included, not entire codebase
2. **Manageable Size**: Documents stay under 500 KB for efficient processing
3. **Clean Organization**: Temporary structure keeps project clean
4. **Comprehensive Context**: Bug report + relevant source code together
5. **AI-Optimized**: Right amount of context without information overload

### **Prohibited Actions**

❌ **NEVER DO**: Create mega documents without explicit user request
❌ **NEVER DO**: Create mega documents manually in chat  
❌ **NEVER DO**: Copy/paste large amounts of source code directly  
❌ **NEVER DO**: Create comprehensive documents without using the tool  
❌ **NEVER DO**: Write documents >1KB manually in responses
❌ **NEVER DO**: Update existing bug reports unless explicitly instructed
❌ **NEVER DO**: Create summary or completion documents
❌ **NEVER DO**: Include entire directories (src/, include/) without filtering
❌ **NEVER DO**: Skip the temporary directory process

✅ **ONLY DO WHEN EXPLICITLY REQUESTED**: Use the 5-step temporary directory process
✅ **WHEN REQUESTED**: Copy ONLY relevant source modules to temp directory
✅ **WHEN REQUESTED**: Include the bug report or requirement document
✅ **WHEN REQUESTED**: Use `create_mega_document.py` with `--directories temp_mega_doc`
✅ **WHEN REQUESTED**: Clean up temporary directory after creation

### **Common Use Cases**

- **Bug Analysis**: Performance issues, algorithm failures, integration problems
- **Feature Requests**: New algorithm implementations, UI enhancements, system upgrades  
- **Performance Reviews**: Benchmarking, optimization analysis, comparison studies
- **Architecture Analysis**: System design reviews, component integration studies
- **Code Reviews**: Comprehensive code analysis with multiple file context

### **Mega Document Size Management (CRITICAL)**

**MANDATORY**: When creating mega documents, include **ONLY relevant source modules** to maintain manageable document sizes.

#### **Size Guidelines**
- **Target Size**: 200-500 KB maximum for focused analysis
- **File Count**: 10-30 relevant files maximum
- **Content Focus**: Only include files directly related to the analysis topic

#### **File Selection Rules**

##### ✅ **INCLUDE Files When Relevant**
- **Core Implementation**: Files directly implementing the analyzed functionality
- **Interface Definitions**: Headers defining the analyzed components
- **Supporting Infrastructure**: Files that directly support the core functionality
- **Configuration**: Files that configure the analyzed behavior
- **Test Files**: Files that test the analyzed functionality

##### ❌ **EXCLUDE Files When Unrelated**
- **Strategy Implementations**: Unless analyzing strategy-specific issues
- **Feature Engineering**: Unless analyzing feature-specific problems
- **ML Models**: Unless analyzing model-specific issues
- **Portfolio Management**: Unless analyzing portfolio-specific problems
- **Signal Processing**: Unless analyzing signal-specific issues
- **UI Components**: Unless analyzing UI-specific problems
- **Utility Functions**: Unless directly related to the analysis

#### **Relevance Assessment Criteria**

**A file is RELEVANT if:**
1. **Direct Implementation**: Contains code that directly implements the analyzed functionality
2. **Core Interface**: Defines interfaces used by the analyzed functionality
3. **Configuration**: Contains settings that affect the analyzed behavior
4. **Supporting Logic**: Contains logic that directly supports the analyzed functionality
5. **Testing**: Contains tests that validate the analyzed functionality

**A file is UNRELATED if:**
1. **Different Domain**: Implements functionality in a different domain (e.g., UI when analyzing backend)
2. **Indirect Support**: Only indirectly supports the analyzed functionality
3. **Generic Utility**: Provides generic functionality not specific to the analysis
4. **Strategy-Specific**: Implements specific strategies when analyzing general framework issues
5. **Feature-Specific**: Implements specific features when analyzing core system issues

#### **Size Management Examples**

##### **Example 1: TPA Metrics Analysis**
**RELEVANT (15-20 files):**
- `include/sentio/metrics.hpp` - Core metrics calculation
- `src/temporal_analysis.cpp` - TPA implementation
- `src/runner.cpp` - Backtest execution
- `audit/src/audit_db.cpp` - Audit metrics calculation
- `audit/src/audit_cli.cpp` - Audit reporting
- `include/sentio/runner.hpp` - Runner interface
- `include/sentio/temporal_analysis.hpp` - TPA interface
- `include/sentio/base_strategy.hpp` - Strategy base class
- `src/audit.cpp` - Audit recorder
- `src/audit_validator.cpp` - Audit validation

**UNRELATED (100+ files):**
- All strategy implementations (`strategy_*.cpp`)
- Feature engineering modules (`feature_engineering/`)
- ML model implementations (`ml/`)
- Portfolio management (`portfolio/`)
- Signal processing (`signal_*.cpp`)

##### **Example 2: Strategy-Specific Bug**
**RELEVANT (5-10 files):**
- `src/strategy_ire.cpp` - Specific strategy implementation
- `include/sentio/strategy_ire.hpp` - Strategy interface
- `include/sentio/base_strategy.hpp` - Base strategy class
- `src/runner.cpp` - Strategy execution
- `include/sentio/runner.hpp` - Runner interface

**UNRELATED (150+ files):**
- All other strategy implementations
- Audit system files
- Feature engineering modules
- ML model implementations
- Portfolio management files

#### **Pre-Creation Checklist**

Before creating a mega document, verify:
- [ ] **File Relevance**: Each included file directly relates to the analysis topic
- [ ] **Size Estimate**: Document will be <500 KB
- [ ] **File Count**: <30 files included
- [ ] **Domain Focus**: Files are from the same functional domain
- [ ] **Analysis Scope**: Files match the analysis scope and depth

#### **Size Monitoring Commands**

```bash
# Check mega document size before creation
ls -lh megadocs/*.md | awk '{print $5, $9}' | sort -hr

# Count files in mega document
grep "^## 📄 \*\*FILE.*\*\*:" megadocs/DOCUMENT_NAME.md | wc -l

# List files included in mega document
grep "^## 📄 \*\*FILE.*\*\*:" megadocs/DOCUMENT_NAME.md | sed 's/## 📄 \*\*FILE [0-9]* of [0-9]*\*\*: //'
```

### **Tool Command Reference**

```bash
# Basic usage with config file
python3 tools/create_mega_document.py \
  --config path/to/config.json \
  --title "Document Title" \
  --description "Document description" \
  --output path/to/output.md

# Direct file list (without config)
python3 tools/create_mega_document.py \
  --files file1.py file2.md file3.json \
  --title "Document Title" \
  --description "Document description" \
  --output path/to/output.md

# With custom source directory
python3 tools/create_mega_document.py \
  --config config.json \
  --source ./algorithms \
  --title "Algorithm Analysis" \
  --description "Algorithm implementation review" \
  --output bug_reports/ALGORITHM_ANALYSIS_MEGA.md
```

---

## 🔧 Development Workflow

### **Standard Development Process**

1. **Code Changes**: Implement features following architecture rules
2. **Integration**: Ensure GUI and multi-algorithm integration
3. **Testing**: Verify functionality and performance
4. **Documentation**: Update `docs/ARCHITECTURE.md` and `docs/README.md`
5. **Cleanup**: Remove any temporary files or outdated information

### **Feature Addition Checklist**

- [ ] Integrates with multi-algorithm system
- [ ] Has GUI controls and monitoring
- [ ] Includes performance tracking
- [ ] Uses Kafka messaging appropriately
- [ ] Follows error handling patterns
- [ ] Updates `docs/ARCHITECTURE.md`
- [ ] Updates `docs/README.md`
- [ ] No new documentation files created

### **Bug Fix Checklist**

- [ ] Identifies root cause
- [ ] Implements proper fix
- [ ] Tests fix thoroughly
- [ ] Updates documentation if architecture affected
- [ ] No temporary documentation created

---

## 🎯 Quality Standards

### **Code Quality**

1. **Performance**: Sub-second response times for GUI operations
2. **Reliability**: Graceful error handling and recovery
3. **Scalability**: Support for multiple algorithms and symbols
4. **Maintainability**: Clear, documented code structure
5. **Integration**: Seamless component interaction

### **Logging Policy (Mandatory)**

All components must use centralized, structured JSON logging:

1. Initialize once via `core.json_logging.configure_json_logging()` at process start.
2. Do not use `print()` in production code. Use `logging.getLogger(__name__)` only.
3. JSON fields emitted by default: `timestamp`, `level`, `logger`, `message`, `run_id`.
4. Include when available: `algo`, `symbol`, `order_id`, `event_seq`, `event`, `component`.
5. Sinks: stdout and `logs/app.jsonl`. Errors also recorded in `logs/errors.log`.
6. Domain messages (Kafka/persistence) must carry `run_id` and `event_seq`.
7. UI and background threads must not directly mutate widgets; emit events/logs instead.

### **Documentation Quality**

1. **Accuracy**: Documentation matches current codebase exactly
2. **Completeness**: All features and components documented
3. **Clarity**: Clear instructions and explanations
4. **Currency**: No outdated information
5. **Consolidation**: All information in two files only

### **User Experience**

1. **Intuitive GUI**: Easy-to-use interface
2. **Real-time Feedback**: Live performance monitoring
3. **Professional Appearance**: Consistent theme system
4. **Reliable Operation**: Stable, predictable behavior
5. **Clear Documentation**: Easy setup and usage instructions

---

## 🧭 UI Design Principles (Global)

- **Grid, alignment, and spacing**
  - Use a consistent grid (e.g., 8pt) with aligned gutters; avoid wide, unallocated columns
  - Keep vertical rhythm consistent; don’t leave single giant blocks surrounded by empty space

- **Information density controls**
  - Provide density modes (compact/comfortable) for tables and lists
  - Balance whitespace: enough for scanability, not so much that primary content gets crowded

- **Container‑aware sizing**
  - Constrain components with sensible min/max sizes
  - Avoid full‑width for low‑content widgets; prefer fit‑to‑content unless the content benefits from “fill available” (e.g., text areas, charts)

- **Visual balance and hierarchy**
  - Use typographic scale, contrast, and proximity to structure content
  - Place secondary metrics/actions in otherwise empty zones only when it improves workflows

- **Content‑first layout**
  - Prioritize the information users read/act on most
  - Allocate area proportionally to content importance and frequency of use


## 🚨 Enforcement

### **Automatic Checks**

AI models should verify:
- No new `.md` files created
- Both documentation files updated when needed
- Architecture changes reflected in documentation
- User-facing changes reflected in README

### **Review Requirements**

Before completing any work:
1. **Architecture Review**: Changes match documented architecture
2. **Documentation Review**: Both files are current and accurate
3. **Integration Review**: Components work together properly
4. **Performance Review**: System meets performance standards

---

## 📈 Success Metrics

### **Documentation Success**
- **Single Source of Truth**: All information in two files
- **Always Current**: Documentation matches codebase
- **User-Friendly**: Clear installation and usage instructions
- **Technically Complete**: Full architecture documentation

### **System Success**
- **Multi-Algorithm Performance**: All algorithms integrated and performing
- **GUI Functionality**: Complete control and monitoring interface
- **Real-time Operation**: Live trading and performance tracking
- **Professional Quality**: Institutional-grade trading platform

---

## 🎉 Conclusion

These rules ensure Sentio Trader maintains:
- **Clean Documentation**: Two comprehensive, current files
- **Professional Architecture**: Consistent, scalable system design
- **Quality Standards**: Reliable, high-performance operation
- **User Experience**: Clear, intuitive interface and documentation
- **Controlled Documentation**: No unsolicited document creation

### **KEY POLICY SUMMARY**

#### **Document Creation Policy**
- **Source Code**: ✅ Create as needed to implement user instructions
- **Configuration Files**: ✅ Create as needed for functionality
- **Documentation Files**: ❌ NEVER create unless explicitly requested
- **Bug Reports**: ❌ NEVER create unless explicitly requested
- **Analysis Documents**: ❌ NEVER create unless explicitly requested
- **Summary Reports**: ❌ NEVER create (provide brief chat summaries instead)

#### **Default AI Behavior**
1. **Complete the requested task** with source code changes
2. **Update permanent docs** (docs/ARCHITECTURE.md, docs/README.md) if needed
3. **Provide brief summary** in chat response
4. **DO NOT create any documentation files** unless specifically asked

**All AI models and contributors must follow these rules without exception.**

## 🏗️ **Architecture Consolidation Rules (Added 2025-09-02)**

- **UNIFIED SYSTEM**: Use `sentio_exec` for all new strategy development, testing, and production deployment
- **DEPRECATED MODULE**: `sentio_unified` is DEPRECATED - do not use for new development  
- **CONSOLIDATION RATIONALE**: Eliminates duplicate classes, provides unified Trade Manager architecture, maintains zero-tolerance for code duplication
- **MIGRATION PATH**: All `sentio_unified` functionality consolidated into `sentio_exec` with enhanced features

---

*Sentio Trader Project Rules - Ensuring Quality and Consistency*
