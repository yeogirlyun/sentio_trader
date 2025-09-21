#!/usr/bin/env python3
"""
Create mega document from source files.
"""

import os
import argparse
import datetime
from pathlib import Path

def create_mega_document(directories, title, description, output, include_bug_report=False, bug_report_file=None):
    """Create a mega document from source files."""
    
    print(f"🔧 Creating mega document: {output}")
    print(f"📁 Source directory: {os.getcwd()}")
    print(f"📁 Output file: {output}")
    
    # Collect all files
    all_files = []
    for directory in directories:
        if os.path.exists(directory):
            for root, dirs, files in os.walk(directory):
                for file in files:
                    if file.endswith(('.hpp', '.cpp', '.h', '.c', '.py', '.md', '.txt', '.cmake', 'CMakeLists.txt')):
                        file_path = os.path.join(root, file)
                        all_files.append(file_path)
    
    print(f"📁 Files to include: {len(all_files)}")
    
    # Sort files for consistent ordering
    all_files.sort()
    
    # Create mega document
    with open(output, 'w', encoding='utf-8') as f:
        f.write(f"# {title}\n\n")
        f.write(f"**Generated**: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"**Source Directory**: {os.getcwd()}\n")
        f.write(f"**Description**: {description}\n\n")
        f.write(f"**Total Files**: {len(all_files)}\n\n")
        f.write("---\n\n")
        
        # Include bug report if requested
        if include_bug_report and bug_report_file and os.path.exists(bug_report_file):
            f.write("## 🐛 **BUG REPORT**\n\n")
            with open(bug_report_file, 'r', encoding='utf-8') as bug_f:
                f.write(bug_f.read())
            f.write("\n\n---\n\n")
        
        # Table of contents
        f.write("## 📋 **TABLE OF CONTENTS**\n\n")
        for i, file_path in enumerate(all_files, 1):
            f.write(f"{i}. [{file_path}](#file-{i})\n")
        f.write("\n---\n\n")
        
        # File contents
        for i, file_path in enumerate(all_files, 1):
            try:
                with open(file_path, 'r', encoding='utf-8') as file_f:
                    content = file_f.read()
                
                f.write(f"## 📄 **FILE {i} of {len(all_files)}**: {file_path}\n\n")
                f.write("**File Information**:\n")
                f.write(f"- **Path**: `{file_path}`\n\n")
                f.write(f"- **Size**: {len(content.splitlines())} lines\n")
                f.write(f"- **Modified**: {datetime.datetime.fromtimestamp(os.path.getmtime(file_path)).strftime('%Y-%m-%d %H:%M:%S')}\n\n")
                f.write(f"- **Type**: {Path(file_path).suffix}\n\n")
                f.write("```text\n")
                f.write(content)
                f.write("\n```\n\n")
                
                print(f"📄 Processing file {i}/{len(all_files)}: {file_path}")
                
            except Exception as e:
                print(f"❌ Error processing {file_path}: {e}")
                f.write(f"## 📄 **FILE {i} of {len(all_files)}**: {file_path}\n\n")
                f.write(f"**Error**: Could not read file - {e}\n\n")
    
    print(f"✅ Mega document created: {output}")
    print(f"📊 Output size: {os.path.getsize(output) / 1024:.1f} KB")
    print(f"📊 Files processed: {len(all_files)}/{len(all_files)}")
    print(f"📊 Content size: {sum(os.path.getsize(f) for f in all_files if os.path.exists(f)) / 1024:.1f} KB")
    print(f"\n🎯 Success! Mega document created:")
    print(f"{output}")
    print(f"\n📁 Location: {os.path.abspath(output)}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Create mega document from source files")
    parser.add_argument("--directories", "-d", nargs="+", required=True, help="Directories to include")
    parser.add_argument("--title", "-t", required=True, help="Document title")
    parser.add_argument("--description", "-desc", required=True, help="Document description")
    parser.add_argument("--output", "-o", required=True, help="Output file path")
    parser.add_argument("--include-bug-report", action="store_true", help="Include bug report")
    parser.add_argument("--bug-report-file", help="Bug report file path")
    
    args = parser.parse_args()
    
    create_mega_document(
        args.directories,
        args.title,
        args.description,
        args.output,
        args.include_bug_report,
        args.bug_report_file
    )
