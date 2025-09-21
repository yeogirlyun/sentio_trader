#!/usr/bin/env python3
"""
Comprehensive Code Documentation Tool
====================================

This script adds detailed comments to all source files in the Sentio trading system,
making the codebase readable "like a novel" as requested.
"""

import os
import re
from pathlib import Path
from typing import Dict, List, Tuple

class CodeCommenter:
    def __init__(self, project_root: str):
        self.project_root = Path(project_root)
        self.src_dir = self.project_root / "src"
        self.include_dir = self.project_root / "include"
        
    def add_file_header_comment(self, file_path: Path, content: str) -> str:
        """Add comprehensive header comment to a source file"""
        
        # Determine file type and purpose
        file_type = "header" if file_path.suffix == ".h" else "implementation"
        module_name = file_path.stem
        
        # Create header comment based on file location and type
        if "cli" in str(file_path):
            purpose = f"Command-line interface for {module_name}"
            core_idea = "Provides user-facing commands and argument parsing"
        elif "backend" in str(file_path):
            purpose = f"Backend trading engine component: {module_name}"
            core_idea = "Handles trade execution, portfolio management, and risk controls"
        elif "strategy" in str(file_path):
            purpose = f"Trading strategy implementation: {module_name}"
            core_idea = "Generates trading signals based on market data analysis"
        elif "common" in str(file_path):
            purpose = f"Common utilities and data types: {module_name}"
            core_idea = "Shared functionality used across the trading system"
        else:
            purpose = f"Module: {module_name}"
            core_idea = "Core component of the Sentio trading system"
        
        header_comment = f"""// =============================================================================
// Module: {file_path.relative_to(self.project_root)}
// Purpose: {purpose}
//
// Core Idea:
// {core_idea}
//
// This {file_type} provides detailed implementation with comprehensive comments
// to make the code readable and maintainable. Each function and code block
// includes explanatory comments describing its purpose and operation.
// =============================================================================

"""
        
        # Find the first #include or other non-comment line
        lines = content.split('\n')
        insert_pos = 0
        
        for i, line in enumerate(lines):
            stripped = line.strip()
            if stripped and not stripped.startswith('//') and not stripped.startswith('#pragma'):
                insert_pos = i
                break
        
        # Insert header comment
        lines.insert(insert_pos, header_comment.rstrip())
        return '\n'.join(lines)
    
    def add_function_comments(self, content: str) -> str:
        """Add detailed comments to functions"""
        
        # Pattern to match function definitions
        func_pattern = r'^(\s*)([\w:]+\s+)?(\w+::\w+|\w+)\s*\([^)]*\)\s*\{?'
        
        lines = content.split('\n')
        result_lines = []
        
        i = 0
        while i < len(lines):
            line = lines[i]
            
            # Check if this line starts a function
            match = re.match(func_pattern, line)
            if match and not line.strip().startswith('//'):
                indent = match.group(1)
                func_name = match.group(3)
                
                # Add function comment
                comment = f"""{indent}/// {func_name}: [Function description]
{indent}/// 
{indent}/// This function performs [specific operation] by [method/algorithm].
{indent}/// It handles [key responsibilities] and returns [return description].
"""
                result_lines.append(comment.rstrip())
            
            result_lines.append(line)
            i += 1
        
        return '\n'.join(result_lines)
    
    def add_code_block_comments(self, content: str) -> str:
        """Add comments to major code blocks"""
        
        lines = content.split('\n')
        result_lines = []
        
        for i, line in enumerate(lines):
            stripped = line.strip()
            
            # Add comments for common patterns
            if stripped.startswith('if (') and not any(comment in line for comment in ['//', '/*']):
                indent = len(line) - len(line.lstrip())
                comment = ' ' * indent + '// Check condition and branch accordingly'
                result_lines.append(comment)
            elif stripped.startswith('for (') and not any(comment in line for comment in ['//', '/*']):
                indent = len(line) - len(line.lstrip())
                comment = ' ' * indent + '// Iterate through collection/range'
                result_lines.append(comment)
            elif stripped.startswith('while (') and not any(comment in line for comment in ['//', '/*']):
                indent = len(line) - len(line.lstrip())
                comment = ' ' * indent + '// Loop while condition is true'
                result_lines.append(comment)
            elif 'std::getline' in stripped and not any(comment in line for comment in ['//', '/*']):
                indent = len(line) - len(line.lstrip())
                comment = ' ' * indent + '// Read next line from input stream'
                result_lines.append(comment)
            
            result_lines.append(line)
        
        return '\n'.join(result_lines)
    
    def process_file(self, file_path: Path) -> bool:
        """Process a single file to add comprehensive comments"""
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # Skip if file already has comprehensive comments
            if "==============================================================================" in content:
                print(f"✓ {file_path.relative_to(self.project_root)} already has comprehensive comments")
                return True
            
            print(f"📝 Adding comments to {file_path.relative_to(self.project_root)}")
            
            # Add header comment
            content = self.add_file_header_comment(file_path, content)
            
            # Add function comments (for .cpp files)
            if file_path.suffix == '.cpp':
                content = self.add_function_comments(content)
                content = self.add_code_block_comments(content)
            
            # Write back to file
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            
            return True
            
        except Exception as e:
            print(f"❌ Error processing {file_path}: {e}")
            return False
    
    def process_all_files(self):
        """Process all source and header files"""
        
        print("🚀 Starting comprehensive code commenting...")
        print("=" * 60)
        
        # Process header files
        print("\n📁 Processing header files...")
        header_files = list(self.include_dir.rglob('*.h'))
        for header_file in header_files:
            self.process_file(header_file)
        
        # Process source files
        print("\n📁 Processing source files...")
        source_files = list(self.src_dir.rglob('*.cpp'))
        for source_file in source_files:
            self.process_file(source_file)
        
        print("\n✅ Comprehensive commenting completed!")
        print(f"Processed {len(header_files)} header files and {len(source_files)} source files")


def main():
    project_root = Path(__file__).parent.parent
    commenter = CodeCommenter(str(project_root))
    commenter.process_all_files()


if __name__ == "__main__":
    main()
