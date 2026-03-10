#!/bin/bash
# Format and organize includes in all source files
# Usage: Run from WSL with: bash tools/format_includes.sh

# Don't exit on error - we want to process all files even if some fail
set +e

# Get the script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=========================================="
echo "Formatting includes in Solstice"
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
# Use array to handle files with spaces properly
mapfile -t HEADER_FILES_ARRAY < <(find "$PROJECT_ROOT/source" -type f \( -name "*.hxx" -o -name "*.hpp" -o -name "*.h" \) 2>/dev/null)
mapfile -t SOURCE_FILES_ARRAY < <(find "$PROJECT_ROOT/source" -type f \( -name "*.cxx" -o -name "*.cpp" -o -name "*.cc" \) 2>/dev/null)

# Combine arrays
ALL_FILES_ARRAY=("${HEADER_FILES_ARRAY[@]}" "${SOURCE_FILES_ARRAY[@]}")

if [ ${#ALL_FILES_ARRAY[@]} -eq 0 ]; then
    echo "No C++ files found."
    exit 0
fi

# Count files
FILE_COUNT=${#ALL_FILES_ARRAY[@]}
echo "Found $FILE_COUNT file(s)"
echo ""

# Check for compile_commands.json in common locations
COMPILE_COMMANDS="$PROJECT_ROOT/compile_commands.json"
HAS_COMPILE_DB=false

if [ -f "$COMPILE_COMMANDS" ]; then
    HAS_COMPILE_DB=true
    echo "Using compile_commands.json"
else
    # Check in build directory
    COMPILE_COMMANDS="$PROJECT_ROOT/out/build/x64-release/compile_commands.json"
    if [ -f "$COMPILE_COMMANDS" ]; then
        HAS_COMPILE_DB=true
        echo "Found compile_commands.json in build directory"
        # Create symlink or copy to project root for clang-tidy
        if [ ! -f "$PROJECT_ROOT/compile_commands.json" ]; then
            ln -sf "$COMPILE_COMMANDS" "$PROJECT_ROOT/compile_commands.json" 2>/dev/null || \
            cp "$COMPILE_COMMANDS" "$PROJECT_ROOT/compile_commands.json" 2>/dev/null || true
        fi
        COMPILE_COMMANDS="$PROJECT_ROOT/compile_commands.json"
    else
        echo "WARNING: compile_commands.json not found."
        echo "clang-tidy works best with compile_commands.json."
        echo ""
    fi
fi

# Run clang-tidy to fix includes
FIXED_COUNT=0
ERROR_COUNT=0
SKIPPED_COUNT=0
WARNING_COUNT=0

for file in "${ALL_FILES_ARRAY[@]}"; do
    if [ -z "$file" ] || [ ! -f "$file" ]; then
        continue
    fi

    # Skip files in 3rdparty directories
    if [[ "$file" == *"3rdparty"* ]]; then
        ((SKIPPED_COUNT++))
        continue
    fi

    echo "Processing: ${file#$PROJECT_ROOT/}"

    # Build clang-tidy command
    # Use readability-include-order check which is configured in .clang-tidy
    if [ "$HAS_COMPILE_DB" = true ] && [ -f "$COMPILE_COMMANDS" ]; then
        # Use compile_commands.json with include ordering check
        CMD_ARGS=(-p "$PROJECT_ROOT" -fix -checks='readability-include-order' "$file")
    else
        # Without compile_commands.json, we can't reliably fix includes
        echo "  SKIPPED: compile_commands.json needed for include formatting"
        ((WARNING_COUNT++))
        continue
    fi

    # Run clang-tidy to fix includes
    OUTPUT=$(clang-tidy "${CMD_ARGS[@]}" 2>&1)
    EXIT_CODE=$?

    # Check results
    if [ $EXIT_CODE -eq 0 ]; then
        ((FIXED_COUNT++))
    elif echo "$OUTPUT" | grep -qi "error.*no checks enabled"; then
        # This shouldn't happen with our checks, but handle it
        ((ERROR_COUNT++))
        echo "  ERROR: No checks enabled (check clang-tidy version)"
    elif echo "$OUTPUT" | grep -qi "error:"; then
        ((ERROR_COUNT++))
        echo "  ERROR: Compiler errors found"
        echo "$OUTPUT" | grep -i "error:" | head -1 | sed 's/^/    /'
    else
        # Partial success or warnings
        ((FIXED_COUNT++))
        ((WARNING_COUNT++))
    fi
done

echo ""
echo "=========================================="
echo "Include formatting complete!"
echo "=========================================="
echo "Files processed successfully: $FIXED_COUNT"
echo "Files with warnings: $WARNING_COUNT"
echo "Files with errors: $ERROR_COUNT"
echo "Files skipped (3rdparty): $SKIPPED_COUNT"
echo ""
if [ "$HAS_COMPILE_DB" = false ]; then
    echo "NOTE: compile_commands.json is required for include formatting."
    echo "      Generate it with: cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B out/build"
    echo ""
fi
echo "Note: Review the changes before committing."
echo "Include order: system headers → third-party → project headers"

