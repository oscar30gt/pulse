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

- **`compiler/`** (`Pulse::Parser`) — VHDL source to a single design object called `Blueprint`. Pipeline order: **Tokenizer → Parser (AST) → Semantic Analyzer → Linker**, like a real VHDL tool. Each file is tokenized and parsed independently into an `ASTRoot`. The files are then analyzed one at a time into a `DesignLibrary` (the `work` library), in the order `analysisOrder()` gives: every file comes after the files that declare the entities of its architectures, and the design units of a file are analyzed in textual order (LRM 13.1), so an entity comes before its architectures in a file. An architecture finds its entity in the library (LRM 13.5), which is how an architecture can live in another file than its entity; names are unique in the library. Only then does `Linker::link()` bind every component instance to the entity with the same name (the default binding of LRM 7.3.3: generics and ports by name, same types, compatible port modes, defaults for what the component leaves out, using the types the library resolved) and merge the files into one linked design, entities first. Analysis never looks at which entity a component stands for; the linker does. The tokenizer accepts the VHDL-2008 lexical grammar and the parser builds trees for the VHDL-2008 subset listed in the README (it is *blind*: syntax and the grammar of each region, such as no declarations inside a process, but never types or scopes; it throws `ast_syntax_error` at the first error, and anything outside its grammar is a syntax error). The analyzer has a handler for every node kind the parser can build, so there is no "not supported yet" layer: the few corners it does not model (aggregates and slices of multi-dimensional arrays) say so in their error message. Very long chains of operators, selections or alternatives are refused (parser: more than 2000 links; analyzer: more than 250 nested levels), and the AST frees such chains iteratively, so no input can exhaust the stack.
- **`engine/`** (`Pulse::Engine`) — the simulation core. A `Blueprint` describes a component's physical circuit as a graph of `ComponentInstance`s (gates, comparators, adders, processes, nested subgraphs, etc.) connected by named wires; `Subgraph` instantiates a functional, runnable version of a `Blueprint` for dataflow simulation with IEEE 1164 multi-valued logic (`LogicVector`) and asynchronous processes.
- **`debugger/`** (`Pulse::Debugger`) — consumes the simulated waveform and renders it in an interactive terminal UI (`tui_viewer.cc`/`tui_render.cc`), letting users step through time and inspect signal values.

The compiled AST is not executed directly — it still needs to be lowered into `Blueprint`/`Subgraph` objects before simulation, and the waveform recorder feeds the debugger.

### Module layout convention

Every feature module `x` inside `engine/` or `debugger/` follows a fixed three-layer split (see CONTRIBUTING.md):

- `include/x.h` — public interface and data structures.
- `x.cc` (or multiple files for large modules, e.g. `parser/expressions.cc`, `analyzer/type_defs.cc`) — implementation.
- `tests/x.test.cc` — GoogleTest unit tests.

Additional conventions:
- `lib/` subdirectories hold internal-only headers not meant for consumers outside the module.
- `compiler/` is organised the same way but one level deeper: each pipeline stage is its own feature folder, `compiler/<feature>/{include,lib,tests,*.cc}`, with the features `diagnostics` (headers only), `tokenizer`, `parser`, `ast`, `analyzer` and `linker`. The `tokenizer`, `parser`, `ast` and `analyzer` features name their source and test files by grammar area or responsibility without the redundant feature prefix (`parser/expressions.cc`, `tokenizer/numbers.cc`, `ast/printer.cc`, `analyzer/choices.cc`, `analyzer/tests/types.test.cc`); a feature's main file keeps the feature name (`analyzer/analyzer.cc`, `linker/linker.cc`), and so do internal headers whose bare name would be ambiguous on the shared include path (`parser_impl.h`, `analyzer_internal.h`, `analyzer_scope.h`). Includes stay bare (`#include "ast.h"`): CMake adds every feature's `include/` (public) and `lib/` (internal) directory to the include path. Test helpers shared by all compiler features live in `compiler/tests/test_helpers.h`.
- `impl/` subdirectories hold implementation for header-only/templated types whose definition lives at the end of the public header (e.g. `engine/include/blueprint.h` pulls in `engine/impl/blueprint_impl.h`).

CMake globs all `.cc` files under `src/shared`, `src/engine`, `src/compiler`, `src/debugger` into a single static library (`pulse_app`), links it into both the `pulse` executable (`src/main.cc`) and the `pulse_tests` executable (every `.cc` inside any `tests/` directory), so a new `.cc`/`tests/*.cc` file only needs to exist on disk to be picked up — no CMakeLists.txt edits required for new source or test files, and a new compiler feature folder with `include/` or `lib/` is added to the include path automatically (re-run `cmake -B build`).