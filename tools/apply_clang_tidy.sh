#!/bin/bash
# Apply clang-tidy fixes to all source files in the Solstice codebase
# Usage: Run from WSL with: bash tools/apply_clang_tidy.sh

# Don't exit on error - we want to process all files even if some fail
set +e

# Get the script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=========================================="
echo "Applying clang-tidy fixes to Solstice"
echo "=========================================="
echo "Project root: $PROJECT_ROOT"
echo ""

# Check if clang-tidy is available
if ! command -v clang-tidy &> /dev/null; then
    echo "ERROR: clang-tidy not found. Please install clang-tidy."
    echo "On Ubuntu/Debian: sudo apt-get install clang-tidy"
    exit 1
fi

# Find all C++ source files
echo "Finding C++ source files..."
# Use array to handle files with spaces properly
mapfile -t SOURCE_FILES_ARRAY < <(find "$PROJECT_ROOT/source" -type f \( -name "*.cxx" -o -name "*.cpp" -o -name "*.cc" -o -name "*.c" \) 2>/dev/null)

if [ ${#SOURCE_FILES_ARRAY[@]} -eq 0 ]; then
    echo "No C++ source files found."
    exit 0
fi

# Count files
FILE_COUNT=${#SOURCE_FILES_ARRAY[@]}
echo "Found $FILE_COUNT source file(s)"
echo ""

# Create compile_commands.json if it doesn't exist
COMPILE_COMMANDS="$PROJECT_ROOT/compile_commands.json"
HAS_COMPILE_DB=false

if [ -f "$COMPILE_COMMANDS" ]; then
    HAS_COMPILE_DB=true
    echo "Using compile_commands.json"
else
    echo "WARNING: compile_commands.json not found."
    echo "clang-tidy works best with compile_commands.json."
    echo "Using fallback include paths (may have limited functionality)..."
    echo ""
fi

# Build include path arguments for clang-tidy when compile_commands.json is missing
# These match the project structure and common include directories
EXTRA_ARGS=(
    "--extra-arg=-I$PROJECT_ROOT"
    "--extra-arg=-I$PROJECT_ROOT/source"
    "--extra-arg=-std=c++20"
    "--extra-arg=-xc++"
    "--extra-arg=-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS"
    "--extra-arg=-D_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS"
    "--extra-arg=-DNOMINMAX"
    "--extra-arg=-DWIN32_LEAN_AND_MEAN"
    "--extra-arg=-D_CRT_SECURE_NO_WARNINGS"
    "--extra-arg=-DSOLSTICE_EXPORTS"
    "--extra-arg=-Wno-everything"
)

# Add 3rdparty include paths if they exist
if [ -d "$PROJECT_ROOT/3rdparty/bgfx/include" ]; then
    EXTRA_ARGS+=("--extra-arg=-I$PROJECT_ROOT/3rdparty/bgfx/include")
fi
if [ -d "$PROJECT_ROOT/3rdparty/sdl3/include" ]; then
    EXTRA_ARGS+=("--extra-arg=-I$PROJECT_ROOT/3rdparty/sdl3/include")
fi
if [ -d "$PROJECT_ROOT/3rdparty/reactphysics3d/include" ]; then
    EXTRA_ARGS+=("--extra-arg=-I$PROJECT_ROOT/3rdparty/reactphysics3d/include")
fi

# Run clang-tidy on each file
FIXED_COUNT=0
ERROR_COUNT=0
SKIPPED_COUNT=0
WARNING_COUNT=0

# Check for compile_commands.json in common locations
COMPILE_COMMANDS="$PROJECT_ROOT/compile_commands.json"
if [ ! -f "$COMPILE_COMMANDS" ]; then
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
    fi
fi

for file in "${SOURCE_FILES_ARRAY[@]}"; do
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
    if [ "$HAS_COMPILE_DB" = true ] && [ -f "$COMPILE_COMMANDS" ]; then
        # Use compile_commands.json
        CMD_ARGS=(-p "$PROJECT_ROOT" -fix "$file")
    else
        # Use extra-arg to pass include paths
        CMD_ARGS=("${EXTRA_ARGS[@]}" -fix "$file")
    fi

    # Run clang-tidy with fixes
    # Capture both stdout and stderr, but don't fail on errors
    OUTPUT=$(clang-tidy "${CMD_ARGS[@]}" 2>&1)
    EXIT_CODE=$?

    # Check for actual errors vs warnings
    if echo "$OUTPUT" | grep -qi "error:"; then
        # Check if it's a configuration error (which we can ignore) or a real compiler error
        if echo "$OUTPUT" | grep -qi "error.*file not found\|error.*clang-diagnostic-error"; then
            ((ERROR_COUNT++))
            echo "  ERROR: Compiler errors found (missing includes or compile_commands.json needed)"
            # Show first error line
            echo "$OUTPUT" | grep -i "error:" | head -1 | sed 's/^/    /'
        else
            # Configuration or other non-critical errors
            ((WARNING_COUNT++))
        fi
    elif echo "$OUTPUT" | grep -qi "warning:"; then
        ((WARNING_COUNT++))
        # Count as fixed if it's just warnings (clang-tidy can fix warnings)
        ((FIXED_COUNT++))
    elif [ $EXIT_CODE -eq 0 ]; then
        ((FIXED_COUNT++))
    else
        # Unknown error, but don't count as critical
        ((WARNING_COUNT++))
    fi
done

echo ""
echo "=========================================="
echo "clang-tidy processing complete!"
echo "=========================================="
echo "Files processed successfully: $FIXED_COUNT"
echo "Files with warnings: $WARNING_COUNT"
echo "Files with errors: $ERROR_COUNT"
echo "Files skipped (3rdparty): $SKIPPED_COUNT"
echo ""
if [ $ERROR_COUNT -gt 0 ]; then
    echo "NOTE: Some files had compiler errors that prevented fixes."
    echo "      To fix this, generate compile_commands.json by:"
    echo "      1. Configuring CMake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    echo "      2. Or running: cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON <build-dir>"
    echo ""
fi
if [ "$HAS_COMPILE_DB" = false ]; then
    echo "TIP: For best results, generate compile_commands.json:"
    echo "     cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B out/build"
    echo "     Then copy or symlink it to the project root"
    echo ""
fi
echo "Note: Review the changes before committing."
echo "Some issues may require manual intervention."

