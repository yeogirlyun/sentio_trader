#!/usr/bin/env python3
"""
Create mega document from source files with automatic splitting for large documents.
Automatically splits documents into multiple parts when they exceed 300KB.
"""

import os
import argparse
import datetime
from pathlib import Path

# Maximum size per mega document file (300KB)
MAX_FILE_SIZE_KB = 300
MAX_FILE_SIZE_BYTES = MAX_FILE_SIZE_KB * 1024

def get_file_size_kb(file_path):
    """Get file size in KB."""
    return os.path.getsize(file_path) / 1024

def create_document_header(title, description, part_num=None, total_parts=None):
    """Create the header for a mega document."""
    header = f"# {title}\n\n"
    if part_num and total_parts:
        header += f"**Part {part_num} of {total_parts}**\n\n"
    header += f"**Generated**: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
    header += f"**Source Directory**: {os.getcwd()}\n"
    header += f"**Description**: {description}\n\n"
    if part_num and total_parts:
        header += f"**Part {part_num} Files**: See file count below\n\n"
    else:
        header += f"**Total Files**: See file count below\n\n"
    header += "---\n\n"
    return header

def create_table_of_contents(all_files, start_idx=0, end_idx=None):
    """Create table of contents for a range of files."""
    if end_idx is None:
        end_idx = len(all_files)
    
    toc = "## 📋 **TABLE OF CONTENTS**\n\n"
    for i in range(start_idx, end_idx):
        file_path = all_files[i]
        toc += f"{i+1}. [{file_path}](#file-{i+1})\n"
    toc += "\n---\n\n"
    return toc

def write_file_content(f, file_path, file_num, total_files):
    """Write a single file's content to the mega document."""
    try:
        with open(file_path, 'r', encoding='utf-8') as file_f:
            content = file_f.read()
        
        f.write(f"## 📄 **FILE {file_num} of {total_files}**: {file_path}\n\n")
        f.write("**File Information**:\n")
        f.write(f"- **Path**: `{file_path}`\n\n")
        f.write(f"- **Size**: {len(content.splitlines())} lines\n")
        f.write(f"- **Modified**: {datetime.datetime.fromtimestamp(os.path.getmtime(file_path)).strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.write(f"- **Type**: {Path(file_path).suffix}\n\n")
        f.write("```text\n")
        f.write(content)
        f.write("\n```\n\n")
        
        print(f"📄 Processing file {file_num}/{total_files}: {file_path}")
        return True
        
    except Exception as e:
        print(f"❌ Error processing {file_path}: {e}")
        f.write(f"## 📄 **FILE {file_num} of {total_files}**: {file_path}\n\n")
        f.write(f"**Error**: Could not read file - {e}\n\n")
        return False

def create_mega_document_part(output_path, all_files, start_idx, end_idx, title, description, 
                            part_num=None, total_parts=None, include_bug_report=False, bug_report_file=None):
    """Create a single part of the mega document."""
    
    with open(output_path, 'w', encoding='utf-8') as f:
        # Write header
        f.write(create_document_header(title, description, part_num, total_parts))
        
        # Include bug report if requested (only in first part)
        if include_bug_report and bug_report_file and os.path.exists(bug_report_file) and part_num == 1:
            f.write("## 🐛 **BUG REPORT**\n\n")
            with open(bug_report_file, 'r', encoding='utf-8') as bug_f:
                f.write(bug_f.read())
            f.write("\n\n---\n\n")
        
        # Table of contents for this part
        f.write(create_table_of_contents(all_files, start_idx, end_idx))
        
        # File contents
        files_in_part = 0
        for i in range(start_idx, end_idx):
            file_path = all_files[i]
            if write_file_content(f, file_path, i+1, len(all_files)):
                files_in_part += 1
    
    return files_in_part

def create_mega_document(directories, title, description, output, include_bug_report=False, bug_report_file=None):
    """Create a mega document from source files with automatic splitting."""
    
    print(f"🔧 Creating mega document: {output}")
    print(f"📁 Source directory: {os.getcwd()}")
    print(f"📁 Output file: {output}")
    print(f"📊 Max file size: {MAX_FILE_SIZE_KB}KB per part")
    
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
    
    # Calculate total estimated size
    total_size = sum(os.path.getsize(f) for f in all_files if os.path.exists(f))
    estimated_parts = max(1, int(total_size / MAX_FILE_SIZE_BYTES) + 1)
    
    print(f"📊 Estimated total size: {total_size / 1024:.1f} KB")
    print(f"📊 Estimated parts needed: {estimated_parts}")
    
    # Create parts
    created_files = []
    current_part = 1
    start_idx = 0
    
    while start_idx < len(all_files):
        # Determine end index for this part
        end_idx = min(start_idx + len(all_files) // estimated_parts + 1, len(all_files))
        
        # Create output filename for this part
        if estimated_parts > 1:
            base_name = Path(output).stem
            extension = Path(output).suffix
            output_path = f"{base_name}_part{current_part}{extension}"
        else:
            output_path = output
        
        print(f"\n📝 Creating part {current_part}: {output_path}")
        
        # Create this part
        files_in_part = create_mega_document_part(
            output_path, all_files, start_idx, end_idx, title, description,
            current_part, estimated_parts, include_bug_report, bug_report_file
        )
        
        # Check actual file size
        actual_size_kb = get_file_size_kb(output_path)
        print(f"📊 Part {current_part} size: {actual_size_kb:.1f} KB ({files_in_part} files)")
        
        created_files.append(output_path)
        
        # Move to next part
        start_idx = end_idx
        current_part += 1
        
        # If we're creating multiple parts and this part is still too large,
        # we need to split more aggressively
        if actual_size_kb > MAX_FILE_SIZE_KB and estimated_parts == 1:
            print(f"⚠️  Single file exceeds {MAX_FILE_SIZE_KB}KB, splitting into multiple parts...")
            estimated_parts = max(2, int(actual_size_kb / MAX_FILE_SIZE_KB) + 1)
            # Restart with better estimation
            start_idx = 0
            current_part = 1
            created_files = []
            continue
    
    # Summary
    total_files_processed = sum(len(created_files) for _ in created_files)
    print(f"\n✅ Mega document creation complete!")
    print(f"📊 Parts created: {len(created_files)}")
    print(f"📊 Total files processed: {len(all_files)}")
    
    for i, file_path in enumerate(created_files, 1):
        size_kb = get_file_size_kb(file_path)
        print(f"📄 Part {i}: {file_path} ({size_kb:.1f} KB)")
    
    print(f"\n🎯 Success! Mega document(s) created:")
    for file_path in created_files:
        print(f"📁 {os.path.abspath(file_path)}")
    
    return created_files

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
