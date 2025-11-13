#!/bin/bash
# Profile EDN.C with Instruments - ensures debug symbols are visible
#
# Usage: ./profile.sh [output_name]
#
# This script:
# 1. Builds benchmark with debug symbols
# 2. Runs Instruments Time Profiler
# 3. Opens results in Instruments GUI
#

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
BENCHMARK="./bench/bench_profile_session"
OUTPUT_NAME="${1:-profile_$(date +%Y%m%d_%H%M%S)}"
TRACE_FILE="${OUTPUT_NAME}.trace"

echo -e "${BLUE}=== EDN.C Performance Profiling ===${NC}\n"

# Step 1: Clean and rebuild with debug symbols
echo -e "${YELLOW}Step 1: Building benchmark with debug symbols...${NC}"
make clean > /dev/null 2>&1
make bench/bench_profile_session

if [ ! -f "$BENCHMARK" ]; then
    echo "Error: Benchmark not found at $BENCHMARK"
    exit 1
fi

# Step 2: Verify debug symbols exist
echo -e "\n${YELLOW}Step 2: Verifying debug symbols...${NC}"
if [ -d "${BENCHMARK}.dSYM" ]; then
    echo -e "${GREEN}✓ dSYM bundle found: ${BENCHMARK}.dSYM${NC}"
    
    # Check if dSYM has actual debug info
    DWARF_FILE="${BENCHMARK}.dSYM/Contents/Resources/DWARF/$(basename $BENCHMARK)"
    if [ -f "$DWARF_FILE" ]; then
        SIZE=$(stat -f%z "$DWARF_FILE")
        echo -e "${GREEN}✓ Debug symbols present (${SIZE} bytes)${NC}"
    else
        echo -e "${YELLOW}⚠ Warning: dSYM bundle exists but seems empty${NC}"
    fi
else
    echo -e "${YELLOW}⚠ Warning: No dSYM bundle found (symbols may still work)${NC}"
fi

# Check for symbol names in binary
SYMBOL_COUNT=$(nm "$BENCHMARK" | grep -c " T _edn_" || true)
echo -e "${GREEN}✓ Found ${SYMBOL_COUNT} EDN symbols in binary${NC}"

# Step 3: Run Instruments
echo -e "\n${YELLOW}Step 3: Running Instruments Time Profiler...${NC}"
echo "This will take ~12 seconds (benchmark duration)"
echo ""

# Use xcrun xctrace (modern) or instruments (legacy)
if command -v xcrun &> /dev/null && xcrun xctrace help &> /dev/null 2>&1; then
    echo "Using: xcrun xctrace record"
    xcrun xctrace record \
        --template 'Time Profiler' \
        --output "$TRACE_FILE" \
        --launch -- "$BENCHMARK"
else
    echo "Using: instruments"
    instruments -t "Time Profiler" \
        -D "$TRACE_FILE" \
        "$BENCHMARK"
fi

# Step 4: Open results
echo -e "\n${YELLOW}Step 4: Opening results in Instruments...${NC}"
if [ -d "$TRACE_FILE" ]; then
    TRACE_SIZE=$(du -h "$TRACE_FILE" | cut -f1)
    echo -e "${GREEN}✓ Trace saved: $TRACE_FILE (${TRACE_SIZE})${NC}"
    echo ""
    echo "Opening Instruments GUI..."
    open "$TRACE_FILE"
    
    echo ""
    echo -e "${BLUE}=== Quick Guide ===${NC}"
    echo "In Instruments:"
    echo "  1. Click 'Call Tree' dropdown (top of bottom panel)"
    echo "  2. Select 'Heaviest Stack Trace'"
    echo "  3. Look for functions with >5% in '% Self' column"
    echo ""
    echo "You should see symbol names like:"
    echo "  • edn_parser_parse_value"
    echo "  • edn_parse_map"
    echo "  • edn_arena_alloc"
    echo "  • edn_value_equal_internal"
    echo ""
    echo "If you DON'T see these names:"
    echo "  1. Check View > Debug Symbols"
    echo "  2. Ensure ${BENCHMARK}.dSYM is in the same directory"
    echo "  3. Try Xcode > Preferences > Locations > Command Line Tools"
    echo ""
else
    echo -e "${YELLOW}⚠ Warning: Trace file not created${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Profiling complete!${NC}"
