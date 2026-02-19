# ECPL Examples

This directory contains example programs written in ECPL.

## Files

| File | Description |
|------|-------------|
| `01_hello.ec` | Basic "Hello World" program |
| `02_for_c_style.ec` | C-style for loop example |
| `03_fizzbuzz.ec` | Classic FizzBuzz problem |

## Running Tests

Execute the test script to compile and run all examples:

```bash
./run_tests.sh
```

Requirements:
- ECPL compiler built (`cd build && make`)
- LLVM tools (`llc`)
- Clang

## Writing New Examples

When adding new examples, make sure they:
1. Have a `fn main()` entry point
2. Can be compiled with the current compiler version
3. Update `run_tests.sh` to include the new test
