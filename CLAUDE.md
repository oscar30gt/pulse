# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Pulse is a multi-platform digital logic simulation engine for VHDL, written in C++20. It compiles VHDL source into a logic-component simulation graph, runs a dataflow simulation, and displays the resulting waveform in an interactive terminal UI. VHDL support is intentionally partial (see "Supported Syntax" in README.md) and being extended incrementally — check README.md before assuming a construct is supported.

## Build & Test Commands

Implementing a feature should be accompanied by unit tests in the corresponding `tests/` directory.
Use the following commands to build and run the project tests:

```bash
# Configure and build (Release used for the README-documented workflow, plain build for dev/CI parity)
cmake -B build
cmake --build build

# Run the full unit test suite (GoogleTest, fetched automatically by CMake)
./build/bin/pulse_tests

# Run a single test or test suite via gtest filter
./build/bin/pulse_tests --gtest_filter=SuiteName.TestName
./build/bin/pulse_tests --gtest_filter=SuiteName.*
```

Tests are all that matters, as TUI is interactive and more difficult to automate. 
If the tests pass, the engine is working as expected.

## Project Architecture

### Three subsystems, one pipeline

`src/` has three top-level areas, each in its own namespace, wired together in `src/main.cc`:

- **`compiler/`** (`Pulse::Parser`) — VHDL source to a single design object called `Blueprint`. Pipeline order: **Tokenizer → Parser (AST) → Linker → Semantic Analyzer.** Each file is tokenized and parsed independently into an `ASTRoot`, all per-file `ASTRoot`s are merged by `Linker::link()` into one linked design (resolving cross-file entity/architecture references), and only then is `analyzeAST()` run on the *linked* design for type checking and scope resolution. The tokenizer accepts the VHDL-2008 lexical grammar and the parser builds trees for the VHDL-2008 subset listed in the README (it is *blind*: syntax and the grammar of each region, such as no declarations inside a process, but never types or scopes; it throws `ast_syntax_error` at the first error, and anything outside its grammar is a syntax error). The analyzer has a handler for every node kind the parser can build, so there is no "not supported yet" layer: the few corners it does not model (aggregates and slices of multi-dimensional arrays) say so in their error message. Very long chains of operators, selections or alternatives are refused (parser: more than 2000 links; analyzer: more than 250 nested levels), and the AST frees such chains iteratively, so no input can exhaust the stack. Note this differs from the stage order shown in the README's pipeline diagram (which lists semantics before linking) — trust `main.cc`'s actual call order over the diagram.
- **`engine/`** (`Pulse::Engine`) — the simulation core. A `Blueprint` describes a component's physical circuit as a graph of `ComponentInstance`s (gates, comparators, adders, processes, nested subgraphs, etc.) connected by named wires; `Subgraph` instantiates a functional, runnable version of a `Blueprint` for dataflow simulation with IEEE 1164 multi-valued logic (`LogicVector`) and asynchronous processes.
- **`debugger/`** (`Pulse::Debugger`) — consumes the simulated waveform and renders it in an interactive terminal UI (`tui_viewer.cc`/`tui_render.cc`), letting users step through time and inspect signal values.

The compiled AST is not executed directly — it still needs to be lowered into `Blueprint`/`Subgraph` objects before simulation, and the waveform recorder feeds the debugger.

### Module layout convention

Every feature module `x` inside `engine/` or `debugger/` follows a fixed three-layer split (see CONTRIBUTING.md):

- `include/x.h` — public interface and data structures.
- `x.cc` (or multiple files for large modules, e.g. `parser/expressions.cc`, `analyzer_type_defs.cc`) — implementation.
- `tests/x.test.cc` — GoogleTest unit tests.

Additional conventions:
- `lib/` subdirectories hold internal-only headers not meant for consumers outside the module.
- `compiler/` is organised the same way but one level deeper: each pipeline stage is its own feature folder, `compiler/<feature>/{include,lib,tests,*.cc}`, with the features `diagnostics` (headers only), `tokenizer`, `parser`, `ast`, `linker` and `analyzer`. The `tokenizer`, `parser` and `ast` features name their files by grammar area without the redundant feature prefix (`parser/expressions.cc`, `tokenizer/numbers.cc`, `ast/printer.cc`); `analyzer` and `linker` still keep it (`analyzer/analyzer_choices.cc`). Includes stay bare (`#include "ast.h"`): CMake adds every feature's `include/` (public) and `lib/` (internal) directory to the include path. Test helpers shared by all compiler features live in `compiler/tests/test_helpers.h`.
- `impl/` subdirectories hold implementation for header-only/templated types whose definition lives at the end of the public header (e.g. `engine/include/blueprint.h` pulls in `engine/impl/blueprint_impl.h`).

CMake globs all `.cc` files under `src/shared`, `src/engine`, `src/compiler`, `src/debugger` into a single static library (`pulse_app`), links it into both the `pulse` executable (`src/main.cc`) and the `pulse_tests` executable (every `.cc` inside any `tests/` directory), so a new `.cc`/`tests/*.cc` file only needs to exist on disk to be picked up — no CMakeLists.txt edits required for new source or test files, and a new compiler feature folder with `include/` or `lib/` is added to the include path automatically (re-run `cmake -B build`).