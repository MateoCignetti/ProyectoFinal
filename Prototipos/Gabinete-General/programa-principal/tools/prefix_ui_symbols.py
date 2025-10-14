#!/usr/bin/env python3
"""
UI Symbol Prefixer for Module Isolation

This script prefixes all UI symbols in a module's UI files with the module name
to prevent symbol conflicts when multiple modules are linked together.

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

def prefix_symbols_in_file(filepath, prefix, symbol_patterns):
    """Replace UI symbols in a file with prefixed versions"""
    with open(filepath, 'r') as f:
        content = f.read()
    
    original_content = content
    
    # Replace each symbol pattern
    for pattern in symbol_patterns:
        # Match: extern <type> ui_<name>; or <type> ui_<name> = ...;
        content = re.sub(
            r'\b(ui_\w+)\b',
            lambda m: f'{prefix}_{m.group(1)}' if m.group(1).startswith('ui_') else m.group(1),
            content
        )
    
    # Only write if changed
    if content != original_content:
        with open(filepath, 'w') as f:
            f.write(content)
        return True
    return False

def process_ui_directory(ui_dir, prefix):
    """Process all .c and .h files in the UI directory"""
    ui_path = Path(ui_dir)
    
    if not ui_path.exists():
        print(f"Error: Directory {ui_dir} does not exist")
        return False
    
    # Common UI symbol patterns
    symbol_patterns = [
        r'ui_\w+',  # All ui_ prefixed identifiers
    ]
    
    modified_files = []
    
    # Process all .c and .h files
    for ext in ['*.c', '*.h']:
        for filepath in ui_path.glob(ext):
            if prefix_symbols_in_file(filepath, prefix, symbol_patterns):
                modified_files.append(filepath)
                print(f"✓ Modified: {filepath}")
    
    if modified_files:
        print(f"\n✓ Successfully prefixed {len(modified_files)} files with '{prefix}_'")
    else:
        print(f"\n⚠ No files needed modification (already prefixed or no UI files found)")
    
    return True

def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    
    ui_dir = sys.argv[1]
    prefix = sys.argv[2]
    
    print(f"Prefixing UI symbols in {ui_dir} with '{prefix}_'...\n")
    
    if not process_ui_directory(ui_dir, prefix):
        sys.exit(1)

if __name__ == '__main__':
    main()
