#!/usr/bin/env python3
"""
Sentio File Organization Checker
================================

This script verifies that all output files are properly organized in the data/ folder
with appropriate subfolders (audit, trades, signals) and identifies any misplaced files.
"""

import os
import sys
from pathlib import Path
from typing import List, Dict, Set

class FileOrganizationChecker:
    def __init__(self, project_root: str = None):
        self.project_root = Path(project_root) if project_root else Path.cwd()
        self.data_dir = self.project_root / "data"
        
        # Expected data subdirectories
        self.expected_subdirs = {
            'audit': [],  # Reserved for future use, no specific file types expected
            'signals': ['*.jsonl', '*_signals_*'],
            'trades': ['*_trades.jsonl', '*_trades_*'],
            'equities': ['*.csv', '*.bin'],
            'future_qqq': ['*.csv', '*.json', '*.bin'],
            'mars_cache': ['*.json']
        }
        
        # Data file extensions that should be in data/
        self.data_extensions = {'.jsonl', '.csv', '.bin', '.json'}
        
    def check_data_directory_structure(self) -> Dict[str, List[str]]:
        """Check if all expected data subdirectories exist"""
        issues = []
        
        if not self.data_dir.exists():
            issues.append("data/ directory does not exist")
            return {"critical": issues}
        
        missing_dirs = []
        for subdir in self.expected_subdirs.keys():
            subdir_path = self.data_dir / subdir
            if not subdir_path.exists():
                missing_dirs.append(f"data/{subdir}/ directory missing")
        
        return {"missing_directories": missing_dirs, "issues": issues}
    
    def find_misplaced_data_files(self) -> Dict[str, List[str]]:
        """Find data files that are not in the data/ directory"""
        misplaced_files = []
        
        # Check root directory for actual data files (not source code)
        for file_path in self.project_root.iterdir():
            if file_path.is_file():
                # Only check for actual data output files, not source code
                if (file_path.suffix in {'.jsonl'} or
                    any(pattern in file_path.name for pattern in ['_signals_', '_trades_'])):
                    misplaced_files.append(str(file_path.relative_to(self.project_root)))
        
        # Check other directories (excluding data/, build/, .git/, src/, include/, tools/)
        exclude_dirs = {'data', 'build', '.git', '.mypy_cache', '__pycache__', 'src', 'include', 'tools', 'docs', 'tests', 'megadocs', 'logs'}
        
        for dir_path in self.project_root.iterdir():
            if dir_path.is_dir() and dir_path.name not in exclude_dirs:
                for file_path in dir_path.rglob('*'):
                    if file_path.is_file():
                        # Only check for actual data output files
                        if (file_path.suffix in {'.jsonl'} or
                            any(pattern in file_path.name for pattern in ['_signals_', '_trades_'])):
                            misplaced_files.append(str(file_path.relative_to(self.project_root)))
        
        return {"misplaced_files": misplaced_files}
    
    def check_data_subdirectory_contents(self) -> Dict[str, List[str]]:
        """Check if files are in the correct data subdirectories"""
        misorganized = []
        
        if not self.data_dir.exists():
            return {"misorganized_files": []}
        
        for subdir_name, patterns in self.expected_subdirs.items():
            subdir_path = self.data_dir / subdir_name
            if not subdir_path.exists():
                continue
                
            # Check files in this subdirectory
            for file_path in subdir_path.rglob('*'):
                if file_path.is_file():
                    file_name = file_path.name
                    
                    # Check if file belongs in this subdirectory
                    belongs_here = False
                    for pattern in patterns:
                        if pattern.startswith('*') and pattern.endswith('*'):
                            # Contains pattern
                            if pattern[1:-1] in file_name:
                                belongs_here = True
                                break
                        elif pattern.startswith('*'):
                            # Ends with pattern
                            if file_name.endswith(pattern[1:]):
                                belongs_here = True
                                break
                        elif pattern.endswith('*'):
                            # Starts with pattern
                            if file_name.startswith(pattern[:-1]):
                                belongs_here = True
                                break
                        else:
                            # Exact match
                            if file_name == pattern:
                                belongs_here = True
                                break
                    
                    if not belongs_here:
                        # Check if it should be in a different subdirectory
                        correct_subdir = self.find_correct_subdirectory(file_name)
                        if correct_subdir and correct_subdir != subdir_name:
                            misorganized.append(f"{file_path.relative_to(self.project_root)} should be in data/{correct_subdir}/")
        
        return {"misorganized_files": misorganized}
    
    def find_correct_subdirectory(self, filename: str) -> str:
        """Find the correct subdirectory for a given filename"""
        for subdir_name, patterns in self.expected_subdirs.items():
            for pattern in patterns:
                if pattern.startswith('*') and pattern.endswith('*'):
                    if pattern[1:-1] in filename:
                        return subdir_name
                elif pattern.startswith('*'):
                    if filename.endswith(pattern[1:]):
                        return subdir_name
                elif pattern.endswith('*'):
                    if filename.startswith(pattern[:-1]):
                        return subdir_name
                else:
                    if filename == pattern:
                        return subdir_name
        return None
    
    def check_source_code_paths(self) -> Dict[str, List[str]]:
        """Check source code for hardcoded paths that might be problematic"""
        problematic_paths = []
        
        src_dir = self.project_root / "src"
        if not src_dir.exists():
            return {"problematic_paths": []}
        
        for file_path in src_dir.rglob('*.cpp'):
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()
                    lines = content.split('\n')
                    
                    for line_num, line in enumerate(lines, 1):
                        # Check for absolute paths
                        if '/' in line and ('/Users/' in line or '/home/' in line or 'C:\\' in line):
                            if not line.strip().startswith('//'):  # Skip comments
                                problematic_paths.append(f"{file_path.relative_to(self.project_root)}:{line_num} - Absolute path: {line.strip()}")
                        
                        # Check for output file paths that don't use data/ directory
                        if any(pattern in line for pattern in ['ofstream', 'std::ofstream']) and any(ext in line for ext in ['.jsonl']):
                            if 'data/' not in line and not line.strip().startswith('//'):
                                problematic_paths.append(f"{file_path.relative_to(self.project_root)}:{line_num} - Output not to data/: {line.strip()}")
            
            except Exception as e:
                problematic_paths.append(f"Error reading {file_path}: {e}")
        
        return {"problematic_paths": problematic_paths}
    
    def generate_report(self) -> Dict[str, any]:
        """Generate comprehensive file organization report"""
        report = {
            "project_root": str(self.project_root),
            "data_directory": str(self.data_dir),
            "timestamp": str(Path(__file__).stat().st_mtime)
        }
        
        # Check directory structure
        dir_check = self.check_data_directory_structure()
        report.update(dir_check)
        
        # Find misplaced files
        misplaced_check = self.find_misplaced_data_files()
        report.update(misplaced_check)
        
        # Check subdirectory organization
        subdir_check = self.check_data_subdirectory_contents()
        report.update(subdir_check)
        
        # Check source code paths
        source_check = self.check_source_code_paths()
        report.update(source_check)
        
        # Calculate overall status
        total_issues = (
            len(report.get("issues", [])) +
            len(report.get("missing_directories", [])) +
            len(report.get("misplaced_files", [])) +
            len(report.get("misorganized_files", [])) +
            len(report.get("problematic_paths", []))
        )
        
        report["overall_status"] = "PASS" if total_issues == 0 else "ISSUES_FOUND"
        report["total_issues"] = total_issues
        
        return report
    
    def print_report(self, report: Dict[str, any]):
        """Print formatted report"""
        print("=" * 70)
        print("SENTIO FILE ORGANIZATION CHECK")
        print("=" * 70)
        print(f"Project Root: {report['project_root']}")
        print(f"Data Directory: {report['data_directory']}")
        print()
        
        # Overall status
        status = report["overall_status"]
        if status == "PASS":
            print("✅ OVERALL STATUS: PASS - All files properly organized")
        else:
            print(f"⚠️  OVERALL STATUS: {report['total_issues']} issues found")
        print()
        
        # Directory structure
        if report.get("missing_directories"):
            print("📁 MISSING DIRECTORIES:")
            for missing in report["missing_directories"]:
                print(f"   ❌ {missing}")
            print()
        
        # Misplaced files
        if report.get("misplaced_files"):
            print("📄 MISPLACED FILES (should be in data/):")
            for misplaced in report["misplaced_files"]:
                print(f"   ❌ {misplaced}")
            print()
        
        # Misorganized files
        if report.get("misorganized_files"):
            print("🗂️  MISORGANIZED FILES:")
            for misorg in report["misorganized_files"]:
                print(f"   ⚠️  {misorg}")
            print()
        
        # Problematic paths in source code
        if report.get("problematic_paths"):
            print("🔍 PROBLEMATIC PATHS IN SOURCE CODE:")
            for prob in report["problematic_paths"]:
                print(f"   ⚠️  {prob}")
            print()
        
        # Summary
        if status == "PASS":
            print("🎉 All files are properly organized in the data/ folder structure!")
        else:
            print("📋 RECOMMENDATIONS:")
            print("   • Move misplaced files to appropriate data/ subdirectories")
            print("   • Create missing directories as needed")
            print("   • Update source code to use proper data/ paths")
            print("   • Ensure all output files go to data/ subdirectories")


def main():
    checker = FileOrganizationChecker()
    report = checker.generate_report()
    checker.print_report(report)
    
    # Exit with error code if issues found
    sys.exit(0 if report["overall_status"] == "PASS" else 1)


if __name__ == "__main__":
    main()
