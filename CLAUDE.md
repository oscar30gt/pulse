# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Pulse is a multi-platform digital logic simulation engine for VHDL, written in C++20. It compiles VHDL source into a logic-component simulation graph, runs a dataflow simulation, and displays the resulting waveform in an interactive terminal UI. `./build/bin/pulse <project_dir> [--top e] [--arch a] [--end 100fs] [-Ologic]` runs the whole pipeline and reports the first error of a design GCC-style (`file:line:column: <stage> error: message`, the source line and a caret). VHDL support is intentionally partial (see "Supported Syntax" in README.md) and being extended incrementally — check README.md before assuming a construct is supported.

## Build & Test Commands

Implementing a feature should be accompanied by unit tests in the corresponding `tests/` directory.
Use the following commands to build and run the project tests:

```bash
# Configure and build
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

`src/` has three top-level areas, each in its own namespace, wired together in `src/main.cc`, plus `src/shared/` (namespace `Pulse`: the header-only `types.h` and `checked_math.h` every area uses):

- **`compiler/`** (`Pulse::Parser`) — VHDL source to the `Blueprint`s of a design. Pipeline order: **Tokenizer → Parser (AST) → Semantic Analyzer → Linker → Elaborator**, like a real VHDL tool. Each file is tokenized and parsed independently into an `ASTRoot`. The files are then analyzed one at a time into a `DesignLibrary` (the `work` library), in the order `analysisOrder()` gives: every file comes after the files that declare the entities of its architectures, and the design units of a file are analyzed in textual order (LRM 13.1), so an entity comes before its architectures in a file. An architecture finds its entity in the library (LRM 13.5), which is how an architecture can live in another file than its entity; names are unique in the library. Only then does `Linker::link()` bind every component instance to the entity with the same name (the default binding of LRM 7.3.3: generics and ports by name, same types, compatible port modes, defaults for what the component leaves out, using the types the library resolved) and merge the files into one linked design, entities first. Analysis never looks at which entity a component stands for; the linker does. The tokenizer accepts the VHDL-2008 lexical grammar except extended identifiers, and the parser builds trees for the VHDL-2008 subset listed in the README (it is *blind*: syntax and the grammar of each region, such as no signal or component declarations inside a process, but never types or scopes; it throws `ast_syntax_error` at the first error, and anything outside its grammar is a syntax error). The analyzer has a handler for every node kind the parser can build, so there is no "not supported yet" layer: the few corners it does not model (aggregates and slices of multi-dimensional arrays, named elements in aggregate targets, aggregate targets of selected assignments, access types) say so in their error message. Very long chains of operators, selections or alternatives are refused (parser: more than 2000 links, or nesting of parentheses, statements and subprograms deeper than 100; analyzer: expressions more than 250 levels deep, so in practice a chain of about 250 operators), and the AST frees such chains iteratively, so no input can exhaust the stack.
  - Every token carries the index of its source file (`Tokenizer(source, file)`), and the parser copies it into every node's `SourceLocation`, so the diagnostics of every stage, linking and elaboration included, name their file. `SourceLocation{}` (line 0) means no location: errors about the design as a whole. `compiler_error::stage()` names the stage ("lexical", "syntax", "semantic", "link", "elaboration"), and `formatDiagnostic()` (`diagnostics/diagnostics.cc`) prints an error GCC-style for `main.cc`.
  - The prelude (`analyzer/prelude.cc`) is VHDL parsed once and shared by every library: the STANDARD types Pulse needs (`boolean`, `severity_level`, `integer`, `natural`, `positive`, `real`, `time`; not `bit`, `character` or `string`), `std_logic` and the `numeric_std` vectors, plus the subprograms of `std_logic_1164` and `numeric_std` as **builtins**: declarations without a body (`SubprogramInfo::builtin`). Operators resolve design-declared overloads first, then builtins by the types of the already-typed operands (`typeOfBuiltinOperator`, so operator chains stay linear), then the LRM's implicit operators (`OperatorRules`, which also keeps the explanations of IEEE failures). A builtin's result length follows its package (`builtinOperatorResult`, `builtinCallResult`).
  - `DesignLibrary` also answers what the elaborator needs: `typeOf(expr)` (the type of every analyzed expression), `declarationOf(name)` (the declaration an object name refers to), `calleeOf(node)` (the builtin or design subprogram a call or operator resolved to), `objectType(decl)`, `predefinedType(name)`, and `entity`/`architecture`/`latestArchitecture` (analysis order, for the LRM 7.3.3 default binding).
  - The **elaborator** (`elaborator/`, `elaborate()`) starts at the top entity and lowers each architecture to a `Blueprint` of logic components (`DesignElaborator` and `UnitElaborator` in `elaborator.cc`, the latter split into `layout.cc`, `evaluator.cc`, `expressions.cc`, `builtins.cc`, `concurrent.cc`, `processes.cc`, `instances.cc`, `drivers.cc`, `symbols.cc`; internal headers `lib/elaborator_internal.h` and `lib/instruction_builder.h`). Generics work like C++ templates: one blueprint per distinct set of static generic values (`DesignElaborator::specialize`). Only -Ologic exists for now (`LogicMode::Logic`: std_logic as the engine's 01XZ); `LogicMode::Vector` (nine-valued) is declared but rejected. Types become wire layouts (std_logic, boolean, enumerations, integers as wide as their base type, 1-D std_logic vectors of up to 64 bits); values known at elaboration time are folded by its own evaluator (generics, constants, loop parameters); processes become process-box instructions, with expressions lowered to combinational components outside them; `for` loops are unrolled. Constructs it does not lower throw `elaboration_error` "... is not supported yet": that is expected, and every rejection a design can reach has a test (the few the analyzer makes unreachable stay as defensive fallbacks).
- **`engine/`** (`Pulse::Engine`) — the simulation core. A `Blueprint` describes a component's physical circuit as a graph of `ComponentInstance`s (gates, comparators, adders, processes, event probes, nested subgraphs, etc.) connected by named wires with default values, plus a `SymbolTable` saying how each signal is displayed; `Subgraph` instantiates a functional, runnable version of a `Blueprint` for dataflow simulation with four-valued 01XZ logic (`LogicVector`, two 64-bit words) and asynchronous processes. The root `Subgraph(bp)` owns the wires of the top's ports, and `tick()` simulates one tick (one femtosecond, one VHDL delta cycle) in steps over the whole hierarchy: every `EventProbe` latches its input (an event means the input differs from its value at the previous tick; never on the first tick), then every probe publishes its 1-bit output, then every component updates (processes run), then processes `commit()` their deferred signal assignments. Wires know nothing about time: `'event` lives in `EventProbe` only, and process boxes run on **triggers**, 1-bit wires that are the probe outputs of their sensitivity list or `wait on` signals (the elaborator's `triggersOf`). Processes assign with `Wire::drive` (ignored while the wire has sources): immediately for variables, deferred for signals.
- **`debugger/`** (`Pulse::Debugger`) — consumes the simulated waveform and renders it in an interactive terminal UI (`tui_viewer.cc`/`tui_render.cc`), letting users step through time and inspect signal values. Each wave takes its display format from the snapshot's symbol table (`formatValue`): logic vectors in hex and expandable into bits, integers in decimal, enumerations by literal.

The compiled AST is not executed directly — the elaborator lowers it into `Blueprint`s, `Subgraph` runs them, and the waveform recorder feeds the debugger.

### Module layout convention

Every feature module `x` inside `engine/` or `debugger/` follows a fixed three-layer split (see CONTRIBUTING.md):

- `include/x.h` — public interface and data structures.
- `x.cc`, at the root of the module (or several files for a large module, e.g. `debugger/tui_viewer.cc` and `debugger/tui_render.cc` for `tui.h`) — implementation.
- `tests/x.test.cc` — GoogleTest unit tests.

Existing exceptions: `logicVector.h`, `miniSet.h` and `symbolTable.h` are header-only; `blueprint.h` keeps its implementation in `impl/`; `adder`, `subtractor` and `multiplicator` share `tests/arithmetic.test.cc`; `tests/propagation.test.cc` tests whole graphs, and `blueprint.h` and `symbolTable.h` are only tested through the subgraph and elaborator tests.

Additional conventions:
- `lib/` subdirectories hold internal-only headers not meant for consumers outside the module.
- `compiler/` is organised the same way but one level deeper: each pipeline stage is its own feature folder, `compiler/<feature>/{include,lib,tests,*.cc}`, with the features `diagnostics` (the error types and the GCC-style formatter), `tokenizer`, `parser`, `ast`, `analyzer`, `linker` and `elaborator`. The `tokenizer`, `parser`, `ast` and `analyzer` features name their source and test files by grammar area or responsibility without the redundant feature prefix (`parser/expressions.cc`, `tokenizer/numbers.cc`, `ast/printer.cc`, `analyzer/choices.cc`, `analyzer/tests/types.test.cc`; a few older test files keep it, such as `ast/tests/ast_clone.test.cc` and `tokenizer/tests/tokenizer_edge.test.cc`); a feature's main file keeps the feature name (`analyzer/analyzer.cc`, `linker/linker.cc`, `elaborator/elaborator.cc`), and so do internal headers whose bare name would be ambiguous on the shared include path (`parser_impl.h`, `analyzer_internal.h`, `analyzer_scope.h`). Includes stay bare (`#include "ast.h"`): CMake adds every feature's `include/` (public) and `lib/` (internal, private to `pulse_app`, so tests cannot include them) directory to the include path. Test helpers shared by all compiler features live in `compiler/tests/test_helpers.h`; `TestUtil::Simulation` compiles, elaborates and simulates design sources for end-to-end tests (each source is file index 0, 1, ... in its locations), and `compiler/tests/` also holds the tests that cross stages (`file_locations.test.cc`).
- `impl/` subdirectories hold implementation for header-only/templated types whose definition lives at the end of the public header (e.g. `engine/include/blueprint.h` pulls in `engine/impl/blueprint_impl.h`).

CMake globs all `.cc` files under `src/shared`, `src/engine`, `src/compiler`, `src/debugger` into a single static library (`pulse_app`), links it into both the `pulse` executable (`src/main.cc`) and the `pulse_tests` executable (every `.cc` inside any `tests/` directory), so a new `.cc`/`tests/*.cc` file only needs to exist on disk to be picked up — no CMakeLists.txt edits required for new source or test files, and a new compiler feature folder with `include/` or `lib/` is added to the include path automatically (re-run `cmake -B build`).