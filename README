# ykcc

A C++ compiler written from scratch, aiming to be self-hosting.

## Overview

ykcc is a C++ compiler project. The bootstrap compiler is written in C++23 and compiled by a host compiler (GCC/Clang). Once the bootstrap compiler can compile itself, the plan is to rewrite it as a C++20 module-based compiler.

## Project Structure

```
compiler/
  bootstrap/          # Bootstrap compiler (C++23)
    src/              # Compiler entry point
    asteroid/         # Core library (lexer, parser, AST)
      include/        # Public headers
      src/            # Library sources
    test/             # Tests
```

## Building

```sh
cmake -S compiler/bootstrap -B compiler/bootstrap/build -G Ninja
cmake --build compiler/bootstrap/build
```

## Testing

```sh
ctest --test-dir compiler/bootstrap/build
```

## License

[MIT](LICENSE)
