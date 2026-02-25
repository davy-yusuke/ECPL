#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
ECC="$ROOT_DIR/build/ecc"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PASS=0
FAIL=0
TOTAL=0

if [ ! -f "$ECC" ]; then
    printf "${RED}Error: Compiler not found at $ECC${NC}\n"
    echo "Please build the project first: cd build && make"
    exit 1
fi

compile_run_and_verify() {
    local ec_file="$1"
    local expected_output="$2"
    local basename=$(basename "$ec_file" .ec)
    local out_dir="/tmp/ecpl_test_${basename}"
    local ll_file="$out_dir/${basename}.ll"
    local obj_file="$out_dir/${basename}.o"
    local exe_file="$out_dir/${basename}_exe"
    
    rm -rf "$out_dir"
    mkdir -p "$out_dir"
    
    ((TOTAL++))
    printf "  ${BLUE}$basename${NC}... "
    
    $ECC "$ec_file" -o "$out_dir" 2>&1 > "$out_dir/compile.log"
    
    if grep -q "codegen error\|codegen failed" "$out_dir/compile.log"; then
        printf "${RED}FAIL${NC} (codegen error)\n"
        cat "$out_dir/compile.log"
        ((FAIL++))
        return 1
    fi
    
    if [ ! -f "$ll_file" ]; then
        printf "${RED}FAIL${NC} (no IR generated)\n"
        ((FAIL++))
        return 1
    fi
    
    llc -filetype=obj "$ll_file" -o "$obj_file" 2>"$out_dir/llc.log"
    if [ $? -ne 0 ]; then
        printf "${RED}FAIL${NC} (llc error)\n"
        cat "$out_dir/llc.log"
        ((FAIL++))
        return 1
    fi
    
    clang "$obj_file" -o "$exe_file" 2>"$out_dir/link.log"
    if [ $? -ne 0 ]; then
        printf "${RED}FAIL${NC} (link error)\n"
        cat "$out_dir/link.log"
        ((FAIL++))
        return 1
    fi
    
    local actual_output
    if command -v timeout &> /dev/null; then
        actual_output=$(timeout 5 "$exe_file" 2>&1)
    elif command -v gtimeout &> /dev/null; then
        actual_output=$(gtimeout 5 "$exe_file" 2>&1)
    else
        actual_output=$("$exe_file" 2>&1)
    fi
    local exit_code=$?
    
    if [ $exit_code -eq 124 ] || [ $exit_code -eq 137 ]; then
        printf "${RED}FAIL${NC} (timeout)\n"
        ((FAIL++))
        return 1
    fi
    
    if [ -n "$expected_output" ]; then
        if echo "$actual_output" | grep -q "$expected_output"; then
            printf "${GREEN}PASS${NC}\n"
            ((PASS++))
            return 0
        else
            printf "${RED}FAIL${NC} (output mismatch)\n"
            echo "  Expected: $expected_output"
            echo "  Got: $(echo "$actual_output" | head -1)"
            ((FAIL++))
            return 1
        fi
    else
        printf "${GREEN}PASS${NC}\n"
        ((PASS++))
        return 0
    fi
}

test_project() {
    local project_dir="$1"
    local expected_output="$2"
    local project_name=$(basename "$project_dir")
    local out_dir="/tmp/ecpl_test_proj_${project_name}"
    
    rm -rf "$out_dir"
    mkdir -p "$out_dir"
    
    ((TOTAL++))
    printf "  ${BLUE}$project_name (project)${NC}... "
    
    rm -rf "$project_dir/build"
    
    (cd "$project_dir" && $ECC build 2>&1) > "$out_dir/compile.log"
    
    if grep -q "codegen error\|codegen failed\|error\|failed" "$out_dir/compile.log"; then
        printf "${RED}FAIL${NC} (build error)\n"
        cat "$out_dir/compile.log"
        ((FAIL++))
        return 1
    fi
    
    local ll_file=$(find "$project_dir/build" -name "*.ll" 2>/dev/null | head -1)
    if [ -z "$ll_file" ]; then
        printf "${RED}FAIL${NC} (no IR generated)\n"
        ((FAIL++))
        return 1
    fi
    
    local obj_file="$out_dir/program.o"
    local exe_file="$out_dir/program"
    
    llc -filetype=obj "$ll_file" -o "$obj_file" 2>"$out_dir/llc.log"
    if [ $? -ne 0 ]; then
        printf "${RED}FAIL${NC} (llc error)\n"
        cat "$out_dir/llc.log"
        ((FAIL++))
        return 1
    fi
    
    clang "$obj_file" -o "$exe_file" 2>"$out_dir/link.log"
    if [ $? -ne 0 ]; then
        printf "${RED}FAIL${NC} (link error)\n"
        cat "$out_dir/link.log"
        ((FAIL++))
        return 1
    fi
    
    local actual_output
    if command -v timeout &> /dev/null; then
        actual_output=$(timeout 5 "$exe_file" 2>&1)
    elif command -v gtimeout &> /dev/null; then
        actual_output=$(gtimeout 5 "$exe_file" 2>&1)
    else
        actual_output=$("$exe_file" 2>&1)
    fi
    local exit_code=$?
    
    if [ $exit_code -eq 124 ] || [ $exit_code -eq 137 ]; then
        printf "${RED}FAIL${NC} (timeout)\n"
        ((FAIL++))
        return 1
    fi
    
    if [ -n "$expected_output" ]; then
        if echo "$actual_output" | grep -q "$expected_output"; then
            printf "${GREEN}PASS${NC}\n"
            ((PASS++))
            return 0
        else
            printf "${RED}FAIL${NC} (output mismatch)\n"
            echo "  Expected: $expected_output"
            echo "  Got: $(echo "$actual_output" | head -1)"
            ((FAIL++))
            return 1
        fi
    else
        printf "${GREEN}PASS${NC}\n"
        ((PASS++))
        return 0
    fi
}

echo "========================================"
echo "  ECPL Test Suite"
echo "========================================"
echo ""

echo "--- Single File Tests ---"
compile_run_and_verify "$SCRIPT_DIR/01_hello.ec" "Hello World"
compile_run_and_verify "$SCRIPT_DIR/02_for_c_style.ec" "10"
compile_run_and_verify "$SCRIPT_DIR/03_fizzbuzz.ec" "FizzBuzz"

echo ""
echo "--- Project Tests ---"
test_project "$SCRIPT_DIR/04_module_project" "30"
test_project "$SCRIPT_DIR/05_multi_module" "Hello"

echo ""
echo "========================================"
printf "  Results: ${GREEN}$PASS/$TOTAL passed${NC}\n"
if [ $FAIL -gt 0 ]; then
    printf "  ${RED}$FAIL failed${NC}\n"
fi
echo "========================================"

if [ $FAIL -gt 0 ]; then
    exit 1
fi

exit 0
