# Changelog

## 2.0.0
Sept 27, 2026

A new compiler: Pulse now reads a much larger subset of VHDL-2008, checks it like a real VHDL tool, and simulates designs with generics, integers, enumerations and numeric_std arithmetic. See "Supported Syntax" in the README for everything it accepts and simulates.

**Compiler**
- New VHDL-2008 tokenizer and parser, with located syntax errors for anything outside the supported subset.
- New semantic analyzer: types, scopes, overloading, subprogram rules, constant folding and the multi-driver rule are checked. Each file is analyzed into the `work` library, so an architecture can live in another file than its entity.
- New linker: every component instance is bound to the entity of the same name (the default binding of LRM 7.3.3), and the component is checked against it.
- New elaborator: the design is lowered from its top entity to logic components, with one blueprint per set of generic values, like C++ templates.
- Predeclared subset of `std_logic_1164` and `numeric_std`: logical operators, reductions, shifts, arithmetic and comparisons on `unsigned` and `signed`, `resize`, `to_integer`, `to_unsigned`, `to_signed`, `rising_edge` and `falling_edge`.

**Simulation**
- Integer types, enumerations, booleans, `unsigned` and `signed`, besides `std_logic` and `std_logic_vector`.
- Processes with variables, `case` and `case?`, unrolled `for` loops with `exit` and `next`, and `wait on`, `wait until` and `wait for`.
- Conditional and selected assignments (also inside processes), matching operators, aggregates, slices and type conversions.

**Command line and debugger**
- `--arch` now defaults to the most recently analyzed architecture of the top entity (it used to be `behavioral`), and every other entity uses its most recently analyzed architecture.
- New `-Ologic` option, the only simulation mode for now: `std_logic` as `0`, `1`, `X` and `Z`.
- Errors are printed the GCC way, with the file, line, column and stage, the source line and a caret, colored on a terminal (`NO_COLOR` turns colors off). Errors from linking and elaboration now name their file too.
- Options can come before or after the project folder; `--help` and `--version` work in any position; a missing option value or a too large `--end` is reported.
- The TUI shows every signal as its VHDL type says (vectors in hexadecimal and expandable into their bits, integers in decimal, enumerations by literal), and a single Esc now quits it on Linux and macOS.

**Breaking changes**
- Types are checked strictly, as the LRM says: for example, arithmetic on `std_logic_vector` needs `unsigned` or `signed` (`numeric_std`).
- Inside VHDL, the second is written `sec` (the LRM unit); `--end` keeps accepting `s`.
- `--arch` defaults to the most recently analyzed architecture instead of `behavioral`.

## 1.0.1
Sept 5, 2026

- Fixed a cmake problem that caused the build to fail on Unix systems.
- Unified the executable output path to be `build/bin/` on all platforms.

## 1.0.0 - Initial Release
Sept 1, 2026

- Basic simulation engine implemented.
- Terminal user interface (TUI) for waveform visualization.
- Support for VHDL parsing and compilation.
- Basic command line options for running simulations.
