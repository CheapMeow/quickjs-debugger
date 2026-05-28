# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A DAP (Debug Adapter Protocol) debugger for quickjs-ng embedded in C++ applications. The end goal is to enable JetBrains Rider to simultaneously debug C++ and JavaScript — setting breakpoints in JS files, inspecting JS stack frames/variables, and stepping through JS execution.

The hard part is the **QuickJS Debug Core** (VM pause, stack frames, variables, breakpoints, stepping), not DAP. A CLI debugger should be built first to validate the core before implementing the DAP adapter.

## Build

Two generator workflows:

**Visual Studio (for Rider / MSVC):**
```bat
build_debug.bat
```
Or manually:
```bat
cmake -S . -B build/ -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/ --parallel 8 --config Debug
```

**Ninja (for clangd / compile_commands.json):**
```shell
cmake -S . -B build-clangd/ -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-clangd/
```

The `compile_commands.json` symlink at the project root points to `build-clangd/compile_commands.json`.

## Run

The Debugger executable is a test harness, not a final application:
```shell
./build/src/debugger/Debug/Debugger.exe       # MSVC
./build-clangd/src/debugger/Debugger            # Ninja
```

It creates a quickjs runtime, runs an infinite JS loop, pauses after 2 seconds, prints the location, then resumes. There are no project-level tests yet.

## Architecture

```
Rider IDE
  ↓
DAP (JSON-RPC)          ← not yet built
  ↓
QuickJS Debug Adapter   ← not yet built
  ↓
QuickJS Debug Core      ← partially built (Phase 1.1)
  ↓
quickjs-ng VM           ← forked, patched
```

### Source layout

- `src/3rdparty/quickjs-ng/` — Git submodule, fork of quickjs-ng on branch `debugger`. Contains custom patches marked with `// [Debugger Begin]` / `// [Debugger End]` comments.
- `src/debugger/` — The debugger project. Builds `Debugger` executable, links against `qjs` library.

### Key files in `src/debugger/`

- [Debugger.h](src/debugger/Debugger.h) / [Debugger.cpp](src/debugger/Debugger.cpp) — Pause/Resume state machine using `std::atomic<bool>` for signaling + `std::mutex`/`std::condition_variable` for VM suspension.
- [QuickJSDebug.h](src/debugger/QuickJSDebug.h) / [QuickJSDebug.cpp](src/debugger/QuickJSDebug.cpp) — `InstallQuickJSDebugger()` registers the interrupt handler via `JS_SetInterruptHandler`. The handler calls `JS_GetCurrentLocation()`, prints a stack trace via `new Error().stack`, then suspends the VM. Uses a global `JSContext*` pointer.
- [main.cpp](src/debugger/main.cpp) — Test harness: creates runtime/context, spawns JS thread, demonstrates pause/resume.

### Custom quickjs-ng API

[quickjs-debugger.h](src/3rdparty/quickjs-ng/quickjs-debugger.h) declares the custom public API:

```c
typedef struct JSDebugLocation {
    JSAtom filename;
    int line;
    int col;
} JSDebugLocation;

int JS_GetCurrentLocation(JSContext *ctx, JSDebugLocation *out_loc);
```

Implementation is in `quickjs.c` (gated by `// [Debugger Begin]` comments). It walks `current_stack_frame`, finds the first bytecode function frame, maps PC to line via the internal `find_line_num()`.

### Interrupt handler constraints (from roadmap.md)

The interrupt handler runs inside the VM execution loop. Do NOT:
- Allocate memory
- Eval JS
- Create exceptions
- Call complex APIs

The handler should only: read state, decide whether to pause.

### Build targets (quickjs-ng submodule)

| Target | Description |
|--------|-------------|
| `qjs` | Core library (what `Debugger` links to) |
| `qjs-libc` | Standard library module |
| `qjs_exe` | CLI `qjs` executable |
| `qjsc` | Bytecode compiler |
| `api-test` | API test |
| `run-test262` | Test262 runner |

## Roadmap

The project follows a 5-phase plan detailed in [roadmap.md](roadmap.md):

1. **VM Debug Core** — `JS_GetCurrentLocation` (done), stack frames, breakpoints, variables, exceptions, stepping
2. **CLI Debugger** — GDB-style interface to validate the core before adding DAP complexity
3. **DAP Adapter** — JSON-RPC over stdio, minimal DAP command set (initialize, launch, setBreakpoints, threads, stackTrace, scopes, variables, continue, pause, next, stepIn, stepOut, disconnect)
4. **Rider Integration** — Connect Rider to the DAP adapter
5. **Advanced** — Mixed C++/JS stack, source maps, async stack, live edit, heap/profiler

Recommended order: current location → stack frames → breakpoints → variables → exceptions → stepping → CLI debugger → DAP adapter → Rider.
