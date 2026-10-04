# MCRE - Magic Combat Rule Engine

A reusable C++ library that calculates the state of magical interactions.

A game supplies components (Fire, Water, ...), construction context and
attacker/defender state. MCRE turns them into **property state**, propagates it
through a validated **influence graph**, and returns an **explainable result**.
The game decides what that result means (damage, burning, knockback, terrain, ...).

> The game supplies context. MCRE calculates state. The game interprets the state.

## Status

**Early skeleton (v0.1, pre-alpha).** The build system, project layout and error
handling are in place. The engine itself is not implemented yet: unfinished
functions call `MCRE_TODO(...)`, which throws `mcre::NotImplementedError`.

| Area | State |
|---|---|
| CMake layout (library + app) | Done |
| `MCRE_TODO` / `NotImplementedError` | Done |
| Core types (`PropertyId`, `Bounds`, enums) | In progress |
| Registries, aggregation | Planned |
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
`FireWaterRule`. See `docs/` for the full design.

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

## What currently runs

`app/main.cpp` is a smoke test: it calls `MCRE_TODO("Hello")` inside a `try` block
and prints the resulting message, for example:

```
NOT IMPLEMENTED : Hello in function main at <path>/app/main.cpp:12
```

## Project layout

```
MCRE/
|- CMakeLists.txt        Top-level: project setup, adds the sub-folders
|- CMakePresets.json     Visual Studio / CMake presets
|- include/mcre/         Public headers (what a game includes)
|  |- core/              Shared basics, e.g. errors.hpp
|  `- model/             Model types, e.g. model.hpp
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

## Roadmap

1. Core types and `clampTo`
2. Response (mechanism) functions
3. Property and component registries
4. Aggregation (sum, weighted mean)
5. Evaluation of acyclic properties
6. Damped fixed-point solver for the Pressure/Expansion loop
7. Tarjan SCC detection, scheduling, and the stability gate
8. Trace output, builder API (`combine`, `amplify`, `concentrate`)
9. Offense / defense resolver, constraints, regression suite

## License

Not yet chosen.
