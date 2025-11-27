# ECPL

**ECPL is a new programming language designed for system programming.**

## Project Highlights

- **LLVM-based**  
  ECPL employs LLVM as its backend, enabling state-of-the-art optimization and multi-platform support.
- **Not Even Beta Yet**  
  This project is in a very early alpha phase and hasn't reached beta status. It is not stable, not for production use, and may undergo significant design and syntax changes.
- **For System Programming**  
  Its primary target is low-level development, such as OS, compilers, and middleware, valuing performance, powerful type representation, and low-level access.

## Design Details

- **Modern Syntax**  
  The syntax is influenced by C-family languages, Go, and Rust.
- **Type System**  
  Statically typed with partial type inference. Structs, pointers, slice arrays, and function types are supported.
- **LLVM IR Generation**  
  The compiler produces LLVM IR from its AST, which can then be compiled for various architectures.
- **CLI / Toolchain**  
  The `ecc` command builds `.ec` files or entire project directories (generating combined LLVM IR). More build options and output targets are planned for the future.

## Dependencies & Building

- **CMake Build System**
- **LLVM 18 or newer recommended**
- A `Dockerfile` is provided for an easy build environment (Ubuntu + LLVM 18 + required dev packages).

```sh
# Minimal build procedure
$ mkdir build
$ cd build
$ cmake -DLLVM_DIR=/your/llvm/path/cmake ..
$ make
```

## Project Layout

- `src/` ... Lexical analysis (lexer), parser, AST, and LLVM codegen
- `cli/` ... Command-line interface implementation
- `tests/` ... (planned for future)

---

If you need more details about syntax, the type system, roadmap, or how to contribute, let me know!
