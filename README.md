# MCRE - Magic Combat Rule Engine

A reusable C++ library that calculates the state of magical interactions.

A game supplies components (Fire, Water, ...), construction context and
attacker/defender state. MCRE turns them into **property state**, propagates it
through a validated **influence graph**, and returns an **explainable result**.
The game decides what that result means (damage, burning, knockback, terrain, ...).

> The game supplies context. MCRE calculates state. The game interprets the state.

## Status

**Early prototype (pre-alpha).** The build system, error handling, logging, the
registries, the JSON loader, the built-in prototype data, the demo app and the first
unit tests are in place. The calculation itself is not wired together yet: there is no
influence graph, solver or trace, and unfinished functions call `MCRE_TODO(...)`, which
throws `mcre::NotImplementedError`.

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

## The three kinds of data (a plain-language picture)

The engine keeps three different kinds of information. Mixing them up is the most
common source of confusion.

| Picture | In the code | What it holds | Changes while a spell is calculated? |
|---|---|---|---|
| A **dictionary** (or the column headings of a form) | `PropertyRegistry` | For each property: id, name, allowed range, how contributions combine, which properties it influences | Never |
| A shelf of **ingredient cards** | `ComponentRegistry`, `component` | Each ingredient's own values (Fire: Temperature 0.95) | Never |
| The **scoreboard** of one spell | `PropertyState` | The current value of every property for this one spell | Yes, step by step |

The dictionary says what Temperature *is* (id 1, range -1 to 1). A card says how hot
*Fire* is (0.95). The scoreboard says how hot *this spell* is (0.5333 after mixing
2 parts Fire and 1 part Water). Fire must still be 0.95 in the next spell, which is
why results are never written back into a card. In one sentence: the card is the
definition of an ingredient, and the scoreboard is the result of one spell.

A small example with plain `std::map` and `std::vector` (it uses none of MCRE's
classes; it uses the same numbers as section 1.6 of the design document):

```cpp
struct Info { int id; double lower; double upper; };

// THE DICTIONARY: facts about each property. Written once, never changed.
std::map<std::string, Info> registry = {
    {"Temperature", {1, -1.0, 1.0}},
    {"Viscosity",   {2,  0.0, 1.0}},
};

// THE INGREDIENT CARDS: fixed recipes, values in property-id order.
std::vector<double> fire  = { 0.95, 0.05 };   // Temperature, Viscosity
std::vector<double> water = {-0.30, 0.78};

// THE SCOREBOARD of ONE spell: 2 parts Fire + 1 part Water.
std::vector<double> state(2);
for (std::size_t i = 0; i < state.size(); ++i)
    state[i] = (2 * fire[i] + 1 * water[i]) / 3;      // step 1: mix (a blend)

// step 2: Temperature pushes Viscosity (Linear, c = -0.50), then clamp
// to the range that the dictionary gives for Viscosity.
int T = registry["Temperature"].id - 1;               // name -> id -> slot
int V = registry["Viscosity"].id - 1;
state[V] = std::clamp(state[V] - 0.50 * state[T],
                      registry["Viscosity"].lower, registry["Viscosity"].upper);
```

After mixing the scoreboard holds `T=0.5333  V=0.2933`; after the push
`T=0.5333  V=0.0267`. The `fire` and `water` cards are unchanged.

Reading one number goes name -> id -> position -> value:
`"Temperature"` -> id 1 (the dictionary) -> slot 0 (id - 1) -> `0.5333`.

Another thing worth knowing: **empty is not zero**. Fire has no Pressure value, so on
the scoreboard Pressure is an *empty* slot (`std::optional<double>`), not `0`.

Section 1.6 of the design document explains the three kinds of data with this example,
section 7.9 shows them on the full Fire + Water example, and section 9.4 shows the
classes.

### File, namespace and class

In `mcre::PropertyRegistry` these are three different things:

| Name | What it is |
|---|---|
| `propertyRegistry.hpp` / `.cpp` | A **file**. It is named after the class it contains. |
| `mcre` | The **namespace**: a shared prefix so names never clash with other libraries or the game. |
| `PropertyRegistry` | A **class**: a type that owns data and can be created many times (every test makes a fresh one). |

If something remembers data, it is a class (`PropertyRegistry`, `ComponentRegistry`,
`PropertyState`). If it only does things, it is a namespace of functions
(`mcre::builtin`, `mcre::logger`).

## What is implemented

Everything is in `include/mcre/` (headers) and `src/` (code for the library `mcre`).

**Core types** (`model.hpp`)

- `PropertyId`, `ComponentId` - numeric ids (`std::uint32_t`). The registries assign
  them (1, 2, 3, ... in the order things are added); 0 is never a real id.
- `Bounds` - allowed range of a property (default `[-1, 1]`), and
  `ClampToBounds(value, bounds)`, which throws on NaN.
- `InfluenceEdge` - "source property affects target property": target id,
  mechanism, signed coefficient, `ResponseParams` (`k`, `threshold`), priority,
  and a mandatory human-readable reason.
- `property`, `component` - property definitions and component data.

**Property registry** (`propertyRegistry.hpp`) - `PropertyRegistry`

- `add(name, bounds, aggregation)` returns the new id. Rejects an empty or duplicate
  name and bounds that are not finite or have `lower > upper`.
- `addInfluence(source, edge)` records an influence. Rejects an unknown source or
  target, a property influencing itself, an empty reason, non-finite numbers, a
  negative `k`, the `Custom` mechanism, an `Exponential` that would overflow, and a
  second edge with the same source, target and mechanism.
- `byId`, `byName` (a pointer, or `nullptr`), `idOf` (0 when missing), `size`, `all`,
  `dump`.

**Component registry** (`componentRegistry.hpp`) - `ComponentRegistry`

- `add(component, properties)` checks the component against the `PropertyRegistry`
  (non-empty unique name, every property id exists, every value finite and inside its
  bounds), assigns the id and returns it. An id set by the caller is ignored.
- `byId`, `byName`, `idOf`, `size`, `all`, `dump`.

**Property state** (`propertyState.hpp`) - `PropertyState`

- One slot per property, each either empty or holding a finite number.
- `get(id)` returns a `std::optional<double>`, `valueOr(id, fallback)`, `has(id)`,
  `set(id, value)` (throws for an id without a slot or a value that is not finite),
  `presentCount`, `size`, `dump`.
- The header is written; the implementation is in progress.

**Response curves** (`response.hpp`) - `responseShape(mechanism, x, params)` shapes
ONE source value. It does not clamp; the caller multiplies by the edge coefficient
and clamps the result.

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

**JSON loader** (`loader.hpp`) - see the next section.

**Built-in prototype data** (`builtin.hpp`) - `mcre::builtin::addPrototypeProperties`
adds Temperature, Viscosity, Pressure and Expansion (ids 1 to 4, all with bounds
`[0, 1]`). `addPrototypeComponents` adds Fire and Water through the normal loader.
No influence edges are registered yet. These values are the older v1.0 ones: design
document v1.1 uses signed ranges for Temperature and Expansion, sums Pressure and gives
Water a Temperature of -0.30. Which side to change is an open question (design
document section 10.4, C-8).

### Using the registries

```cpp
#include <iostream>
#include <utility>

#include "mcre/model/builtin.hpp"
#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/loader.hpp"
#include "mcre/model/propertyRegistry.hpp"

mcre::PropertyRegistry properties;
mcre::builtin::addPrototypeProperties(properties);              // Temperature 1, Viscosity 2, Pressure 3, Expansion 4

mcre::ComponentRegistry components;
mcre::builtin::addPrototypeComponents(components, properties);  // Fire 1, Water 2

const mcre::component* fire = components.byName("Fire");        // nullptr if there is no such component
double t = fire->properties.at(properties.idOf("Temperature")); // 0.95

// Your own file:
mcre::LoadResult loaded = mcre::loadComponentsFromFile("components.json", properties);
if (!loaded.ok()) {
    for (const mcre::Diagnostic& d : loaded.problems) std::cerr << d.where << "  ->  " << d.message << "\n";
} else {
    for (mcre::component& c : loaded.components) components.add(std::move(c), properties);
}
```

## Component files (JSON)

```json
{
  "schemaVersion": 1,
  "components": [
    { "name": "Fire", "version": "0.1.0",
      "properties": { "Temperature": 0.95, "Viscosity": 0.05, "Expansion": 0.72 } }
  ]
}
```

| Rule | Behaviour |
|---|---|
| Names, not ids | Files use property names. The loader turns them into ids through the `PropertyRegistry`. |
| Strict keys | Top level: only `$schema`, `schemaVersion` (must be `1`) and `components`. Inside a component: only `name`, `version` and `properties`. Anything else is reported, which catches typos. |
| Out of range is rejected | A value outside its property's bounds is a problem. It is never clamped silently. |
| Missing means absent | A property that a component does not list is simply absent. It is not stored as 0. |
| All or nothing | If any problem is found, no components are returned. |
| Problems are returned | `LoadResult::problems` holds every problem found, each with an address such as `fire.json#/components/0/properties/Temperature` and a message such as `value 1.7 is outside [0, 1]`. Bad data in a file never throws. |
| Ids come later | The loader leaves `componentId` at 0. `ComponentRegistry::add` assigns it. |

Known limitation: if the same key appears twice in one JSON object, the last value
wins and nothing is reported (a test documents this).

Only `src/model/loader.cpp` includes the JSON library (nlohmann_json).

## Error handling and logging

| Situation | What happens |
|---|---|
| A mistake in a user's data file | Returned as problems in `LoadResult`; never thrown |
| A wrong call by the programmer (empty name, duplicate name, unknown id, ...) | `std::invalid_argument`, and the reason is logged |
| A bug inside the library (the built-in data does not load) | `std::logic_error` |
| A feature that is not written yet | `MCRE_TODO(...)` throws `mcre::NotImplementedError` |

`mcre::logger` appends one line per message to `mcre.log` in the working directory
(already git-ignored). The first time it opens the file it prints one line to
standard error, `[MCRE] Logging to file ...`. Logging never throws.

## Requirements

- CMake 3.16 or newer
- A C++20 compiler (developed with MSVC from Visual Studio)
- Git
- Internet access the first time CMake configures: it downloads nlohmann_json
  (v3.12.0) and GoogleTest (v1.17.0)

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
ctest --test-dir build -C Debug --output-on-failure
```

`--test-dir` needs CMake 3.20 or newer; with an older CMake, run `ctest` from inside
`build/`. To skip building the tests (and the GoogleTest download), configure with
`-DMCRE_BUILD_TESTS=OFF`.

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
| `LNK2019: unresolved external symbol ...` for a function you just wrote | The new `.cpp` file is not in the source list in `src/CMakeLists.txt`, so it was never compiled. Add it. |
| A new test does not run | Add the test file to `tests/CMakeLists.txt`. |

## Tests

Unit tests use GoogleTest and live in `tests/`. Run them from Visual Studio's Test
Explorer or with `ctest` (see Build).

| File | Tests | Covers |
|---|---|---|
| `propertyRegistryTests.cpp` | 15 | adding properties, lookups, influence edges and the checks on them |
| `componentRegistryTests.cpp` | 14 | adding components, id assignment, the checks, lookups |
| `loaderTests.cpp` | 26 | good files, every kind of bad file, reading from disk |
| `builtinTests.cpp` | 7 | the prototype properties and components |

Aggregation, the response curves and `PropertyState` do not have tests yet.

## What currently runs

`mcre_app` is a small command-line demo. It keeps `main()` thin: read the arguments,
call the library, print, return an exit code.

```
mcre_app                   show the built-in prototype data (4 properties, Fire and Water)
mcre_app components.json   load a file against the prototype properties and show the result
mcre_app --help            show the usage text
```

Exit code: `0` success, `1` the file had problems or something went wrong, `2` wrong
usage. Results go to standard output; problems and errors go to standard error.

Running it with no arguments prints:

```
PropertyRegistry: 4 properties
  id  name                   lower   upper  aggregation   edges
   1  Temperature                0       1  WeightedMean      0
   2  Viscosity                  0       1  WeightedMean      0
   3  Pressure                   0       1  WeightedMean      0
   4  Expansion                  0       1  WeightedMean      0
ComponentRegistry: 2 components
  #1 Fire (version 0.1.0)
        Temperature = 0.95
        Viscosity = 0.05
        Expansion = 0.72
  #2 Water (version 0.1.0)
        Temperature = 0.2
        Viscosity = 0.78
        Expansion = 0.12
```

## Project layout

```
MCRE/
|- CMakeLists.txt        Top-level: project setup, third-party libraries, adds the sub-folders
|- CMakePresets.json     Visual Studio / CMake presets
|- include/mcre/         Public headers (what a game includes)
|  |- core/              errors.hpp (MCRE_TODO, NotImplementedError), logger.hpp
|  `- model/             model.hpp, response.hpp, aggregation.hpp,
|                        propertyRegistry.hpp, componentRegistry.hpp, propertyState.hpp,
|                        loader.hpp, builtin.hpp
|- src/                  Engine implementation, built as the static library `mcre`
|  |- core/              errors.cpp
|  `- model/             propertyRegistry.cpp, componentRegistry.cpp, loader.cpp, builtin.cpp
|- app/                  Demo executable `mcre_app` (contains only main)
`- tests/                GoogleTest unit tests, built as `mcre_tests`
```

Targets:

- `mcre` - static library with all engine code. Public include path and the C++20
  requirement propagate to anything that links to it. nlohmann_json is linked
  privately.
- `mcre_app` - executable that links `mcre`.
- `mcre_tests` - GoogleTest executable that links `mcre` (built unless
  `MCRE_BUILD_TESTS` is `OFF`).

## Conventions

- All code lives in `namespace mcre`; groups of free functions use a nested namespace
  (`mcre::builtin`, `mcre::logger`).
- File names are camelCase (`propertyRegistry.hpp`). Public headers use
  `#pragma once`, the `.hpp` extension, and are included as
  `#include "mcre/<module>/<file>.hpp"`.
- Files and tests refer to properties by **name**; the engine works with **ids**.
- Library code does not print results. `dump()` functions write to a stream the
  caller passes in; messages go to the log file.
- Every new `.cpp` file is added to the `add_library(mcre ...)` list in
  `src/CMakeLists.txt`, and every new test file to `add_executable(mcre_tests ...)`
  in `tests/CMakeLists.txt`.
- Build output (`out/`, `build/`, `.vs/`) and log files are git-ignored.

## Design documentation

The design is described in **MCRE Design Document v1.1** (PDF, first written
6 October 2026, updated 20 October 2026). Where it is silent, the v1.0 document still
applies. Good places to start:

| Section | What it gives you |
|---|---|
| 1.6 and 7.9 | The dictionary, the ingredient cards and the scoreboard, with a worked example |
| 9 | The code as it is built: files, types, the JSON format, the checks, errors and logging, the tests |
| 10.4 | Every place where the code differs from the document, and what to do about it |
| 12 | The roadmap and the current status |

When the code and the document disagree again, add a row to section 10.4 of the
document and update this README.
