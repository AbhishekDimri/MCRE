# MCRE - Magic Combat Rule Engine

A reusable C++ library that calculates the state of magical interactions.

A game supplies components (Fire, Water, ...), construction context and
attacker/defender state. MCRE turns them into **property state**, propagates it
through a validated **influence graph**, and returns an **explainable result**.
The game decides what that result means (damage, burning, knockback, terrain, ...).

> The game supplies context. MCRE calculates state. The game interprets the state.

## Status

**Early skeleton (v0.1, pre-alpha).** The build system, project layout, error
handling and the first model building blocks (core types, response curves,
aggregation) are in place. The engine itself is not wired together yet:
unfinished functions call `MCRE_TODO(...)`, which throws `mcre::NotImplementedError`.

| Area | State |
|---|---|
| CMake layout (library + app) | Done |
| `MCRE_TODO` / `NotImplementedError` | Done |
| Core types (`PropertyId`, `Bounds`, `InfluenceEdge`, enums) | Done |
| Response (mechanism) functions | Done, except `Custom` |
| Aggregation (sum, weighted mean, weighted median) | Done |
| Registries | Planned |
| Influence graph, SCC detection, stability gate | Planned |
| Solver, trace, builder API | Planned |
| Offense / defense resolver, constraints | Planned |

The first milestone is a small vertical slice: 4 properties (Temperature,
Viscosity, Pressure, Expansion), 2 components (Fire, Water), and one feedback loop
(Pressure <-> Expansion), producing a result with a full trace.

## Core idea

1. Elements are components that carry **property data**, not hard-coded damage.
2. Properties are **aggregated** from all components in a spell.
3. Properties **influence** each other through a sparse, validated graph.
4. Acyclic parts are evaluated once; cyclic parts (feedback loops) are solved
   iteratively, after passing a stability check at registration time.
5. Every meaningful value can be **traced** back to its inputs.
6. Offense and defense use the same machinery.

Combinations such as Fire + Water emerge from property interactions; there is no
`FireWaterRule`.

## What is implemented

All of these are header-only and live in `include/mcre/model/`.

**Core types** (`model.hpp`)

- `PropertyId`, `ComponentId` - numeric ids (`std::uint32_t`).
- `Bounds` - allowed range of a property (default `[-1, 1]`), and
  `ClampToBounds(value, bounds)`, which throws on NaN.
- `InfluenceEdge` - "source property affects target property": target id,
  mechanism, signed coefficient, `ResponseParams` (`k`, `threshold`), priority,
  and a mandatory human-readable reason.
- `property`, `component` - property definitions and component data.

**Response curves** (`response.hpp`) - `responseShape(mechanism, x, params)`
shapes ONE source value. It does not clamp; the caller multiplies by the edge
coefficient and clamps the result.

| Mechanism | Shape | Notes |
|---|---|---|
| `Linear` | `x` | |
| `Inverse` | `1 / (1 + k·abs(x))` | Shrinks as `x` grows; always finite |
| `Saturation` | `x / (1 + k·abs(x))` | Diminishing returns |
| `Exponential` | `exp(k·x) - 1` | 0 at `x = 0` |
| `Threshold` | `1 / (1 + exp(-k·(x - threshold)))` | Set `k` per edge; a large `k` (about 10) gives a sharp switch |
| `Sigmoid` | `2 / (1 + exp(-k·x)) - 1` | S-curve through 0, range (-1, 1) |
| `Custom` | - | Not implemented (`MCRE_TODO`) |

**Aggregation** (`aggregation.hpp`) - `aggregate(kind, contributions)` combines
every component's contribution to ONE property.

| Kind | Result | With no contributors |
|---|---|---|
| `Sum` | `Σ value·weight` | 0 |
| `WeightedMean` | `Σ value·weight / Σ weight` | 0 |
| `Median` | Weighted median: the value where the running weight first reaches half the total | 0 |

- Weights must be non-negative (they mean amount or concentration); a negative
  or NaN weight throws `std::invalid_argument`. Use the edge coefficient's sign,
  not the weight, to make something lower a property.
- Contributions are sorted by (component, value, weight) before summing, so
  results are bit-identical regardless of input order, even with duplicate
  component ids.

## Requirements

- CMake 3.16 or newer
- A C++20 compiler (developed with MSVC from Visual Studio)
- Git

## Build

### Visual Studio (open as a CMake folder)

1. **File > Open > Folder...** and select the repository root.
2. Wait for CMake configuration to finish (see the Output window).
3. Choose `mcre_app.exe` in the startup item dropdown.
4. Build with `Ctrl+Shift+B` and run with `Ctrl+F5`.

### Command line

```
cmake -S . -B build
cmake --build build
```

The location of the executable depends on the CMake generator. With a
single-configuration generator such as Ninja it is typically `build/app/mcre_app`.

With MSVC, run these from a **Developer PowerShell / Command Prompt for VS** so
that `cl.exe` is on the `PATH`.

### Visual Studio segment heap

When configured inside the Visual Studio environment, the top-level
`CMakeLists.txt` loads Visual Studio's optional `SegmentHeap.cmake`, which embeds
a faster Windows memory-allocator manifest into executables. Outside that
environment it is skipped, so configuring from a plain terminal or another IDE
still works. (It used to be set in `CMakePresets.json`, which broke configuration
whenever `VSINSTALLDIR` was empty.)

### Troubleshooting

| Error | Cause and fix |
|---|---|
| `CMAKE_PROJECT_TOP_LEVEL_INCLUDES file does not exist: Common7/...` | A stale value in the CMake cache from older presets. In Visual Studio: **Project > Delete Cache and Reconfigure**. |
| `The CMAKE_CXX_COMPILER: cl.exe is not a full path and was not found in the PATH` | Visual Studio did not load the MSVC environment. Close Visual Studio, delete the `.vs/` and `out/` folders, and reopen the folder. If it persists, check **View > Output > CMake**, or start Visual Studio from *Developer PowerShell for VS* with `devenv <repo path>`. |

## What currently runs

`app/main.cpp` is a smoke test: it calls `MCRE_TODO("Hello")` inside a `try` block
and prints the resulting message, for example:

```
NOT IMPLEMENTED : Hello in function main at <path>/app/main.cpp:12
```

It then waits for Enter (`cin.get()`) so the console window stays open.

## Project layout

```
MCRE/
|- CMakeLists.txt        Top-level: project setup, adds the sub-folders
|- CMakePresets.json     Visual Studio / CMake presets
|- include/mcre/         Public headers (what a game includes)
|  |- core/              Shared basics: errors.hpp (MCRE_TODO, NotImplementedError)
|  `- model/             model.hpp (core types), response.hpp, aggregation.hpp
|- src/                  Engine implementation, built as static library `mcre`
|  `- core/
|- app/                  Demo executable `mcre_app` (contains only main)
`- docs/                 Design notes and amendments (planned)
```

Targets:

- `mcre` - static library with all engine code. Public include path and the C++20
  requirement propagate to anything that links to it.
- `mcre_app` - executable that links `mcre`.
- A test executable will be added under `tests/` and will also link `mcre`.

## Conventions

- All code lives in `namespace mcre`.
- Public headers use `#pragma once`, the `.hpp` extension, and are included as
  `#include "mcre/<module>/<file>.hpp"`.
- Library code does not print. Unfinished work throws via `MCRE_TODO(...)`.
- Every new `.cpp` file is added to the `add_library(mcre ...)` list in
  `src/CMakeLists.txt`.
- Build output (`out/`, `build/`, `.vs/`) is git-ignored.

## Design documentation

The initial design document (MCRE v1.0 Design Document) is the baseline. It is a
living document: where implementation diverges from it, the change and the reason
are recorded in `docs/DESIGN_AMENDMENTS.md`.

