#!/bin/bash
# Quick Start: Component-Based Architecture Migration
# Run this script to see what needs to be done

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║     Component-Based Architecture - Quick Start Guide          ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""
echo "This guide will help you migrate to the new architecture."
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ Error: Please run this from the project root directory"
    exit 1
fi

echo "✓ Current directory: $(pwd)"
echo ""

echo "📋 STEP-BY-STEP GUIDE"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "STEP 1: Understand the New Structure"
echo "────────────────────────────────────"
echo "  Read:  components/README.md"
echo "  View:  ARCHITECTURE_DIAGRAMS.md"
echo ""
read -p "Press Enter when you've reviewed the architecture..."
echo ""

echo "STEP 2: Run the Migration Script"
echo "────────────────────────────────────"
echo "  This will copy all files to the new structure"
echo ""
echo "  Run: ./migrate_to_components.sh"
echo ""
read -p "Press Enter to run the migration script now, or Ctrl+C to do it manually..."
echo ""

if [ -f "migrate_to_components.sh" ]; then
    chmod +x migrate_to_components.sh
    ./migrate_to_components.sh
    echo ""
    read -p "Press Enter to continue..."
else
    echo "❌ migrate_to_components.sh not found!"
    exit 1
fi

echo ""
echo "STEP 3: Update Main CMakeLists.txt"
echo "────────────────────────────────────"
echo "  Open: CMakeLists.txt"
echo ""
echo "  Change line ~5 from:"
echo "    set(EXTRA_COMPONENT_DIRS \"modules_src\" \"modules_src/ui_buck\" \"modules_src/ui_dimmer\" \"src\")"
echo ""
echo "  To:"
echo "    set(EXTRA_COMPONENT_DIRS \"components/dimmer_module\" \"components/buck_module\" \"src\")"
echo ""
read -p "Press Enter when you've updated CMakeLists.txt..."
echo ""

echo "STEP 4: Build the Project"
echo "────────────────────────────────────"
echo "  Run: idf.py build"
echo ""
read -p "Press Enter to build now, or Ctrl+C to build manually..."
echo ""

idf.py build

if [ $? -eq 0 ]; then
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║                  ✓ BUILD SUCCESSFUL!                           ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
    echo "Next steps:"
    echo "  1. Flash to device: idf.py flash"
    echo "  2. Monitor output:  idf.py monitor"
    echo "  3. Test both modules (buck and dimmer)"
    echo ""
    echo "If everything works, you can clean up old directories:"
    echo "  rm -rf modules_src/ui_buck modules_src/ui_dimmer"
    echo "  rm -rf modules_include/ui_buck modules_include/ui_dimmer"
    echo ""
else
    echo ""
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║                  ⚠ BUILD FAILED                                ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo ""
    echo "Common issues and solutions:"
    echo ""
    echo "1. SYMBOL CONFLICTS (multiple definition errors)"
    echo "   Solution: Run the symbol renaming script"
    echo "   → python3 rename_dimmer_symbols.py"
    echo "   → idf.py build"
    echo ""
    echo "2. MISSING FILES"
    echo "   Solution: Check that migration script copied all files"
    echo "   → ls -la components/dimmer_module/"
    echo "   → ls -la components/buck_module/"
    echo ""
    echo "3. INCLUDE ERRORS"
    echo "   Solution: Update include paths in source files"
    echo "   → Check src/*.c for old include paths"
    echo ""
    echo "For detailed troubleshooting, see MIGRATION_GUIDE.md"
    echo ""
fi

echo "════════════════════════════════════════════════════════════════"
echo ""
echo "📚 Documentation:"
echo "   • IMPLEMENTATION_SUMMARY.md  - Overview of what was created"
echo "   • MIGRATION_GUIDE.md         - Detailed migration steps"
echo "   • MIGRATION_CHECKLIST.md     - Track your progress"
echo "   • components/README.md       - Architecture documentation"
echo "   • ARCHITECTURE_DIAGRAMS.md   - Visual diagrams"
echo ""
echo "💡 Need help? Check the documentation above or review the"
echo "   error messages carefully."
echo ""
