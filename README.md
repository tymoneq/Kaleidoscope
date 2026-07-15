# Kaleidoscope

Kaleidoscope is a small compiler project that demonstrates a simple language frontend, AST construction, optimization passes, and LLVM-based code generation/JIT integration. This repository contains sources, headers, and a Makefile to build the project and a small test harness.

This is my implementation of Kaleidoscope from LLVM tutorial https://llvm.org/docs/tutorial/index.html.
This language supports:
    double type is the only data type currently available
    function definition using def keyword
    extern function like sin cos putchard with keyword extern
    mutable variable 
    control flow if then else construction
    for loop

**example code**
```
# Define ':' for sequencing: as a low-precedence operator that ignores operands
# and just returns the RHS.
def binary : 1 (x y) y;

# Recursive fib, we could do this before.
def fib(x)
  if (x < 3) then
    1
  else
    fib(x-1)+fib(x-2);

# Iterative fib.
def fibi(x)
  var a = 1, b = 1, c in
  (for i = 3, i < x in
     c = a + b :
     a = b :
     b = c) :
  b;

# Call it.
fibi(10);



extern putchard(char);
def printstar(n)
  for i = 1, i < n, 1.0 in
    putchard(42);  # ascii 42 = '*'

# print 100 '*' characters
printstar(100);

```

**Status:** lightweight learning/example project — requires LLVM development tools to build.

**Contents:** brief usage, build, and development instructions for this workspace.

**Prerequisites:**
- `clang++` or a compatible C++ compiler
- LLVM development libraries and tools (ensure `llvm-config` is on `PATH`)
- `make`

**Build**

From the repository root run:

```bash
make all
```

This will produce the `main` binary by compiling object files and linking with the LLVM toolchain flags (the Makefile invokes `llvm-config`). To build the test harness run:

```bash
make test
```

To clean build artifacts:

```bash
make clean
```

**Run**

After `make` completes you can run the main program:

```bash
./main
```
Then type your function and exit program


If `test` was built, run it with:

```bash
./test
```

**Project layout**

- `main.cpp` - program entry point
- `test.cpp` - small test harness
- `Makefile` - build rules (uses `llvm-config`)
- `include/` - public headers: `ast.hpp`, `lexer.hpp`, `optimizer.hpp`, `llvmStructs.hpp`, `KaleidoscopeJIT.h`
- `src/` - implementation: `ast.cpp`, `lexer.cpp`, `llvmStructs.cpp`, `optimizer.cpp`
- `LICENSE` - project license

**Development notes**

- The Makefile relies on `llvm-config` to provide flags; ensure it points to the LLVM installation you want to target.
- Compilation flags are controlled by the `FLAGS` variable in the Makefile. You can inspect or override it when invoking `make`:

```bash
make FLAGS="$(llvm-config --cxxflags --ldflags --system-libs --libs all)"
```

- If you prefer to build a single translation unit manually, you can use the `clang++` command shown in the Makefile rules; the Makefile already scripts the correct sequence of compile+link steps.

**Troubleshooting**

- If `make` fails due to missing LLVM headers or libraries, confirm `llvm-config --version` works and that the output of `llvm-config --cxxflags` contains the include paths.
- On systems with multiple LLVM versions, make sure `llvm-config` resolves to the intended version (you can use an absolute path or adjust `PATH`).