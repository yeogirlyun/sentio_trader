#!/usr/bin/env python3
"""
Static analysis script to identify potentially unused code in Sentio C++
"""

import os
import re
import glob
from collections import defaultdict, Counter

def find_cpp_files(directory):
    """Find all C++ source and header files"""
    cpp_files = []
    for root, dirs, files in os.walk(directory):
        # Skip build and third_party directories
        dirs[:] = [d for d in dirs if d not in ['build', 'third_party', '.git', 'MarS', 'sota_portfolio_python']]
        
        for file in files:
            if file.endswith(('.cpp', '.hpp', '.h')):
                cpp_files.append(os.path.join(root, file))
    return cpp_files

def extract_functions_and_classes(file_path):
    """Extract function and class definitions from a C++ file"""
    functions = []
    classes = []
    
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            
        # Find class definitions
        class_pattern = r'class\s+(\w+)'
        classes.extend(re.findall(class_pattern, content))
        
        # Find function definitions (simplified pattern)
        # This catches most function definitions but may have false positives
        func_pattern = r'(?:^|\n)\s*(?:(?:inline|static|virtual|explicit|const|constexpr)\s+)*(?:\w+(?:\s*\*|\s*&)?(?:\s*::\s*\w+)?)\s+(\w+)\s*\([^)]*\)\s*(?:const)?\s*(?:override)?\s*(?:noexcept)?\s*[{;]'
        functions.extend(re.findall(func_pattern, content, re.MULTILINE))
        
        # Find method definitions within classes
        method_pattern = r'(?:public|private|protected):\s*(?:[^}]*?)(?:(?:inline|static|virtual|explicit|const|constexpr)\s+)*(?:\w+(?:\s*\*|\s*&)?(?:\s*::\s*\w+)?)\s+(\w+)\s*\([^)]*\)\s*(?:const)?\s*(?:override)?\s*(?:noexcept)?\s*[{;]'
        functions.extend(re.findall(method_pattern, content, re.MULTILINE | re.DOTALL))
        
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
    
    return functions, classes

def find_usage(symbol, files):
    """Find usage of a symbol across all files"""
    usage_count = 0
    usage_files = []
    
    for file_path in files:
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                
            # Count occurrences (simple text search)
            count = len(re.findall(r'\b' + re.escape(symbol) + r'\b', content))
            if count > 0:
                usage_count += count
                usage_files.append((file_path, count))
                
        except Exception as e:
            continue
    
    return usage_count, usage_files

def analyze_unused_code():
    """Main analysis function"""
    print("🔍 SENTIO C++ UNUSED CODE ANALYSIS")
    print("==================================")
    
    # Find all C++ files
    cpp_files = find_cpp_files('.')
    print(f"📁 Found {len(cpp_files)} C++ files")
    
    # Extract all functions and classes
    all_functions = defaultdict(list)
    all_classes = defaultdict(list)
    
    print("📊 Extracting symbols...")
    for file_path in cpp_files:
        functions, classes = extract_functions_and_classes(file_path)
        
        for func in functions:
            all_functions[func].append(file_path)
        
        for cls in classes:
            all_classes[cls].append(file_path)
    
    print(f"🔧 Found {len(all_functions)} unique function names")
    print(f"🏗️  Found {len(all_classes)} unique class names")
    
    # Analyze usage
    print("\n🔍 Analyzing function usage...")
    unused_functions = []
    low_usage_functions = []
    
    for func_name, definition_files in all_functions.items():
        # Skip common/generic names that are likely to have false positives
        if func_name in ['main', 'get', 'set', 'init', 'run', 'test', 'size', 'empty', 'clear', 'begin', 'end']:
            continue
            
        usage_count, usage_files = find_usage(func_name, cpp_files)
        
        # If usage count is very low (1-2), it might be unused or only defined
        if usage_count <= 2:
            unused_functions.append((func_name, usage_count, definition_files))
        elif usage_count <= 5:
            low_usage_functions.append((func_name, usage_count, definition_files))
    
    # Analyze class usage
    print("🔍 Analyzing class usage...")
    unused_classes = []
    low_usage_classes = []
    
    for class_name, definition_files in all_classes.items():
        usage_count, usage_files = find_usage(class_name, cpp_files)
        
        if usage_count <= 2:
            unused_classes.append((class_name, usage_count, definition_files))
        elif usage_count <= 5:
            low_usage_classes.append((class_name, usage_count, definition_files))
    
    # Report results
    print(f"\n📋 ANALYSIS RESULTS")
    print(f"==================")
    
    print(f"\n❌ POTENTIALLY UNUSED FUNCTIONS ({len(unused_functions)}):")
    print("%-30s %-8s %s" % ("Function", "Usage", "Defined In"))
    print("%-30s %-8s %s" % ("-" * 30, "-" * 8, "-" * 20))
    for func_name, usage_count, def_files in sorted(unused_functions)[:20]:
        file_names = [os.path.basename(f) for f in def_files[:2]]
        print("%-30s %-8d %s" % (func_name, usage_count, ", ".join(file_names)))
    
    if len(unused_functions) > 20:
        print(f"... and {len(unused_functions) - 20} more")
    
    print(f"\n⚠️  LOW USAGE FUNCTIONS ({len(low_usage_functions)}):")
    print("%-30s %-8s %s" % ("Function", "Usage", "Defined In"))
    print("%-30s %-8s %s" % ("-" * 30, "-" * 8, "-" * 20))
    for func_name, usage_count, def_files in sorted(low_usage_functions)[:15]:
        file_names = [os.path.basename(f) for f in def_files[:2]]
        print("%-30s %-8d %s" % (func_name, usage_count, ", ".join(file_names)))
    
    print(f"\n❌ POTENTIALLY UNUSED CLASSES ({len(unused_classes)}):")
    print("%-30s %-8s %s" % ("Class", "Usage", "Defined In"))
    print("%-30s %-8s %s" % ("-" * 30, "-" * 8, "-" * 20))
    for class_name, usage_count, def_files in sorted(unused_classes)[:15]:
        file_names = [os.path.basename(f) for f in def_files[:2]]
        print("%-30s %-8d %s" % (class_name, usage_count, ", ".join(file_names)))
    
    # Summary
    print(f"\n📊 SUMMARY:")
    print(f"   • Total functions analyzed: {len(all_functions)}")
    print(f"   • Potentially unused functions: {len(unused_functions)}")
    print(f"   • Low usage functions: {len(low_usage_functions)}")
    print(f"   • Total classes analyzed: {len(all_classes)}")
    print(f"   • Potentially unused classes: {len(unused_classes)}")
    
    return unused_functions, low_usage_functions, unused_classes

if __name__ == "__main__":
    analyze_unused_code()
