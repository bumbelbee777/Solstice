#!/bin/bash
# Check naming conventions across the codebase
# Usage: Run from WSL with: bash tools/check_naming.sh

set -e

# Get the script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=========================================="
echo "Checking naming conventions in Solstice"
echo "=========================================="
echo "Project root: $PROJECT_ROOT"
echo ""

# Check if clang-tidy is available
if ! command -v clang-tidy &> /dev/null; then
    echo "ERROR: clang-tidy not found. Please install clang-tidy."
    echo "On Ubuntu/Debian: sudo apt-get install clang-tidy"
    exit 1
fi

# Find all C++ header and source files
echo "Finding C++ files..."
HEADER_FILES=$(find "$PROJECT_ROOT/source" -type f \( -name "*.hxx" -o -name "*.hpp" -o -name "*.h" \) 2>/dev/null || true)
SOURCE_FILES=$(find "$PROJECT_ROOT/source" -type f \( -name "*.cxx" -o -name "*.cpp" -o -name "*.cc" \) 2>/dev/null || true)

ALL_FILES=$(echo -e "$HEADER_FILES\n$SOURCE_FILES" | grep -v "^$")

if [ -z "$ALL_FILES" ]; then
    echo "No C++ files found."
    exit 0
fi

# Count files
FILE_COUNT=$(echo "$ALL_FILES" | wc -l)
echo "Found $FILE_COUNT file(s)"
echo ""

# Create compile_commands.json if it doesn't exist
COMPILE_COMMANDS="$PROJECT_ROOT/compile_commands.json"
if [ ! -f "$COMPILE_COMMANDS" ]; then
    echo "WARNING: compile_commands.json not found."
    echo "clang-tidy works best with compile_commands.json."
    echo ""
fi

# Run clang-tidy to check naming
VIOLATION_COUNT=0
CLEAN_COUNT=0
SKIPPED_COUNT=0
VIOLATION_FILES=()

while IFS= read -r file; do
    if [ -z "$file" ]; then
        continue
    fi

    # Skip files in 3rdparty directories
    if [[ "$file" == *"3rdparty"* ]]; then
        ((SKIPPED_COUNT++))
        continue
    fi

    # Check for naming violations
    OUTPUT=$(clang-tidy -p "$PROJECT_ROOT" \
        -checks='-*,readability-identifier-naming' \
        "$file" 2>&1 || true)

    if echo "$OUTPUT" | grep -q "warning:"; then
        echo "VIOLATIONS in: ${file#$PROJECT_ROOT/}"
        echo "$OUTPUT" | grep "warning:" | head -5
        ((VIOLATION_COUNT++))
        VIOLATION_FILES+=("$file")
    else
        ((CLEAN_COUNT++))
    fi
done <<< "$ALL_FILES"

echo ""
echo "=========================================="
echo "Naming convention check complete!"
echo "=========================================="
echo "Files with violations: $VIOLATION_COUNT"
echo "Files clean: $CLEAN_COUNT"
echo "Files skipped (3rdparty): $SKIPPED_COUNT"
echo ""

if [ $VIOLATION_COUNT -gt 0 ]; then
    echo "Files with naming violations:"
    for file in "${VIOLATION_FILES[@]}"; do
        echo "  - ${file#$PROJECT_ROOT/}"
    done
    echo ""
    echo "Run 'bash tools/apply_clang_tidy.sh' to auto-fix some issues."
    exit 1
else
    echo "All files conform to naming conventions!"
    exit 0
fi

