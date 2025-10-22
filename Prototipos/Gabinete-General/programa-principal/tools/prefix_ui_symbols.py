#!/usr/bin/env python3
"""
UI Symbol Prefixer for Module Isolation

This script prefixes all UI symbols in a module's UI files with the module name
to prevent symbol conflicts when multiple modules are linked together.

Features:
- Renames UI files with the specified prefix
- Updates include guards in .h files
- Updates #include statements to reference renamed files
- Prefixes all ui_ symbols in the code

Usage:
    python3 prefix_ui_symbols.py <module_path> <prefix>
    
Example:
    python3 prefix_ui_symbols.py components/buck_module/ui buck
    python3 prefix_ui_symbols.py components/dimmer_module/ui dimmer
"""

import os
import sys
import re
from pathlib import Path

def rename_files(ui_dir, prefix):
    """
    Rename all .c and .h files in the UI directory with the given prefix.
    Returns a mapping of old filenames to new filenames.
    """
    rename_map = {}
    ui_path = Path(ui_dir)
    
    files_to_process = [f for f in os.listdir(ui_dir) if f.endswith(('.c', '.h'))]
    
    if not files_to_process:
        return rename_map
    
    print("--- 📂 Step 1: Renaming files ---")
    for filename in files_to_process:
        # Skip if already prefixed
        if filename.startswith(f"{prefix}_"):
            print(f"⚠ Skipping '{filename}' (already prefixed)")
            continue
            
        new_filename = f"{prefix}_{filename}"
        old_filepath = ui_path / filename
        new_filepath = ui_path / new_filename
        
        try:
            os.rename(old_filepath, new_filepath)
            rename_map[filename] = new_filename
            print(f"✅ '{filename}' ➔ '{new_filename}'")
        except OSError as e:
            print(f"❌ Error renaming '{filename}': {e}")
    
    return rename_map

def update_include_guards(filepath, new_filename):
    """Update include guards in .h files based on the new filename"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    
    # Create new guard name from filename
    # Example: "buck_ui_helpers.h" -> "BUCK_UI_HELPERS_H"
    base_name = os.path.splitext(new_filename)[0]
    new_guard = base_name.upper() + '_H'
    
    # Extract the base pattern from original filename to match the old guard
    # For files like "inverter_ui_helpers.h", look for "UI_HELPERS_H" or "INVERTER_UI_HELPERS_H"
    # For files like "inverter_ui.h", look for "UI_H" or "INVERTER_UI_H"
    
    # Look for the include guard pattern specifically:
    # Line 1-3: // comments or blank
    # Then: #ifndef SOMETHING_H
    # Then: #define SOMETHING_H
    
    lines = content.split('\n')
    ifndef_found = False
    ifndef_line_idx = -1
    define_line_idx = -1
    
    # Find the #ifndef and #define pair that form the include guard
    for i in range(min(15, len(lines))):  # Check first 15 lines for guard
        line_stripped = lines[i].strip()
        
        # Find #ifndef XXX_H (with nothing after it)
        if not ifndef_found and line_stripped.startswith('#ifndef') and line_stripped.endswith('_H'):
            match = re.match(r'#ifndef\s+(\w+_H)\s*$', line_stripped)
            if match:
                ifndef_found = True
                ifndef_line_idx = i
                continue
        
        # Find the matching #define XXX_H (must come right after #ifndef, possibly with blank lines)
        if ifndef_found and define_line_idx == -1:
            if line_stripped.startswith('#define') and line_stripped.endswith('_H'):
                match = re.match(r'#define\s+(\w+_H)\s*$', line_stripped)
                if match and (i - ifndef_line_idx) <= 2:  # Allow 1 blank line between
                    define_line_idx = i
                    break
    
    # Update the guard lines if found
    if ifndef_line_idx != -1:
        lines[ifndef_line_idx] = re.sub(r'(#ifndef\s+)\w+_H\s*$', rf'\1{new_guard}', lines[ifndef_line_idx])
    if define_line_idx != -1:
        lines[define_line_idx] = re.sub(r'(#define\s+)\w+_H\s*$', rf'\1{new_guard}', lines[define_line_idx])
    
    content = '\n'.join(lines)
    
    return content if content != original_content else None

def update_includes(content, rename_map):
    """Update all #include statements to reference renamed files"""
    for old_header, new_header in rename_map.items():
        if old_header.endswith('.h'):
            old_include = f'#include "{old_header}"'
            new_include = f'#include "{new_header}"'
            content = content.replace(old_include, new_include)
    return content

def prefix_symbols_in_content(content, prefix):
    """Replace UI symbols in content with prefixed versions"""
    # Match only lowercase ui_ prefixed identifiers (not _UI_ or UI_)
    # This ensures we only prefix SquareLine Studio generated symbols like ui_Screen1
    # and not internal macros like _UI_BASIC_PROPERTY_HEIGHT
    content = re.sub(
        r'\bui_(\w+)\b',
        rf'{prefix}_ui_\1',
        content
    )
    return content

def process_file_content(filepath, new_filename, prefix, rename_map):
    """Process a single file's content: update guards, includes, and symbols"""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_content = content
        
        # Step 1: Update include guards if it's a .h file
        if new_filename.endswith('.h'):
            updated_content = update_include_guards(filepath, new_filename)
            if updated_content:
                content = updated_content
        
        # Step 2: Update #include statements
        content = update_includes(content, rename_map)
        
        # Step 3: Prefix all ui_ symbols
        content = prefix_symbols_in_content(content, prefix)
        
        # Only write if changed
        if content != original_content:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        return False
    
    except Exception as e:
        print(f"❌ Error processing '{filepath}': {e}")
        return False

def process_ui_directory(ui_dir, prefix):
    """Process all .c and .h files in the UI directory"""
    ui_path = Path(ui_dir)
    
    if not ui_path.exists():
        print(f"❌ Error: Directory {ui_dir} does not exist")
        return False
    
    # Step 1: Rename all files
    rename_map = rename_files(ui_dir, prefix)
    
    if not rename_map:
        print("\n🤷 No files found to rename (they may already be prefixed)")
        # Still try to process existing prefixed files
        files_to_process = list(ui_path.glob('*.c')) + list(ui_path.glob('*.h'))
        if not files_to_process:
            print("⚠ No .c or .h files found in directory")
            return False
        rename_map = {}  # Empty map for processing
    
    # Step 2: Update content in all files
    print("\n--- 📝 Step 2: Updating file content ---")
    modified_files = []
    
    for ext in ['*.c', '*.h']:
        for filepath in ui_path.glob(ext):
            filename = filepath.name
            if process_file_content(filepath, filename, prefix, rename_map):
                modified_files.append(filepath)
                print(f"🔧 Content updated in '{filename}'")
    
    # Summary
    if rename_map or modified_files:
        print(f"\n✅ Process completed successfully!")
        if rename_map:
            print(f"   - Renamed {len(rename_map)} files")
        if modified_files:
            print(f"   - Modified content in {len(modified_files)} files")
    else:
        print(f"\n⚠ No changes were needed")
    
    return True

def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    
    ui_dir = sys.argv[1]
    prefix = sys.argv[2]
    
    print(f"🚀 Prefixing UI files in {ui_dir} with '{prefix}_'...\n")
    
    if not process_ui_directory(ui_dir, prefix):
        sys.exit(1)

if __name__ == '__main__':
    main()
