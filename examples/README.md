# ECPL Examples

This directory contains example programs written in ECPL.

## Single-File Examples

| File | Description |
|------|-------------|
| `01_hello.ec` | Basic "Hello World" program |
| `02_for_c_style.ec` | C-style for loop example |
| `03_fizzbuzz.ec` | Classic FizzBuzz problem |

## Project Examples

| Directory | Description |
|------|-------------|
| `04_module_project/` | Single module import example |
| `05_multi_module/` | Multi-module project with nested packages |

### Project Structure

Each project has an `ecpl.json` configuration file:

```
project/
├── ecpl.json          # Project configuration
└── src/
    ├── main.ec        # Entry point
    └── math.ec        # Module with pub functions
```

### ecpl.json Format

```json
{
    "name": "myproject",
    "version": "0.1.0",
    "entry": "src/main.ec",
    "src": ["src/"],
    "output": "build/"
}
```

### Module Syntax

```ecpl
// Declaration (exported with pub)
module math

pub fn add(a i32, b i32) i32 {
    return a + b
}

fn internal_helper() {  // Not exported (private)
    // ...
}

// Import and usage
import "math"

fn main() {
    result := add(1, 2)  // Calls math.add
    println(result)
}
```

### Visibility Rules

- Functions with `pub` keyword are exported and accessible from other modules
- Functions without `pub` are private (static linkage in LLVM IR)
- Use `pub fn` for public API functions

## Running Tests

```bash
./run_tests.sh
```

For project-based examples:

```bash
cd examples/04_module_project
../../build/ecc build
```

Requirements:
- ECPL compiler built (`cd build && make`)
- LLVM tools (`llc`)
- Clang

## Adding New Examples

1. Single file: Add `.ec` file and update `run_tests.sh`
2. Project: Create directory with `ecpl.json` and `src/` folder
