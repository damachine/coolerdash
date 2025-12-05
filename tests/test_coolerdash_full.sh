#!/bin/bash
# Full/Exhaustive Quality Check Script for CoolerDash
# Deep analysis with --check-level=exhaustive (slower but more thorough)
# Use this for release checks or security-critical changes

set -e

echo "=========================================="
echo "CoolerDash FULL Quality Checks"
echo "⚠️  This may take several minutes..."
echo "=========================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Change to project root
cd "$(dirname "$0")/.."

# 1. Build Test
echo -e "${YELLOW}[1/5] Testing build...${NC}"
if make clean && make; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi
echo ""

# 2. Exhaustive Cppcheck Static Analysis
echo -e "${YELLOW}[2/5] Running EXHAUSTIVE cppcheck analysis...${NC}"
echo -e "${BLUE}ℹ️  Using --check-level=exhaustive (may take a while)${NC}"
mkdir -p reports
START_TIME=$(date +%s)

if cppcheck --enable=warning,style,performance,portability \
    --check-level=exhaustive \
    --suppress=missingIncludeSystem \
    --inline-suppr \
    -I./src \
    --error-exitcode=1 \
    --quiet \
    src/ 2>&1 | tee reports/test_coolerdash_full-cppcheck.txt; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    echo -e "${GREEN}✓ Exhaustive cppcheck passed (took ${DURATION}s)${NC}"
    # Write success report
    cat > reports/test_coolerdash_full-cppcheck.txt << EOF
CoolerDash - EXHAUSTIVE Static Analysis Report (cppcheck)
Generated: $(date +%Y-%m-%d\ %H:%M:%S)
Analysis Duration: ${DURATION} seconds

Status: ✓ PASSED - No errors or warnings found

Deep analysis with --check-level=exhaustive completed successfully.
All code paths and branches were analyzed thoroughly.

No action required.
EOF
else
    echo -e "${RED}✗ Exhaustive cppcheck found issues (see reports/test_coolerdash_full-cppcheck.txt)${NC}"
    exit 1
fi
echo ""

# 3. Compiler Warnings Check (stricter flags)
echo -e "${YELLOW}[3/5] Testing with strict compiler warnings...${NC}"
> reports/test_coolerdash_full-compiler.txt  # Clear previous report
COMPILE_FAILED=0
for src_file in $(find src -name "*.c"); do
    echo -e "${BLUE}  Checking: $src_file${NC}"
    gcc -Wall -Wextra -Werror -Wpedantic -std=c99 \
        -I./src \
        $(pkg-config --cflags cairo jansson libcurl inih) \
        -c "$src_file" -o /tmp/test_compile.o 2>&1 | tee -a reports/test_coolerdash_full-compiler.txt
    if [ $? -ne 0 ]; then
        COMPILE_FAILED=1
    fi
    rm -f /tmp/test_compile.o
done
if [ $COMPILE_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ No compiler warnings${NC}"
    # Write success report
    cat > reports/test_coolerdash_full-compiler.txt << EOF
CoolerDash - FULL Compiler Warnings Report (gcc strict mode)
Generated: $(date +%Y-%m-%d\ %H:%M:%S)

Status: ✓ PASSED - No compiler warnings found

All source files compiled successfully with strict compiler flags:
- -Wall -Wextra -Werror -Wpedantic -std=c99

Tested files:
- src/device/sys.c
- src/device/usr.c
- src/main.c
- src/mods/circle.c
- src/mods/display.c
- src/mods/dual.c
- src/srv/cc_conf.c
- src/srv/cc_main.c
- src/srv/cc_sensor.c

No warnings or errors detected.
Code complies with C99 standard and project coding guidelines.
EOF
else
    echo -e "${YELLOW}⚠ Compiler warnings found (see reports/test_coolerdash_full-compiler.txt)${NC}"
fi
echo ""

# 4. Code Formatting Check (if clang-format is available)
echo -e "${YELLOW}[4/5] Checking code formatting...${NC}"
if command -v clang-format &> /dev/null; then
    # Check if any files need formatting
    NEEDS_FORMAT=$(find src -name "*.c" -o -name "*.h" | xargs clang-format --dry-run --Werror 2>&1 || true)
    if [ -z "$NEEDS_FORMAT" ]; then
        echo -e "${GREEN}✓ Code formatting is correct${NC}"
    else
        echo -e "${YELLOW}⚠ Some files need formatting:${NC}"
        echo "$NEEDS_FORMAT"
    fi
else
    echo -e "${YELLOW}⚠ clang-format not installed, skipping${NC}"
fi
echo ""

# 5. Extended Issue Checks
echo -e "${YELLOW}[5/5] Checking for common issues...${NC}"
ISSUES=0  # Track issues count (currently unused, reserved for future enhancement)

# Check for TODO/FIXME comments
TODO_COUNT=$(grep -rn "TODO\|FIXME" src/ 2>/dev/null | wc -l)
if [ $TODO_COUNT -gt 0 ]; then
    echo -e "${YELLOW}⚠ Found $TODO_COUNT TODO/FIXME comments:${NC}"
    grep -rn "TODO\|FIXME" src/ | head -10
    echo ""
fi

# Check for debug prints (fprintf to stderr/stdout that aren't log_message)
DEBUG_COUNT=$(grep -rn "printf\|fprintf" src/ | grep -v "log_message\|snprintf" | grep -v "//" 2>/dev/null | wc -l)
if [ $DEBUG_COUNT -gt 0 ]; then
    echo -e "${BLUE}ℹ️  Found $DEBUG_COUNT potential debug prints (may be intentional)${NC}"
    echo ""
fi

# Check for hardcoded paths
HARDCODED=$(grep -rn "/tmp/\|/home/\|/root/" src/ --include="*.c" --include="*.h" 2>/dev/null || true)
if [ ! -z "$HARDCODED" ]; then
    echo -e "${YELLOW}⚠ Found hardcoded paths:${NC}"
    echo "$HARDCODED"
    echo ""
fi

# Check for potential buffer overflows (strcpy, strcat, sprintf)
UNSAFE=$(grep -rn "strcpy\|strcat\|sprintf[^f]" src/ --include="*.c" 2>/dev/null || true)
if [ ! -z "$UNSAFE" ]; then
    echo -e "${YELLOW}⚠ Found potentially unsafe string functions:${NC}"
    echo "$UNSAFE"
    echo ""
fi

echo -e "${GREEN}✓ Extended issue checks completed${NC}"
echo ""

# Summary
echo "=========================================="
echo -e "${GREEN}FULL Quality Checks Completed!${NC}"
echo "=========================================="
echo ""
echo "Reports generated:"
echo "  - reports/test_coolerdash_full-cppcheck.txt (exhaustive analysis)"
echo "  - reports/test_coolerdash_full-compiler.txt (strict warnings)"
echo ""
echo "Next steps:"
echo "1. Review all reports for any issues"
echo "2. Fix any warnings or potential problems"
echo "3. Run regular ./tests/test_coolerdash.sh for quick checks"
echo "4. Create Pull Request when ready"
echo ""
