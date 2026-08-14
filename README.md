# Pulse

## Build

```powershell
# Configure (run once)
cmake -B build

# Compile all (app + tests)
cmake --build build
```

### Build Specific Target
```powershell
# App only
cmake --build build --target pulse

# Tests only
cmake --build build --target pulse_tests
```

## Run

### Main Application
```powershell
.\build\Debug\pulse.exe <input_file> [--cli]
```

### Run Tests
```powershell
# Direct output
.\build\Debug\pulse_tests.exe

# Or via CTest
ctest --test-dir build --output-on-failure
```

## Adding New Tests
Create any `*.cc` or `*.cpp` file inside a `tests/` folder (e.g. `core/engine/tests/signals.test.cc`). It will be auto-discovered on the next build.
