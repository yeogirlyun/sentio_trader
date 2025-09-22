#!/usr/bin/env python3
"""
Enhanced C++ Code Analyzer with AST-based parsing
Uses Clang Python bindings for accurate C++ analysis
"""

import os
import sys
import json
import hashlib
import argparse
from pathlib import Path
from collections import defaultdict, Counter
from datetime import datetime
from typing import Dict, List, Set, Tuple, Optional, Any
import subprocess

# Try to import clang bindings
try:
    import clang.cindex as clang
except ImportError:
    print("Error: libclang Python bindings not found.")
    print("Install with: pip install libclang")
    print("You may also need to install clang: apt-get install clang or brew install llvm")
    sys.exit(1)

# Initialize clang
def init_clang():
    """Initialize clang library path"""
    # Common paths for libclang
    possible_paths = [
        "/usr/lib/llvm-14/lib",
        "/usr/lib/llvm-13/lib",
        "/usr/lib/llvm-12/lib",
        "/usr/lib/llvm-11/lib",
        "/usr/lib/llvm-10/lib",
        "/usr/local/opt/llvm/lib",
        "/Library/Developer/CommandLineTools/usr/lib",
        "/opt/homebrew/opt/llvm/lib",  # Apple Silicon Homebrew
    ]
    
    for path in possible_paths:
        if os.path.exists(path):
            clang.Config.set_library_path(path)
            break

init_clang()

class CodeMetrics:
    """Calculate various code metrics"""
    
    @staticmethod
    def cyclomatic_complexity(cursor) -> int:
        """Calculate cyclomatic complexity of a function"""
        complexity = 1
        
        try:
            for child in cursor.walk_preorder():
                # Check for control flow statements
                if child.kind in [
                    clang.CursorKind.IF_STMT,
                    clang.CursorKind.WHILE_STMT,
                    clang.CursorKind.FOR_STMT,
                    clang.CursorKind.CASE_STMT,
                    clang.CursorKind.DEFAULT_STMT,
                ]:
                    complexity += 1
                # Add CATCH_STMT if available in this clang version
                elif hasattr(clang.CursorKind, 'CATCH_STMT') and child.kind == clang.CursorKind.CATCH_STMT:
                    complexity += 1
                elif child.kind == clang.CursorKind.BINARY_OPERATOR:
                    try:
                        tokens = list(child.get_tokens())
                        for token in tokens:
                            if token.spelling in ['&&', '||']:
                                complexity += 1
                    except:
                        # Skip if token access fails
                        pass
        except Exception as e:
            # Fallback to basic complexity if AST traversal fails
            return 1
        
        return complexity
    
    @staticmethod
    def count_lines(cursor) -> int:
        """Count lines of code in a cursor"""
        if cursor.extent.start.file and cursor.extent.end.file:
            return cursor.extent.end.line - cursor.extent.start.line + 1
        return 0
    
    @staticmethod
    def count_parameters(cursor) -> int:
        """Count function parameters"""
        if cursor.kind == clang.CursorKind.FUNCTION_DECL:
            return len(list(cursor.get_arguments()))
        return 0

class CallGraphAnalyzer:
    """Build and analyze function call graphs"""
    
    def __init__(self):
        self.functions = {}  # function_name -> cursor info
        self.calls = defaultdict(set)  # function -> set of called functions
        self.called_by = defaultdict(set)  # function -> set of callers
        self.virtual_functions = set()
        self.exported_functions = set()
        self.template_functions = set()
        
    def add_function(self, cursor, file_path: str):
        """Add function definition to the graph"""
        func_name = cursor.spelling
        self.functions[func_name] = {
            'cursor': cursor,
            'file': file_path,
            'line': cursor.location.line,
            'is_virtual': cursor.is_virtual_method(),
            'is_static': cursor.is_static_method(),
            'is_template': cursor.kind == clang.CursorKind.FUNCTION_TEMPLATE,
            'access': cursor.access_specifier.name if hasattr(cursor, 'access_specifier') else 'PUBLIC'
        }
        
        if cursor.is_virtual_method():
            self.virtual_functions.add(func_name)
        
        if cursor.kind == clang.CursorKind.FUNCTION_TEMPLATE:
            self.template_functions.add(func_name)
            
        # Check if function is exported (simplified check)
        if cursor.linkage == clang.LinkageKind.EXTERNAL:
            self.exported_functions.add(func_name)
    
    def add_call(self, caller: str, callee: str):
        """Add a function call relationship"""
        self.calls[caller].add(callee)
        self.called_by[callee].add(caller)
    
    def find_unused_functions(self) -> List[Dict]:
        """Find functions that are never called"""
        unused = []
        
        # Entry points that should not be marked as unused
        entry_points = {'main', 'Main', 'WinMain', 'DllMain'}
        
        for func_name, func_info in self.functions.items():
            # Skip entry points
            if func_name in entry_points:
                continue
                
            # Skip virtual functions (may be called polymorphically)
            if func_name in self.virtual_functions:
                continue
                
            # Skip exported functions (may be called externally)
            if func_name in self.exported_functions:
                continue
                
            # Skip template functions (instantiated on demand)
            if func_name in self.template_functions:
                continue
                
            # Skip constructors and destructors
            if func_name.startswith('~') or func_name == func_info.get('class_name', ''):
                continue
                
            # Check if function is called
            if func_name not in self.called_by or not self.called_by[func_name]:
                unused.append({
                    'name': func_name,
                    'file': func_info['file'],
                    'line': func_info['line'],
                    'access': func_info.get('access', 'UNKNOWN')
                })
        
        return unused
    
    def find_circular_dependencies(self) -> List[List[str]]:
        """Find circular function call dependencies"""
        cycles = []
        visited = set()
        rec_stack = set()
        
        def dfs(node, path):
            if node in rec_stack:
                # Found a cycle
                cycle_start = path.index(node)
                cycle = path[cycle_start:]
                cycles.append(cycle)
                return
            
            if node in visited:
                return
                
            visited.add(node)
            rec_stack.add(node)
            path.append(node)
            
            for neighbor in self.calls.get(node, []):
                dfs(neighbor, path[:])
            
            rec_stack.remove(node)
        
        for func in self.functions:
            if func not in visited:
                dfs(func, [])
        
        return cycles

class DuplicateDetector:
    """Advanced duplicate detection using AST comparison"""
    
    def __init__(self):
        self.token_sequences = defaultdict(list)
        self.ast_hashes = defaultdict(list)
        self.semantic_signatures = defaultdict(list)
    
    def get_token_sequence(self, cursor) -> str:
        """Extract normalized token sequence from cursor"""
        tokens = []
        for token in cursor.get_tokens():
            # Normalize identifiers for structural comparison
            if token.kind == clang.TokenKind.IDENTIFIER:
                tokens.append("ID")
            elif token.kind == clang.TokenKind.LITERAL:
                tokens.append("LIT")
            else:
                tokens.append(token.spelling)
        return ' '.join(tokens)
    
    def get_ast_hash(self, cursor) -> str:
        """Generate hash of AST structure"""
        def traverse(node, depth=0):
            result = f"{node.kind.name}"
            if depth < 5:  # Limit depth to avoid infinite recursion
                for child in node.get_children():
                    result += traverse(child, depth + 1)
            return result
        
        ast_string = traverse(cursor)
        return hashlib.md5(ast_string.encode()).hexdigest()
    
    def get_semantic_signature(self, cursor) -> str:
        """Generate semantic signature for similarity detection"""
        features = []
        
        # Function characteristics
        if cursor.kind == clang.CursorKind.FUNCTION_DECL:
            features.append(f"params:{CodeMetrics.count_parameters(cursor)}")
            features.append(f"complexity:{CodeMetrics.cyclomatic_complexity(cursor)}")
            features.append(f"lines:{CodeMetrics.count_lines(cursor)}")
            
            # Count different statement types
            stmt_counts = Counter()
            for child in cursor.walk_preorder():
                if 'STMT' in child.kind.name:
                    stmt_counts[child.kind.name] += 1
            
            for stmt_type, count in stmt_counts.most_common(5):
                features.append(f"{stmt_type}:{count}")
        
        return '|'.join(features)
    
    def add_function(self, cursor, file_path: str, start_line: int):
        """Add function to duplicate detection"""
        func_name = cursor.spelling
        
        # Get different representations
        token_seq = self.get_token_sequence(cursor)
        ast_hash = self.get_ast_hash(cursor)
        semantic_sig = self.get_semantic_signature(cursor)
        
        # Store in appropriate buckets
        self.token_sequences[token_seq].append({
            'name': func_name,
            'file': file_path,
            'line': start_line,
            'cursor': cursor
        })
        
        self.ast_hashes[ast_hash].append({
            'name': func_name,
            'file': file_path,
            'line': start_line,
            'cursor': cursor
        })
        
        self.semantic_signatures[semantic_sig].append({
            'name': func_name,
            'file': file_path,
            'line': start_line,
            'cursor': cursor
        })
    
    def get_duplicates(self) -> Dict[str, List]:
        """Get all types of duplicates"""
        return {
            'exact': [funcs for funcs in self.token_sequences.values() if len(funcs) > 1],
            'structural': [funcs for funcs in self.ast_hashes.values() if len(funcs) > 1],
            'semantic': [funcs for funcs in self.semantic_signatures.values() if len(funcs) > 1]
        }

class RefactoringEngine:
    """Generate refactoring suggestions and code"""
    
    @staticmethod
    def suggest_extract_method(cursor, complexity: int) -> Dict[str, Any]:
        """Suggest method extraction for complex functions"""
        suggestions = []
        
        # Find extractable blocks
        for child in cursor.walk_preorder():
            if child.kind in [clang.CursorKind.COMPOUND_STMT]:
                block_complexity = CodeMetrics.cyclomatic_complexity(child)
                if block_complexity > 3:
                    suggestions.append({
                        'type': 'extract_method',
                        'location': child.location.line,
                        'complexity': block_complexity,
                        'recommendation': f"Extract lines {child.extent.start.line}-{child.extent.end.line} into separate method"
                    })
        
        return {
            'function': cursor.spelling,
            'current_complexity': complexity,
            'suggestions': suggestions
        }
    
    @staticmethod
    def suggest_parameter_object(cursor, param_count: int) -> Dict[str, Any]:
        """Suggest parameter object for functions with many parameters"""
        params = list(cursor.get_arguments())
        
        # Group related parameters
        param_groups = []
        current_group = []
        
        for param in params:
            param_type = param.type.spelling
            param_name = param.spelling
            current_group.append({'name': param_name, 'type': param_type})
            
            # Simple heuristic: group every 3 params
            if len(current_group) >= 3:
                param_groups.append(current_group)
                current_group = []
        
        if current_group:
            param_groups.append(current_group)
        
        return {
            'function': cursor.spelling,
            'parameter_count': param_count,
            'suggested_objects': param_groups,
            'code_template': RefactoringEngine._generate_parameter_object_code(cursor.spelling, param_groups)
        }
    
    @staticmethod
    def _generate_parameter_object_code(func_name: str, param_groups: List) -> str:
        """Generate parameter object class template"""
        code = []
        
        for i, group in enumerate(param_groups):
            code.append(f"// Parameter object for {func_name}")
            code.append(f"struct {func_name}Params{i+1} {{")
            for param in group:
                code.append(f"    {param['type']} {param['name']};")
            code.append("};")
            code.append("")
        
        return '\n'.join(code)
    
    @staticmethod
    def suggest_class_decomposition(class_name: str, method_count: int, variable_count: int) -> Dict[str, Any]:
        """Suggest class decomposition for large classes"""
        suggestions = []
        
        if method_count > 20:
            suggestions.append({
                'type': 'split_by_responsibility',
                'recommendation': f"Class has {method_count} methods. Consider splitting by responsibility.",
                'pattern': 'Extract classes for distinct responsibilities'
            })
        
        if variable_count > 10:
            suggestions.append({
                'type': 'extract_data_class',
                'recommendation': f"Class has {variable_count} member variables. Consider extracting data classes.",
                'pattern': 'Create separate data holder classes'
            })
        
        return {
            'class': class_name,
            'method_count': method_count,
            'variable_count': variable_count,
            'suggestions': suggestions
        }

class DesignPatternDetector:
    """Detect common design patterns and anti-patterns"""
    
    @staticmethod
    def detect_singleton(cursor) -> bool:
        """Detect singleton pattern"""
        has_private_constructor = False
        has_static_instance = False
        
        for child in cursor.get_children():
            if child.kind == clang.CursorKind.CONSTRUCTOR:
                if child.access_specifier == clang.AccessSpecifier.PRIVATE:
                    has_private_constructor = True
            elif child.kind == clang.CursorKind.CXX_METHOD:
                if child.is_static_method() and 'instance' in cursor.spelling.lower():
                    has_static_instance = True
        
        return has_private_constructor and has_static_instance
    
    @staticmethod
    def detect_factory(cursor) -> bool:
        """Detect factory pattern"""
        create_methods = 0
        
        for child in cursor.get_children():
            if child.kind == clang.CursorKind.CXX_METHOD:
                if 'create' in child.spelling.lower() or 'make' in child.spelling.lower():
                    create_methods += 1
        
        return create_methods >= 2
    
    @staticmethod
    def detect_god_class(method_count: int, variable_count: int) -> bool:
        """Detect god class anti-pattern"""
        return method_count > 30 or variable_count > 20
    
    @staticmethod
    def detect_feature_envy(cursor) -> Dict[str, int]:
        """Detect feature envy - methods using other classes more than their own"""
        external_access = Counter()
        internal_access = 0
        
        for child in cursor.walk_preorder():
            if child.kind == clang.CursorKind.MEMBER_REF_EXPR:
                # Check if accessing external class members
                if child.semantic_parent != cursor.semantic_parent:
                    external_access[child.type.spelling] += 1
                else:
                    internal_access += 1
        
        return {
            'external_accesses': dict(external_access),
            'internal_accesses': internal_access,
            'has_feature_envy': any(count > internal_access for count in external_access.values())
        }

class ArchitecturalAnalyzer:
    """Analyze software architecture and dependencies"""
    
    def __init__(self):
        self.module_dependencies = defaultdict(set)
        self.layer_violations = []
        self.circular_dependencies = []
        
    def analyze_dependencies(self, includes: Dict[str, Set[str]]) -> Dict[str, Any]:
        """Analyze module dependencies"""
        # Group files into modules (simplified - based on directory)
        modules = defaultdict(set)
        for file_path in includes.keys():
            module = Path(file_path).parent.name
            if not module:
                module = 'root'
            modules[module].add(file_path)
        
        # Build module dependency graph
        for file_path, included_files in includes.items():
            source_module = Path(file_path).parent.name or 'root'
            
            for included in included_files:
                # Try to determine target module
                if '/' in included:
                    target_module = included.split('/')[0]
                else:
                    target_module = 'system'
                
                if target_module != source_module:
                    self.module_dependencies[source_module].add(target_module)
        
        # Detect circular dependencies
        self._detect_circular_dependencies()
        
        # Detect layer violations (simplified)
        self._detect_layer_violations()
        
        return {
            'modules': dict(modules),
            'dependencies': dict(self.module_dependencies),
            'circular_dependencies': self.circular_dependencies,
            'layer_violations': self.layer_violations
        }
    
    def _detect_circular_dependencies(self):
        """Detect circular dependencies between modules"""
        visited = set()
        rec_stack = set()
        
        def dfs(module, path):
            if module in rec_stack:
                cycle_start = path.index(module)
                self.circular_dependencies.append(path[cycle_start:])
                return
            
            if module in visited:
                return
            
            visited.add(module)
            rec_stack.add(module)
            path.append(module)
            
            for dep in self.module_dependencies.get(module, []):
                dfs(dep, path[:])
            
            rec_stack.remove(module)
        
        for module in self.module_dependencies:
            if module not in visited:
                dfs(module, [])
    
    def _detect_layer_violations(self):
        """Detect violations of layered architecture"""
        # Define typical layers (simplified)
        layers = {
            'ui': 0,
            'controller': 1,
            'service': 2,
            'business': 2,
            'domain': 3,
            'data': 4,
            'database': 4,
            'infrastructure': 5
        }
        
        for source_module, targets in self.module_dependencies.items():
            source_layer = next((layer for name, layer in layers.items() if name in source_module.lower()), -1)
            
            for target in targets:
                target_layer = next((layer for name, layer in layers.items() if name in target.lower()), -1)
                
                # Check for upward dependencies (violations)
                if source_layer > 0 and target_layer >= 0 and target_layer < source_layer:
                    self.layer_violations.append({
                        'source': source_module,
                        'target': target,
                        'violation': f"Lower layer '{source_module}' depends on higher layer '{target}'"
                    })

class PerformanceAnalyzer:
    """Analyze potential performance issues"""
    
    @staticmethod
    def analyze_function_performance(cursor) -> Dict[str, Any]:
        """Analyze function for performance issues"""
        issues = []
        
        # Check for expensive operations in loops
        in_loop = False
        loop_depth = 0
        
        for child in cursor.walk_preorder():
            if child.kind in [clang.CursorKind.FOR_STMT, clang.CursorKind.WHILE_STMT, clang.CursorKind.DO_STMT]:
                in_loop = True
                loop_depth += 1
            elif child.kind == clang.CursorKind.CALL_EXPR and in_loop:
                # Check for expensive calls in loops
                if child.spelling in ['malloc', 'new', 'delete', 'free']:
                    issues.append({
                        'type': 'allocation_in_loop',
                        'location': child.location.line,
                        'depth': loop_depth,
                        'severity': 'high',
                        'recommendation': 'Consider pre-allocating memory outside the loop'
                    })
                elif child.spelling in ['strlen', 'sizeof']:
                    issues.append({
                        'type': 'repeated_calculation',
                        'location': child.location.line,
                        'depth': loop_depth,
                        'severity': 'medium',
                        'recommendation': 'Cache the result outside the loop'
                    })
        
        # Check for unnecessary copies
        for child in cursor.get_arguments():
            param_type = child.type.spelling
            if 'std::vector' in param_type or 'std::string' in param_type:
                if not ('&' in param_type or 'const' in param_type):
                    issues.append({
                        'type': 'expensive_copy',
                        'parameter': child.spelling,
                        'type': param_type,
                        'severity': 'medium',
                        'recommendation': f"Pass '{child.spelling}' by const reference"
                    })
        
        return {
            'function': cursor.spelling,
            'performance_issues': issues
        }

class SecurityAnalyzer:
    """Analyze potential security vulnerabilities"""
    
    # Unsafe functions that should be avoided
    UNSAFE_FUNCTIONS = {
        'strcpy': 'Use strncpy or strlcpy instead',
        'strcat': 'Use strncat or strlcat instead',
        'sprintf': 'Use snprintf instead',
        'gets': 'Use fgets instead',
        'scanf': 'Use fgets + sscanf with bounds checking',
        'vsprintf': 'Use vsnprintf instead'
    }
    
    @staticmethod
    def analyze_security(cursor) -> List[Dict[str, Any]]:
        """Analyze function for security issues"""
        issues = []
        
        for child in cursor.walk_preorder():
            if child.kind == clang.CursorKind.CALL_EXPR:
                func_name = child.spelling
                
                # Check for unsafe functions
                if func_name in SecurityAnalyzer.UNSAFE_FUNCTIONS:
                    issues.append({
                        'type': 'unsafe_function',
                        'function': func_name,
                        'location': child.location.line,
                        'severity': 'high',
                        'recommendation': SecurityAnalyzer.UNSAFE_FUNCTIONS[func_name]
                    })
                
                # Check for potential buffer overflow
                elif func_name == 'memcpy':
                    issues.append({
                        'type': 'potential_buffer_overflow',
                        'function': func_name,
                        'location': child.location.line,
                        'severity': 'medium',
                        'recommendation': 'Ensure size parameter is validated'
                    })
        
        return issues

class EnhancedCppAnalyzer:
    """Enhanced C++ analyzer with AST-based analysis"""
    
    def __init__(self, root_dir: str = '.', compilation_db: Optional[str] = None):
        self.root_dir = Path(root_dir)
        self.compilation_db = compilation_db
        self.cpp_extensions = {'.h', '.hpp', '.cpp', '.cc', '.cxx', '.c++'}
        self.source_files = []
        self.translation_units = []
        
        # Analysis components
        self.duplicate_detector = DuplicateDetector()
        self.call_graph = CallGraphAnalyzer()
        self.architectural_analyzer = ArchitecturalAnalyzer()
        
        # Metrics
        self.file_metrics = {}
        self.function_metrics = {}
        self.class_metrics = {}
        self.includes = defaultdict(set)
        
        # Issues
        self.issues = {
            'high_priority': [],
            'medium_priority': [],
            'low_priority': []
        }
        
    def find_source_files(self) -> int:
        """Find all C++ source files"""
        for path in self.root_dir.rglob('*'):
            if (path.suffix in self.cpp_extensions and 
                not self._is_generated_file(path)):
                self.source_files.append(path)
        return len(self.source_files)
    
    def _is_generated_file(self, path: Path) -> bool:
        """Check if file is generated (build artifacts, etc.)"""
        generated_patterns = [
            'build/', 'cmake/', '.git/', 'node_modules/',
            'CMakeFiles/', 'moc_', 'ui_', '.pb.h', '.pb.cc'
        ]
        return any(pattern in str(path) for pattern in generated_patterns)
    
    def parse_file(self, file_path: Path) -> Optional[clang.TranslationUnit]:
        """Parse a C++ file into AST"""
        index = clang.Index.create()
        
        # Compilation flags
        flags = [
            '-std=c++17',
            '-I/usr/include',
            '-I/usr/local/include',
            '-I/Library/Developer/CommandLineTools/usr/include/c++/v1',
            '-I/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include',
            f'-I{self.root_dir}/include',
            '-D__CODE_ANALYSIS__',
            '-stdlib=libc++',
            '-x', 'c++'
        ]
        
        # If we have a compilation database, use it
        if self.compilation_db:
            # Load compilation commands from compile_commands.json
            pass  # Simplified for now
        
        try:
            tu = index.parse(
                str(file_path),
                args=flags,
                options=clang.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
            )
            
            # Check for parse errors
            if tu.diagnostics:
                errors = [d for d in tu.diagnostics if d.severity >= clang.Diagnostic.Error]
                warnings = [d for d in tu.diagnostics if d.severity == clang.Diagnostic.Warning]
                
                if errors:
                    print(f"  Parse errors in {file_path.name}: {len(errors)} errors")
                    # Only show first error to avoid spam
                    if errors:
                        print(f"    - {errors[0].spelling}")
                elif warnings:
                    print(f"  Parse warnings in {file_path.name}: {len(warnings)} warnings")
            
            return tu
            
        except Exception as e:
            print(f"  Error parsing {file_path}: {e}")
            return None
    
    def analyze_function(self, cursor, file_path: str):
        """Analyze a function declaration/definition"""
        if not cursor.spelling:  # Skip anonymous functions
            return
            
        func_name = cursor.spelling
        
        # Calculate metrics
        complexity = CodeMetrics.cyclomatic_complexity(cursor)
        lines = CodeMetrics.count_lines(cursor)
        params = CodeMetrics.count_parameters(cursor)
        
        # Store metrics
        self.function_metrics[f"{file_path}:{func_name}"] = {
            'name': func_name,
            'file': file_path,
            'line': cursor.location.line,
            'complexity': complexity,
            'lines': lines,
            'parameters': params
        }
        
        # Check for high complexity
        if complexity > 10:
            self.issues['high_priority' if complexity > 20 else 'medium_priority'].append({
                'type': 'high_complexity',
                'function': func_name,
                'file': file_path,
                'line': cursor.location.line,
                'complexity': complexity,
                'recommendation': f"Consider breaking down this function (complexity: {complexity})"
            })
        
        # Check for long functions
        if lines > 50:
            self.issues['medium_priority' if lines > 100 else 'low_priority'].append({
                'type': 'long_function',
                'function': func_name,
                'file': file_path,
                'line': cursor.location.line,
                'lines': lines,
                'recommendation': f"Consider splitting this function ({lines} lines)"
            })
        
        # Check for too many parameters
        if params > 5:
            self.issues['medium_priority'].append({
                'type': 'too_many_parameters',
                'function': func_name,
                'file': file_path,
                'line': cursor.location.line,
                'parameters': params,
                'recommendation': f"Consider using a parameter object ({params} parameters)"
            })
        
        # Add to duplicate detector
        self.duplicate_detector.add_function(cursor, file_path, cursor.location.line)
        
        # Add to call graph
        self.call_graph.add_function(cursor, file_path)
        
        # Analyze function calls within this function
        for child in cursor.walk_preorder():
            if child.kind == clang.CursorKind.CALL_EXPR:
                called_func = child.spelling
                if called_func:
                    self.call_graph.add_call(func_name, called_func)
        
        # Analyze performance issues
        perf_issues = PerformanceAnalyzer.analyze_function_performance(cursor)
        for issue in perf_issues.get('performance_issues', []):
            self.issues[f"{issue['severity']}_priority"].append({
                'type': f"performance_{issue['type']}",
                'function': func_name,
                'file': file_path,
                'line': issue.get('location', cursor.location.line),
                'recommendation': issue['recommendation']
            })
        
        # Analyze security issues
        security_issues = SecurityAnalyzer.analyze_security(cursor)
        for issue in security_issues:
            self.issues[f"{issue['severity']}_priority"].append({
                'type': f"security_{issue['type']}",
                'function': func_name,
                'file': file_path,
                'line': issue['location'],
                'recommendation': issue['recommendation']
            })
    
    def analyze_class(self, cursor, file_path: str):
        """Analyze a class/struct declaration"""
        class_name = cursor.spelling
        if not class_name:
            return
            
        # Count members
        public_methods = 0
        private_methods = 0
        protected_methods = 0
        member_variables = 0
        
        for child in cursor.get_children():
            if child.kind == clang.CursorKind.CXX_METHOD:
                if child.access_specifier == clang.AccessSpecifier.PUBLIC:
                    public_methods += 1
                elif child.access_specifier == clang.AccessSpecifier.PRIVATE:
                    private_methods += 1
                else:
                    protected_methods += 1
            elif child.kind == clang.CursorKind.FIELD_DECL:
                member_variables += 1
        
        total_methods = public_methods + private_methods + protected_methods
        
        # Store metrics
        self.class_metrics[f"{file_path}:{class_name}"] = {
            'name': class_name,
            'file': file_path,
            'line': cursor.location.line,
            'public_methods': public_methods,
            'private_methods': private_methods,
            'protected_methods': protected_methods,
            'member_variables': member_variables,
            'total_methods': total_methods
        }
        
        # Check for large classes (God class anti-pattern)
        if DesignPatternDetector.detect_god_class(total_methods, member_variables):
            self.issues['high_priority'].append({
                'type': 'god_class',
                'class': class_name,
                'file': file_path,
                'line': cursor.location.line,
                'methods': total_methods,
                'variables': member_variables,
                'recommendation': 'Consider splitting this class (Single Responsibility Principle)'
            })
        elif total_methods > 15 or member_variables > 8:
            self.issues['medium_priority'].append({
                'type': 'large_class',
                'class': class_name,
                'file': file_path,
                'line': cursor.location.line,
                'methods': total_methods,
                'variables': member_variables,
                'recommendation': 'Consider splitting this class (Single Responsibility Principle)'
            })
        
        # Detect design patterns
        if DesignPatternDetector.detect_singleton(cursor):
            self.issues['low_priority'].append({
                'type': 'singleton_pattern',
                'class': class_name,
                'file': file_path,
                'line': cursor.location.line,
                'recommendation': 'Singleton detected - consider dependency injection instead'
            })
    
    def analyze_includes(self, cursor, file_path: str):
        """Analyze include dependencies"""
        for child in cursor.get_children():
            if child.kind == clang.CursorKind.INCLUSION_DIRECTIVE:
                included_file = child.spelling
                if included_file:
                    self.includes[file_path].add(included_file)
    
    def analyze_ast(self, cursor, file_path: str, depth: int = 0):
        """Recursively analyze the AST"""
        # Skip if we're in a different file (e.g., included headers)
        if cursor.location.file and cursor.location.file.name != file_path:
            return
            
        # Analyze based on cursor kind
        if cursor.kind == clang.CursorKind.FUNCTION_DECL:
            if cursor.is_definition():
                self.analyze_function(cursor, file_path)
        elif cursor.kind in [clang.CursorKind.CLASS_DECL, clang.CursorKind.STRUCT_DECL]:
            if cursor.is_definition():
                self.analyze_class(cursor, file_path)
        elif cursor.kind == clang.CursorKind.CXX_METHOD:
            if cursor.is_definition():
                self.analyze_function(cursor, file_path)
        elif cursor.kind == clang.CursorKind.INCLUSION_DIRECTIVE:
            self.analyze_includes(cursor, file_path)
        
        # Recurse into children
        for child in cursor.get_children():
            self.analyze_ast(child, file_path, depth + 1)
    
    def analyze(self) -> Dict[str, Any]:
        """Main analysis function"""
        print(f"Starting enhanced C++ analysis in {self.root_dir}")
        
        # Find source files
        file_count = self.find_source_files()
        print(f"Found {file_count} C++ source files")
        
        # Parse and analyze each file
        for i, file_path in enumerate(self.source_files, 1):
            print(f"Analyzing {i}/{file_count}: {file_path.name}")
            
            tu = self.parse_file(file_path)
            if tu:
                self.translation_units.append(tu)
                self.analyze_ast(tu.cursor, str(file_path))
        
        # Cross-file analysis
        print("Performing cross-file analysis...")
        
        # Find duplicates
        duplicates = self.duplicate_detector.get_duplicates()
        
        # Find unused functions
        unused_functions = self.call_graph.find_unused_functions()
        
        # Find circular dependencies
        circular_deps = self.call_graph.find_circular_dependencies()
        
        # Architectural analysis
        arch_analysis = self.architectural_analyzer.analyze_dependencies(self.includes)
        
        # Calculate summary statistics
        total_functions = len(self.function_metrics)
        high_complexity_funcs = sum(1 for m in self.function_metrics.values() if m['complexity'] > 10)
        
        return {
            'summary': {
                'total_files': file_count,
                'total_functions': total_functions,
                'total_classes': len(self.class_metrics),
                'high_complexity_functions': high_complexity_funcs,
                'exact_duplicates': len(duplicates['exact']),
                'structural_duplicates': len(duplicates['structural']),
                'semantic_duplicates': len(duplicates['semantic']),
                'unused_functions': len(unused_functions),
                'circular_dependencies': len(circular_deps),
                'high_priority_issues': len(self.issues['high_priority']),
                'medium_priority_issues': len(self.issues['medium_priority']),
                'low_priority_issues': len(self.issues['low_priority'])
            },
            'duplicates': duplicates,
            'unused_functions': unused_functions,
            'circular_dependencies': circular_deps,
            'architectural_analysis': arch_analysis,
            'issues': self.issues,
            'metrics': {
                'functions': self.function_metrics,
                'classes': self.class_metrics
            }
        }
    
    def generate_report(self, results: Dict[str, Any], output_format: str = 'text') -> str:
        """Generate detailed analysis report"""
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        
        if output_format == 'json':
            return json.dumps(results, indent=2, default=str)
        
        # Generate text report
        report = []
        report.append("=" * 80)
        report.append("ENHANCED C++ CODE ANALYSIS REPORT")
        report.append(f"Generated: {timestamp}")
        report.append(f"Directory: {self.root_dir.absolute()}")
        report.append("=" * 80)
        report.append("")
        
        # Executive Summary
        report.append("EXECUTIVE SUMMARY")
        report.append("-" * 40)
        summary = results['summary']
        report.append(f"Files Analyzed: {summary['total_files']}")
        report.append(f"Total Functions: {summary['total_functions']}")
        report.append(f"Total Classes: {summary['total_classes']}")
        report.append("")
        report.append("Key Findings:")
        report.append(f"  • High Priority Issues: {summary['high_priority_issues']}")
        report.append(f"  • Medium Priority Issues: {summary['medium_priority_issues']}")
        report.append(f"  • Low Priority Issues: {summary['low_priority_issues']}")
        report.append("")
        
        # Priority Issues
        if results['issues']['high_priority']:
            report.append("🚨 HIGH PRIORITY ISSUES (Immediate Action Required)")
            report.append("-" * 50)
            for issue in results['issues']['high_priority'][:10]:
                report.append(f"  [{issue['type'].upper()}] {issue.get('function', issue.get('class', 'Unknown'))}")
                report.append(f"    Location: {issue['file']}:{issue['line']}")
                report.append(f"    {issue['recommendation']}")
                report.append("")
        
        if results['issues']['medium_priority']:
            report.append("⚠️  MEDIUM PRIORITY ISSUES")
            report.append("-" * 40)
            for issue in results['issues']['medium_priority'][:10]:
                report.append(f"  [{issue['type'].upper()}] {issue.get('function', issue.get('class', 'Unknown'))}")
                report.append(f"    Location: {issue['file']}:{issue['line']}")
                report.append(f"    {issue['recommendation']}")
                report.append("")
        
        # Duplicate Analysis
        duplicates = results['duplicates']
        if duplicates['exact'] or duplicates['structural'] or duplicates['semantic']:
            report.append("🔍 DUPLICATE CODE ANALYSIS")
            report.append("-" * 40)
            
            if duplicates['exact']:
                report.append(f"\nExact Duplicates: {len(duplicates['exact'])} groups")
                for i, dup in enumerate(duplicates['exact'][:5], 1):
                    report.append(f"  Group {i}: {len(dup)} identical functions")
                    for func in dup[:3]:
                        report.append(f"    - {func['name']} ({func['file']}:{func['line']})")
            
            if duplicates['structural']:
                report.append(f"\nStructural Duplicates: {len(duplicates['structural'])} groups")
                report.append("  (Same structure, different variable names)")
                for i, dup in enumerate(duplicates['structural'][:3], 1):
                    report.append(f"  Group {i}: {len(dup)} similar functions")
            
            if duplicates['semantic']:
                report.append(f"\nSemantic Duplicates: {len(duplicates['semantic'])} groups")
                report.append("  (Similar functionality, different implementation)")
        
        # Unused Functions
        if results['unused_functions']:
            report.append("\n🗑️  POTENTIALLY UNUSED FUNCTIONS")
            report.append("-" * 40)
            report.append(f"Found {len(results['unused_functions'])} potentially unused functions")
            for func in results['unused_functions'][:10]:
                report.append(f"  - {func['name']} ({func['file']}:{func['line']}) [{func['access']}]")
        
        # Circular Dependencies
        if results['circular_dependencies']:
            report.append("\n🔄 CIRCULAR DEPENDENCIES")
            report.append("-" * 40)
            for cycle in results['circular_dependencies'][:5]:
                report.append(f"  Cycle: {' -> '.join(cycle)} -> {cycle[0]}")
        
        # Architectural Analysis
        arch = results.get('architectural_analysis', {})
        if arch.get('layer_violations'):
            report.append("\n🏗️  ARCHITECTURAL VIOLATIONS")
            report.append("-" * 40)
            for violation in arch['layer_violations'][:5]:
                report.append(f"  {violation['violation']}")
        
        # Refactoring Recommendations
        report.append("\n💡 REFACTORING RECOMMENDATIONS")
        report.append("-" * 40)
        report.append("1. Address HIGH PRIORITY issues first (complexity, security)")
        report.append("2. Consolidate exact duplicate functions into shared utilities")
        report.append("3. Review and remove unused functions after verification")
        report.append("4. Break down high-complexity functions (CC > 10)")
        report.append("5. Split large classes following Single Responsibility Principle")
        report.append("6. Resolve circular dependencies to improve architecture")
        
        # Metrics Summary
        report.append("\n📊 CODE QUALITY METRICS")
        report.append("-" * 40)
        
        # Function complexity distribution
        complexities = [m['complexity'] for m in self.function_metrics.values()]
        if complexities:
            report.append("Cyclomatic Complexity Distribution:")
            report.append(f"  • Low (1-5): {sum(1 for c in complexities if c <= 5)} functions")
            report.append(f"  • Medium (6-10): {sum(1 for c in complexities if 6 <= c <= 10)} functions")
            report.append(f"  • High (11-20): {sum(1 for c in complexities if 11 <= c <= 20)} functions")
            report.append(f"  • Very High (>20): {sum(1 for c in complexities if c > 20)} functions")
        
        return "\n".join(report)

def main():
    parser = argparse.ArgumentParser(description='Enhanced C++ Code Analyzer with AST Support')
    parser.add_argument('directory', nargs='?', default='.',
                       help='Directory to analyze (default: current)')
    parser.add_argument('--output', '-o', default='enhanced_cpp_analysis.txt',
                       help='Output file for report')
    parser.add_argument('--format', '-f', choices=['text', 'json', 'html'], default='text',
                       help='Report format')
    parser.add_argument('--compilation-db', '-c',
                       help='Path to compile_commands.json')
    parser.add_argument('--priority', '-p', choices=['all', 'high', 'medium'],
                       default='all', help='Filter issues by priority')
    parser.add_argument('--metrics', '-m', action='store_true',
                       help='Include detailed metrics in report')
    
    args = parser.parse_args()
    
    # Create analyzer
    analyzer = EnhancedCppAnalyzer(args.directory, args.compilation_db)
    
    # Run analysis
    print("\n" + "=" * 60)
    print("ENHANCED C++ CODE ANALYZER")
    print("Using Clang AST for accurate analysis")
    print("=" * 60 + "\n")
    
    results = analyzer.analyze()
    
    # Generate report
    report = analyzer.generate_report(results, args.format)
    
    # Save report
    with open(args.output, 'w', encoding='utf-8') as f:
        f.write(report)
    
    print(f"\n✓ Report saved to: {args.output}")
    
    # Save additional outputs
    if args.metrics:
        metrics_file = args.output.rsplit('.', 1)[0] + '_metrics.json'
        with open(metrics_file, 'w', encoding='utf-8') as f:
            json.dump(results['metrics'], f, indent=2, default=str)
        print(f"✓ Metrics saved to: {metrics_file}")
    
    # Always save JSON data for further processing
    json_file = args.output.rsplit('.', 1)[0] + '_data.json'
    with open(json_file, 'w', encoding='utf-8') as f:
        json.dump(results, f, indent=2, default=str)
    print(f"✓ JSON data saved to: {json_file}")
    
    # Print actionable summary
    print("\n" + "=" * 60)
    print("ANALYSIS COMPLETE - ACTION ITEMS")
    print("=" * 60)
    
    summary = results['summary']
    
    # Priority breakdown
    if summary['high_priority_issues'] > 0:
        print(f"\n🔴 HIGH PRIORITY: {summary['high_priority_issues']} issues")
        print("   Fix immediately - complexity, security, or critical bugs")
    
    if summary['medium_priority_issues'] > 0:
        print(f"\n🟡 MEDIUM PRIORITY: {summary['medium_priority_issues']} issues")
        print("   Address in next refactoring sprint")
    
    if summary['low_priority_issues'] > 0:
        print(f"\n🟢 LOW PRIORITY: {summary['low_priority_issues']} issues")
        print("   Consider during routine maintenance")
    
    # Quick wins
    print("\n💡 QUICK WINS:")
    if summary['exact_duplicates'] > 0:
        print(f"  • Consolidate {summary['exact_duplicates']} exact duplicate groups")
    if summary['unused_functions'] > 0:
        print(f"  • Remove {summary['unused_functions']} unused functions")
    if summary['high_complexity_functions'] > 0:
        print(f"  • Refactor {summary['high_complexity_functions']} high-complexity functions")
    
    print("\n✅ Next Steps:")
    print("  1. Review high priority issues in the report")
    print("  2. Run 'git diff' after fixes to track improvements")
    print("  3. Set up CI/CD integration for continuous monitoring")
    print("  4. Consider using compile_commands.json for better accuracy")

if __name__ == "__main__":
    main()
