# Contributing to Pulse

Pulse is an open-source project, and all contributions are welcome! Whether you're fixing bugs, adding new features, or improving documentation, your help is appreciated.

Here are some guidelines to help you get started with contributing to Pulse:

## First Steps

### Prerequisites
Ensure you have a C++20 compatible compiler and CMake installed.

### Cloning the Repository

First, **fork** the repository on GitHub and clone your fork to your local machine.

```bash
git clone https://github.com/<YOUR_USERNAME>/pulse.git
cd pulse
```

### Configure and build the project

Start by creating a build directory and running CMake as an initial check to ensure everything is set up correctly. You can do this with the following commands:

```bash
cmake -B build
cmake --build build
```

Then, run the tests and the test VHDL project to ensure everything is working as expected:

```bash
# Run tests
./build/Debug/pulse_tests # or pulse_tests.exe on Windows

# Run test project
./build/Debug/pulse test-project # or pulse.exe test-project on Windows
```

## Project Structure

Inside the `src` directory, you'll find 3 main subdirectories:
- `debugger/`: Contains waveform-related code (recorder, TUI, etc.)
- `engine/`: Contains the core components used to run the simulation (wires, gates, subgraphs, etc.)
- `parser/`: Contains the whole compilation pipeline to convert VHDL code into a simulation graph.

Each of these directories implements its functionality in its own namespace.

Inside of each folder, every feature or module (e.g., `x`) is divided into three distinct layers:
* `include/x.h`: The public interface and data structures.
* `src/x.cc`: The internal logic and implementation details.
* `tests/x.test.cc`: The unit tests for the module. Tests are written using gtest.

> Additional internal interfaces may be created inside a `lib/` subdirectory.

> For single-header modules (either private or public) whose implementation is included at the end of the header file, implementation can be placed inside a `impl/` subdirectory. For example, `src/engine/include/blueprint.h` has its implementation in `src/engine/impl/blueprint_impl.h`.


## Contributing Guidelines

When contributing to this project, please follow these guidelines:

1. **Code Style**: Follow the existing code style and conventions.
2. **Documentation**: Add doc comments for new features (specially for public interfaces) and update existing documentation as needed.
3. **Testing**: Add unit tests for new features or bug fixes.
4. **Commit Messages**: Write clear and concise commit messages. No fixed format is required, but a good commit message should explain what has changed and why.
5. **Pull Requests**: Submit pull requests with a clear description of the changes.

## Contribution Areas

We welcome contributions across all layers of the simulation engine. Here are three major areas where you can help:

### 1. Extending the VHDL Compiler Pipeline

#### The Goal
VHDL is a huge language with many complex constructs. We aim to support as much of the syntax and semantics as possible, including advanced sequential statements, package declarations, and complex module hierarchies.

#### Where to Look
You can find parsing-related code inside `/src/parser/`.

#### How to Contribute
The compilation pipeline consists of several stages: lexing, parsing, semantic analysis, and linking. Every of these stages should be able to handle new VHDL constructs. Finally, you will need to find the best way the new constructs can be represented in the simulation graph when building the design blueprint.

### 2. Simulation Engine Optimization

#### The Goal
The simulation engine is the core of Pulse. Performance is critical, especially for large designs. Adding structural optimizations such as implementing higher-level components that abstract complex logic is highly valuable.

#### Where to Look
You can find the simulation engine code inside `/src/engine/`.

#### How to Contribute
Identify bottlenecks in the simulation engine and propose optimizations. This could involve improving the used data structures, implementing more efficient algorithms, or adding new components that can replace complex subgraphs with simpler equivalents. 

For that last case, make sure you not obfuscate developer-declared signals or, at least, provide a way to trace them back to the original signals so that the user can still inspect them in the waveform.


### 3. TUI Waveform Visualization Enhancements

#### The Goal
The terminal-based waveform visualization is a key feature of Pulse. Enhancing the user experience with interactive features will make it easier for users to analyze simulation results.

#### Where to Look
You can find the TUI code inside `/src/debugger/`.

#### How to Contribute
New interaction features, better navigation, and improved signal inspection capabilities are welcome. Consider adding features like zooming, filtering signals, or exporting waveforms to different formats.

> Any other contributions that improve the functionality, performance, or usability of Pulse are also very valuable.

## Reporting Issues

If you encounter any bugs or have feature requests, please open an issue on the GitHub repository. Provide as much detail as possible, including steps to reproduce the issue and any relevant logs or screenshots.