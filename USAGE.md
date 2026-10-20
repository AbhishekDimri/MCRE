# MCRE Usage Guide

How to use the Magic Combat Rule Engine from your own code: what to set up, every option, what comes back, and what can go wrong.

This guide describes the code as of **21 October 2026**. Every code example in it was compiled and run, and the output shown under each one is the real output. For the design reasoning behind the numbers, see the *MCRE Design Document v1.1*; for the build setup, see the README.

## Contents

1. [What MCRE does](#1-what-mcre-does)
2. [Quick start](#2-quick-start)
3. [Build, run and test](#3-build-run-and-test)
4. [Concepts and vocabulary](#4-concepts-and-vocabulary)
5. [Step by step: every option](#5-step-by-step-every-option)
6. [How a spell is calculated](#6-how-a-spell-is-calculated)
7. [Built-in data](#7-built-in-data)
8. [Errors, warnings and problems](#8-errors-warnings-and-problems)
9. [Recipes](#9-recipes)
10. [Limits and what is not done yet](#10-limits-and-what-is-not-done-yet)
11. [Troubleshooting](#11-troubleshooting)
12. [API cheat sheet](#12-api-cheat-sheet)

---

## 1. What MCRE does

You describe a magic spell as a list of ingredients (for example "2 parts Fire, 1 part Water"). MCRE turns it into a **property sheet**: a number for every property in your model (Temperature, Pressure, Expansion, ...). Your game decides what those numbers mean (damage, burning, knockback). MCRE never mentions damage.

> The game supplies context. MCRE calculates state. The game interprets the state.

Three lists make up a model:

| List | Picture | Code | Changes during a spell? |
|---|---|---|---|
| **Dictionary** | The column headings of a form | `PropertyRegistry` | Never |
| **Ingredient cards** | One card per ingredient, with its own numbers | `ComponentRegistry`, `component` | Never |
| **Scoreboard** | The result of one spell | `PropertyState` (inside a `Result`) | Yes |

The calculation, in one picture:

```
 Spell                        Dictionary + cards
 (2 Fire, 1 Water,            (ranges, mixing rules,       build once
  amplify, concentrate)        influences, card values)  ------------+
        |                                                             v
        |  1. mix the cards into one base value per property     Engine (never changes)
        +-------------------------------------------------------->  |
           2. add the pushes of the influences, in a safe order      |
           3. settle feedback loops by repeating                     |
           4. clamp every value into its range                       v
                                                              Result: scoreboard + trace + warnings
```

Properties can push each other ("heat thins liquids"). Some pushes run in a circle (pressure pushes expansion, expansion pushes pressure). The engine finds these loops when you build it, refuses a loop that would run out of control, and settles the safe ones by repeating the calculation until the numbers stop changing.

---

## 2. Quick start

Include the headers, build the engine once, then cast as many spells as you like.

```cpp
#include <iomanip>
#include <iostream>
#include <utility>

#include "mcre/model/builtin.hpp"
#include "mcre/model/engine.hpp"
#include "mcre/model/spellBuilder.hpp"
#include "mcre/model/stateNames.hpp"

int main() {
	using namespace mcre;

	// 1. The dictionary (properties and their influences) and the ingredient cards (components).
	PropertyRegistry properties;
	builtin::addDemoProperties(properties);
	ComponentRegistry components;
	builtin::addDemoComponents(components, properties);

	// 2. Build the engine once. It checks the model and works out the order and the loops.
	BuildResult built = Engine::build(std::move(properties), std::move(components));
	if (!built.ok()) {
		for (const Diagnostic& d : built.problems) std::cerr << d.where << ": " << d.message << "\n";
		return 1;
	}
	for (const Diagnostic& d : built.warnings) std::cout << "warning: " << d.where << ": " << d.message << "\n";
	const Engine& engine = *built.engine;

	// 3. Cast: 2 Fire + 1 Water, a little hotter, and concentrated.
	const Result result = SpellBuilder(engine)
		.combine("Fire", 2)
		.combine("Water")
		.amplify("Temperature", 1.2)
		.concentrate(0.8)
		.cast();

	// 4. Read the result by name.
	std::cout << std::fixed << std::setprecision(4);
	for (const auto& [name, value] : toNamedValues(result.state, engine.properties())) {
		std::cout << std::setw(14) << name << "  " << std::setw(7) << value << "\n";
	}
	std::cout << "converged: " << std::boolalpha << result.converged
		<< ", warnings: " << result.warnings.size() << "\n";
}
```

Output:

```text
   Temperature   0.6400
       Density   0.1433
     Viscosity   0.0000
      Pressure   0.7706
    Volatility   0.7733
     Expansion   0.8999
   Compression   0.2667
 PhaseAffinity   1.0000
converged: true, warnings: 0
```

What each step did:

1. `addDemoProperties` and `addDemoComponents` fill the dictionary and the ingredient cards with the built-in demo model (section 7). For your own model, you fill them yourself (section 5).
2. `Engine::build` checks the model and works out the evaluation order. Do this **once**, not once per spell. Check `ok()` before using the engine.
3. `SpellBuilder` turns names into a `Spell` and `cast()` evaluates it. Fire at amount 2 and Water at the default amount 1 are mixed 2 : 1; Temperature is multiplied by 1.2 before mixing; `concentrate(0.8)` gives the summed properties 1.25 times as much weight.
4. `toNamedValues` gives `(name, value)` pairs. **Read results by name.**

The same cast without the builder, as plain data (this is what the builder produces):

```cpp
#include <iostream>
#include <utility>

#include "mcre/model/builtin.hpp"
#include "mcre/model/engine.hpp"

int main() {
	using namespace mcre;

	PropertyRegistry properties;
	builtin::addDemoProperties(properties);
	ComponentRegistry components;
	builtin::addDemoComponents(components, properties);
	BuildResult built = Engine::build(std::move(properties), std::move(components));
	const Engine& engine = *built.engine;

	// The same spell as in the quick start, written as plain data with ids instead of names.
	Spell spell;
	spell.ingredients = {
		{ engine.components().idOf("Fire"), 2.0 },
		{ engine.components().idOf("Water"), 1.0 },
	};
	spell.amplify = { { engine.properties().idOf("Temperature"), 1.2 } };
	spell.energyBudget = 1.0 / 0.8;   // what concentrate(0.8) sets

	const Result result = engine.evaluate(spell);   // throws std::invalid_argument for a bad spell

	const PropertyId pressure = engine.properties().idOf("Pressure");
	std::cout << "Pressure = " << result.state.get(pressure).value() << "\n";
}
```

Output:

```text
Pressure = 0.770636
```

---

## 3. Build, run and test

**Requirements:** CMake 3.16 or newer, a C++20 compiler (developed with MSVC from Visual Studio 2022), Git, and internet access the first time CMake configures (it downloads nlohmann_json 3.12.0 and GoogleTest 1.17.0).

| Task | How |
|---|---|
| Open in Visual Studio | **File > Open > Folder...** on the repository root, wait for CMake to configure, pick `mcre_app.exe`, `Ctrl+Shift+B` to build, `Ctrl+F5` to run |
| Command line | `cmake -S . -B build`, then `cmake --build build` |
| Run all tests | `ctest --test-dir build -C Debug --output-on-failure` (CMake 3.20+), or Visual Studio's Test Explorer |
| Skip the tests and the GoogleTest download | Configure with `-DMCRE_BUILD_TESTS=OFF` |

**Targets**

| Target | What it is |
|---|---|
| `mcre` | The static library with all engine code. Linking to it gives you the include path and the C++20 requirement automatically. |
| `mcre_app` | A small command-line demo (below). |
| `mcre_tests` | The GoogleTest executable. |

**Using MCRE from another CMake project**

```cmake
add_subdirectory(path/to/MCRE)          # configure with -DMCRE_BUILD_TESTS=OFF to skip the tests
target_link_libraries(my_game PRIVATE mcre)
```

Then `#include "mcre/model/engine.hpp"` and so on. All code is in `namespace mcre`.

**Compiler flags used for the library:** `/W4 /permissive-` for MSVC, `-Wall -Wextra -Wpedantic` for GCC and Clang. The library is expected to build without warnings.

**The demo app**

```text
mcre_app                   show the built-in prototype data (4 properties, Fire and Water)
mcre_app components.json   load a components file against the prototype properties and show it
mcre_app --help            show the usage text
```

Exit code `0` = success, `1` = the file had problems or something failed, `2` = wrong usage. It does not cast spells yet; for that, use the code in section 2.

**The log file.** Every registration, every registry and loader error, and every accepted influence is appended to `mcre.log` in the working directory. The first time the file opens, one line `[MCRE] Logging to file ...` is printed to standard error. Logging never throws, and the log path is not configurable yet.

---

## 4. Concepts and vocabulary

| Term | Meaning | In code |
|---|---|---|
| **Property** | One measurable quality: Temperature, Pressure, ... It has an id, a name, a range, and a mixing rule. | `property`, `PropertyRegistry::add` |
| **Bounds** | The allowed range of a property, for example -1 to 1. Every calculated value is forced into it ("clamped"). | `Bounds{lower, upper}` |
| **Aggregation** | How the values of several ingredients combine into one: `Sum`, `WeightedMean` or `Median`. | `AggregationKind` |
| **Component** | An ingredient *card*: Fire is 0.95 hot, Water is -0.30 hot. A card is the definition of an ingredient. It never changes. | `component`, `ComponentRegistry` |
| **Ingredient** | One line of a spell: "this component, this much of it". "2 Fire" is `{ fireId, 2.0 }`. The same component may be listed twice. | `Ingredient{component, amount}` |
| **Amplify** | "Make this property stronger or weaker." Every ingredient's value for that property is multiplied by a factor before mixing. Factor 0 switches the property off; a negative factor is rejected. | `Amplify{property, factor}` |
| **Energy budget** | A number that scales the weight of ingredients in *summed* properties only (for example Pressure). `concentrate(c)` sets it to `1 / c`. | `Spell::energyBudget` |
| **Spell** | Everything needed for one cast: the ingredients, the amplify list and the energy budget. It holds ids, so never save one. | `Spell` |
| **Influence** | "This property pushes that property": a source, a target, a curve, a strength, and a written reason. | `InfluenceEdge` |
| **Mechanism** | The curve of an influence: Linear, Inverse, Saturation, Exponential, Threshold or Sigmoid. | `Mechanism` |
| **Push** | What one influence added to a property in one calculation. | `Push` |
| **Base** | A property's value after mixing, before any pushes. | `TraceEntry::base` |
| **Loop** | A group of properties that push each other in a circle. Settled by repeating. | `EvalStep::isLoop` |
| **Engine** | The finished model: dictionary, cards, evaluation order, loops. Built once, never changes. | `Engine` |
| **Result** | What a cast returns: the scoreboard, the trace, warnings, and whether the loops settled. | `Result` |
| **Scoreboard** | One value per property for one spell. A slot is either empty or holds a number (empty is not 0). | `PropertyState` |
| **Trace** | The story of how each value was made: base, every push, raw sum, final value. | `std::vector<TraceEntry>` |
| **Warning** | Something worth knowing that is not an error, for example "a loop did not settle". | `Warning{where, message}` |
| **Problem** | A fault in *data* (a bad file, a loop that would blow up). Reported in a list, never thrown. | `Diagnostic{where, message}` |
| **Id** | A property's or component's number (1, 2, 3, ... in the order added). **0 is never a valid id.** Ids exist only in memory. | `PropertyId`, `ComponentId` |

**Ids versus names.** Inside a running program you use ids because they are fast. Anything that is **saved, sent over a network, or written to a log that someone reads later must use names**, because ids change when a property is added or reordered. `toNamedValues` and `fromNamedValues` convert between the two (section 5.7).

**Component versus ingredient.** A component is the card on the shelf (Fire, with its numbers). An ingredient is one line of a spell that points at a card and says how much. Casting never changes a card.

---

## 5. Step by step: every option

### 5.1 Properties (the dictionary)

```cpp
PropertyId add(std::string_view name, Bounds bounds,
               AggregationKind aggregation = AggregationKind::WeightedMean);
```

| Option | Values | Notes |
|---|---|---|
| `name` | Any non-empty text, unique | Files and games refer to the property by this name. |
| `bounds` | `{ lower, upper }`, both finite, `lower <= upper` | Signed ranges such as `{ -1, 1 }` let opposites share one axis (cold to hot). |
| `aggregation` | `WeightedMean` (default), `Sum`, `Median` | See the table below. |

The returned id is 1 for the first property, 2 for the second, and so on. A bad call throws `std::invalid_argument` and logs the reason.

**Aggregation kinds.** Each ingredient contributes a value (the card's number, times any amplify factor) and a weight.

| Kind | Result | Weight of an ingredient | Use it for |
|---|---|---|---|
| `WeightedMean` | `sum(value * weight) / sum(weights)`; 0 if nobody contributes | its amount | Qualities that blend: Temperature, Viscosity |
| `Sum` | `sum(value * weight)` | its amount times the energy budget | Quantities that add up: Pressure, Force |
| `Median` | The weighted lower median: sort by value, take the value where the running weight first reaches half the total | its amount | Qualities where an outlier should not dominate |

An ingredient whose card has **no value** for a property does not take part for that property at all. It does not count as 0, so it does not dilute the mean. A property that no ingredient has comes out as 0, then clamped into its range (so a property with range 0.2 to 0.9 and no contributors ends at 0.2).

### 5.2 Influences (who pushes whom)

Add influences **after** all properties exist, because an influence names both ends.

```cpp
void PropertyRegistry::addInfluence(PropertyId source, const InfluenceEdge& edge);
```

| `InfluenceEdge` field | Default | Meaning |
|---|---|---|
| `target` | 0 | The property that gets pushed. Must exist. |
| `mechanism` | `Linear` | The curve (table below). |
| `coefficient` | 0 | The strength. Positive raises the target, negative lowers it. Must be finite. |
| `params.k` | 1.0 | Strength or steepness of the curve. Finite and not negative. |
| `params.threshold` | 0.5 | Where `Threshold` switches on. Finite. |
| `priority` | 0 | Order in which pushes into one target are added up and listed in the trace (lower first). |
| `reason` | empty | **Mandatory**, non-empty: why this influence exists. |

What a push adds to the target is `coefficient * shape(x)`, where `x` is the **final (clamped) value of the source** and `shape` is the curve:

| Mechanism | `shape(x)` | Uses | Typical use |
|---|---|---|---|
| `Linear` | `x` | nothing | Straight proportion |
| `Inverse` | `1 / (1 + k * abs(x))` | `k` | Shrinks as x grows |
| `Saturation` | `x / (1 + k * abs(x))` | `k` | Diminishing returns |
| `Exponential` | `exp(k * x) - 1` | `k` | Escalation; 0 at x = 0 |
| `Threshold` | `1 / (1 + exp(-k * (x - threshold)))` | `k`, `threshold` | A smooth switch; use a large k such as 10 |
| `Sigmoid` | `2 / (1 + exp(-k * x)) - 1` | `k` | S-curve through 0, between -1 and 1; k = 4 is typical |
| `Custom` | not written yet | | **Rejected** by `addInfluence` for now |

**Rules checked by `addInfluence`** (a violation throws `std::invalid_argument`, after logging):

- the source and the target exist, and they are different properties (no self-influence)
- the reason is not empty
- `coefficient` and `threshold` are finite; `k` is finite and `>= 0`
- the mechanism is not `Custom`
- for `Exponential`: `k * max(abs(source bounds))` must not exceed 700 (otherwise `exp` would overflow)
- there is no second influence with the **same mechanism** between the same source and target (so at most six parallel influences between a pair)

**About priority.** Mathematically the order of adding pushes does not change the sum. Priority decides the order of the pushes in the trace and fixes the order of the floating-point additions so the result is the same on every run. Ties are broken by lower source id, then by the influence's position in its source. In practice you can leave priority at 0.

A model with a loop, built from scratch:

```cpp
#include <iomanip>
#include <iostream>
#include <utility>

#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/engine.hpp"
#include "mcre/model/propertyRegistry.hpp"
#include "mcre/model/spellBuilder.hpp"

int main() {
	using namespace mcre;

	// ---- the dictionary: three properties, each with a range and a way of mixing
	PropertyRegistry properties;
	const PropertyId heat     = properties.add("Heat",     { -1.0, 1.0 });                        // weighted mean
	const PropertyId charge   = properties.add("Charge",   {  0.0, 1.0 });
	const PropertyId pressure = properties.add("Pressure", {  0.0, 1.0 }, AggregationKind::Sum);   // adds up

	// ---- the influences (added after all properties exist, because they name both ends)
	InfluenceEdge edge;
	edge.target = pressure;                      // Heat pushes Pressure
	edge.mechanism = Mechanism::Linear;
	edge.coefficient = 0.4;
	edge.reason = "Hot air raises pressure";     // mandatory
	properties.addInfluence(heat, edge);

	edge = {};                                   // Pressure pushes Charge, with diminishing returns
	edge.target = charge;
	edge.mechanism = Mechanism::Saturation;
	edge.coefficient = 0.5;
	edge.params.k = 2.0;
	edge.reason = "Squeezed gas builds static";
	properties.addInfluence(pressure, edge);

	edge = {};                                   // Charge pushes Pressure back: a loop
	edge.target = pressure;
	edge.coefficient = 0.2;
	edge.reason = "Charged gas pushes itself apart";
	properties.addInfluence(charge, edge);

	// ---- the ingredient cards: what each component is worth for each property
	ComponentRegistry components;
	component spark;
	spark.componentName = "Spark";
	spark.version = "1.0.0";
	spark.properties = { { heat, 0.9 }, { charge, 0.8 }, { pressure, 0.05 } };
	components.add(spark, properties);

	component cloud;
	cloud.componentName = "Cloud";
	cloud.version = "1.0.0";
	cloud.properties = { { heat, -0.2 }, { charge, 0.1 }, { pressure, 0.10 } };
	components.add(cloud, properties);

	// ---- build and cast
	BuildResult built = Engine::build(std::move(properties), std::move(components));
	if (!built.ok()) return 1;
	const Engine& engine = *built.engine;

	const Result result = SpellBuilder(engine).combine("Spark").combine("Cloud", 2).cast();

	std::cout << std::fixed << std::setprecision(4);
	for (const property& p : engine.properties().all()) {
		std::cout << std::setw(9) << p.propertyName << "  " << result.state.get(p.propertyId).value() << "\n";
	}
	for (const EvalStep& step : engine.order()) {
		std::cout << (step.isLoop ? "loop:" : "single:");
		for (PropertyId id : step.properties) std::cout << " " << engine.properties().byId(id)->propertyName;
		std::cout << "\n";
	}
}
```

Output:

```text
     Heat  0.1667
   Charge  0.4453
 Pressure  0.4057
single: Heat
loop: Charge Pressure
```

Heat is worked out first and alone (nothing pushes it). Charge and Pressure push each other, so they form one loop and are settled together.

### 5.3 Components (the ingredient cards)

In code:

```cpp
ComponentId ComponentRegistry::add(component c, const PropertyRegistry& properties);
```

Set `componentName`, optionally `version`, and `properties` (a map from property **id** to value). `add` ignores any `componentId` you set and assigns 1, 2, 3, ... It throws `std::invalid_argument` when the name is empty or already used, a property id does not exist, or a value is not finite or lies outside its property's bounds. A card does not have to list every property.

**From a JSON file**

```json
{
  "schemaVersion": 1,
  "components": [
    { "name": "Fire", "version": "0.1.0",
      "properties": { "Temperature": 0.95, "Viscosity": 0.05, "Expansion": 0.72 } }
  ]
}
```

```cpp
LoadResult loadComponentsFromFile(const std::filesystem::path& file, const PropertyRegistry& properties);
LoadResult loadComponentsFromText(std::string_view jsonText, std::string_view sourceName,
                                  const PropertyRegistry& properties);
```

| Rule | Behaviour |
|---|---|
| Names, not ids | Files use property names. The loader turns them into ids. |
| Strict keys | Top level: only `$schema`, `schemaVersion` (must be the number `1`) and `components`. A component: only `name`, `version` and `properties`. Anything else is a problem, which catches typos. |
| Required | `schemaVersion`, `components`, and for each component `name` (non-empty, unique in the file) and `properties`. `version` is optional text. |
| Out of range is rejected | A value outside its property's bounds is a problem. It is never clamped silently. |
| Missing means absent | A property the card does not list is absent, not 0. |
| All or nothing | If any problem is found, **no** components are returned. |
| Problems are returned | `LoadResult::problems` lists every problem with an address and a message. Bad data in a file never throws. |
| Ids come later | Loaded components have `componentId` 0; `ComponentRegistry::add` assigns it. |

```cpp
#include <iostream>
#include <string_view>
#include <utility>

#include "mcre/model/builtin.hpp"
#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/loader.hpp"
#include "mcre/model/propertyRegistry.hpp"

int main() {
	using namespace mcre;

	PropertyRegistry properties;
	builtin::addDemoProperties(properties);

	constexpr std::string_view good = R"json({
		"schemaVersion": 1,
		"components": [
			{ "name": "Ice", "version": "0.1.0",
			  "properties": { "Temperature": -0.9, "Viscosity": 0.6, "Compression": 0.7 } }
		]
	})json";

	constexpr std::string_view bad = R"json({
		"schemaVersion": 1,
		"components": [
			{ "name": "Mud", "propertys": {}, "properties": { "Temperature": 1.7, "Weight": 0.5 } }
		]
	})json";

	LoadResult ok = loadComponentsFromText(good, "ice.json", properties);
	std::cout << "ice.json: " << (ok.ok() ? "ok" : "problems") << ", " << ok.components.size() << " component(s)\n";

	LoadResult broken = loadComponentsFromText(bad, "mud.json", properties);
	for (const Diagnostic& d : broken.problems) std::cout << "  " << d.where << "  ->  " << d.message << "\n";
	std::cout << "components kept from mud.json: " << broken.components.size() << "\n";

	// Loaded components arrive with componentId 0; add() checks them again and gives the real id.
	ComponentRegistry components;
	for (component& c : ok.components) components.add(std::move(c), properties);
	std::cout << "Ice has id " << components.idOf("Ice") << "\n";
}
```

Output:

```text
ice.json: ok, 1 component(s)
  mud.json#/components/0/propertys  ->  unknown key
  mud.json#/components/0/properties/Temperature  ->  value 1.7 is outside [-1, 1]
  mud.json#/components/0/properties/Weight  ->  unknown property
components kept from mud.json: 0
Ice has id 1
```

### 5.4 Building the engine

```cpp
static BuildResult Engine::build(PropertyRegistry properties, ComponentRegistry components);
```

The registries are moved in; the engine keeps its own copy and **never changes afterwards**.

| `BuildResult` member | Meaning |
|---|---|
| `engine` | `std::optional<Engine>`. Empty when there are problems. |
| `problems` | `std::vector<Diagnostic>`. The model cannot be used. |
| `warnings` | `std::vector<Diagnostic>`. The model works, but look at these. |
| `ok()` | `true` when the engine exists and there are no problems. Warnings do not affect it. |

**What `build` does**

1. Refuses a model with no properties (a problem).
2. Lists, for every property, the influences that push it (`incoming`).
3. Finds the loops and puts the properties in a safe order (section 6.4).
4. Runs the **stability check** on every loop (section 6.6):
   - growth rate **1.0 or more**: a problem; no engine is built
   - growth rate **0.9 or more**: a warning; the engine is built, but the loop settles slowly and may fail to settle at the extremes

```cpp
#include <iostream>
#include <stdexcept>
#include <utility>

#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/engine.hpp"
#include "mcre/model/propertyRegistry.hpp"
#include "mcre/model/spellBuilder.hpp"

int main() {
	using namespace mcre;

	// A loop whose pushes are far too strong (each is 3.0): the engine refuses to build.
	PropertyRegistry properties;
	const PropertyId a = properties.add("A", { -1.0, 1.0 });
	const PropertyId b = properties.add("B", { -1.0, 1.0 });
	InfluenceEdge edge;
	edge.coefficient = 3.0;
	edge.reason = "too strong";
	edge.target = b;
	properties.addInfluence(a, edge);
	edge.target = a;
	properties.addInfluence(b, edge);

	ComponentRegistry components;
	component x;
	x.componentName = "X";
	x.properties = { { a, 0.05 }, { b, 0.0 } };
	components.add(x, properties);

	BuildResult built = Engine::build(std::move(properties), std::move(components));
	std::cout << "ok: " << std::boolalpha << built.ok() << ", engine built: " << built.engine.has_value() << "\n";
	for (const Diagnostic& d : built.problems) std::cout << d.where << ": " << d.message << "\n";

	// Programming mistakes throw std::invalid_argument:
	PropertyRegistry p2;
	try {
		p2.add("Temperature", { 1.0, -1.0 });   // lower bound above upper bound
	}
	catch (const std::invalid_argument& e) {
		std::cout << "invalid_argument: " << e.what() << "\n";
	}
}
```

Output:

```text
ok: false, engine built: false
loop A, B: growth rate 2.400000 is 1 or more: the loop would grow out of control instead of settling
invalid_argument: PropertyRegistry::add: bounds must be finite numbers with lower <= upper
```

**What you can ask an engine**

| Call | Returns |
|---|---|
| `engine.properties()` | The `PropertyRegistry` (look up ids and names, bounds, influences). |
| `engine.components()` | The `ComponentRegistry`. |
| `engine.order()` | The evaluation steps: each is one property or one loop, in the order they are worked out. |
| `engine.incoming(id)` | Who pushes this property, in the order the pushes are added up. Empty for an id with no slot. |
| `engine.evaluate(spell)` | The `Result` of a spell. |

### 5.5 Casting a spell

There are two ways: the **builder** (names, convenient) and the **`Spell` struct** (ids, plain data). Both end in `engine.evaluate`.

**`SpellBuilder`** (`#include "mcre/model/spellBuilder.hpp"`)

```cpp
SpellBuilder builder(engine);    // keeps a pointer: the engine must outlive the builder
```

| Call | What it does | Options and limits |
|---|---|---|
| `combine(name, amount = 1.0)` | Adds an ingredient by component name. | The component must exist. `amount` is finite and above 0. The same component may be added twice (equal to adding the sum of the amounts). |
| `amplify(property, factor)` | Multiplies every ingredient's value for the property before mixing. | `factor` is finite and not negative; 0 is allowed. Calling it again for the same property **replaces** the factor. An **unknown property name is ignored** and `cast()` adds a warning (`where` is `"amplify"`). |
| `concentrate(c)` | Sets the energy budget to `1 / c`. | `c` is finite and above 0. `c = 1` changes nothing, `c < 1` gives summed properties more weight (`0.8` gives 1.25 times), `c > 1` gives them less. |
| `build()` | Returns the finished `Spell`. | Throws if the spell has no ingredients. |
| `cast()` | `build()` followed by `evaluate`, plus the warnings for unknown amplify names. | |

Every call returns the builder, so calls chain. The order of the calls does not matter, except that ingredients are kept in the order added. A bad name or number throws `std::invalid_argument` naming the culprit, and a failed call leaves the builder as it was.

**`Spell`** (what the builder produces)

| Field | Default | Rules (checked by `validateSpell`, which `evaluate` calls) |
|---|---|---|
| `ingredients` | empty | At least one. Every component id exists. Every amount is finite and above 0. |
| `amplify` | empty | Every factor is finite and not negative. At most one entry per property. A property id that does not exist is **not** an error: it is ignored, with a warning in the result. |
| `energyBudget` | 1.0 | Finite and above 0. Only summed properties use it. |

`validateSpell` and `evaluate` throw `std::invalid_argument` for a bad spell; the reason is logged.

### 5.6 Reading the result

```cpp
struct Result {
    PropertyState state;               // the scoreboard
    std::vector<TraceEntry> trace;     // how each value was made
    std::vector<Warning> warnings;     // things worth knowing
    bool converged = true;             // false when a loop did not settle
};
```

**Every property has a value** in `result.state` after an evaluation. A property that nobody had is 0, forced into its range.

**`PropertyState`**

| Call | Returns |
|---|---|
| `get(id)` | `std::optional<double>`: the value, or empty. |
| `valueOr(id, fallback)` | The value, or `fallback` when empty. |
| `has(id)` | `true` when the slot holds a value. `false` also for an id with no slot (0 or too large). |
| `set(id, value)` | Stores a value. Throws `std::invalid_argument` for an id with no slot or a value that is not finite. |
| `size()` / `presentCount()` | Number of slots / how many hold a value. |
| `dump(out, properties)` | Prints every property by name with its value or `(none)`. |
| `==` | Equal when both have the same slots and values. |

A state **never clamps by itself**: whoever computes a value clamps it first. Empty is not 0.

**The trace.** One `TraceEntry` per property, in the order the engine worked.

| Field | Meaning |
|---|---|
| `property` | The property id. |
| `base` | The mixed value (after amplify), forced into the range. |
| `pushes` | Every influence that reached this property: `source`, `mechanism`, `coefficient`, `input` (the source's value), `output` (`coefficient * shape(input)`). |
| `raw` | `base` plus all pushes, **before** clamping. A raw value of 1.17 with a final value of 1.0 tells you the property was pushed out of range, which the final value alone would hide. |
| `clamped` | The final value, the one in `result.state`. |
| `repeat` | 0 for a property that is not in a loop; otherwise how many repeats its loop took. |

For a property in a loop the entry describes the **final values**: `pushes` are what reached it at those values, `raw` is what it wanted, and `clamped` is where it ended.

```cpp
#include <iomanip>
#include <iostream>
#include <utility>

#include "mcre/model/builtin.hpp"
#include "mcre/model/engine.hpp"
#include "mcre/model/spellBuilder.hpp"

namespace {
	const char* mechanismName(mcre::Mechanism m) {
		switch (m) {
		case mcre::Mechanism::Linear: return "Linear";
		case mcre::Mechanism::Inverse: return "Inverse";
		case mcre::Mechanism::Saturation: return "Saturation";
		case mcre::Mechanism::Exponential: return "Exponential";
		case mcre::Mechanism::Threshold: return "Threshold";
		case mcre::Mechanism::Sigmoid: return "Sigmoid";
		case mcre::Mechanism::Custom: return "Custom";
		}
		return "?";
	}
}

int main() {
	using namespace mcre;

	PropertyRegistry properties;
	builtin::addDemoProperties(properties);
	ComponentRegistry components;
	builtin::addDemoComponents(components, properties);
	BuildResult built = Engine::build(std::move(properties), std::move(components));
	const Engine& engine = *built.engine;

	const Result result = SpellBuilder(engine).combine("Earth", 3).cast();

	std::cout << std::fixed << std::setprecision(4);
	for (const TraceEntry& t : result.trace) {
		if (t.property != engine.properties().idOf("Density") && t.property != engine.properties().idOf("Pressure")) continue;

		std::cout << engine.properties().byId(t.property)->propertyName
			<< ": base " << t.base << ", raw " << t.raw << ", final " << t.clamped
			<< (t.repeat > 0 ? ", loop repeats " + std::to_string(t.repeat) : "") << "\n";
		for (const Push& p : t.pushes) {
			std::cout << "    from " << engine.properties().byId(p.source)->propertyName
				<< " (" << mechanismName(p.mechanism) << " x" << p.coefficient << "): input "
				<< p.input << " -> " << p.output << "\n";
		}
	}
}
```

Output:

```text
Density: base 0.9500, raw 1.1723, final 1.0000
    from PhaseAffinity (Linear x-0.2500): input -0.8892 -> 0.2223
Pressure: base 0.1800, raw 0.3759, final 0.3759, loop repeats 33
    from Temperature (Linear x0.2500): input 0.1000 -> 0.0250
    from Density (Linear x0.2000): input 1.0000 -> 0.2000
    from Volatility (Exponential x0.0400): input 0.0550 -> 0.0047
    from Expansion (Linear x0.3000): input -0.4126 -> -0.1238
    from Compression (Linear x0.1000): input 0.9000 -> 0.0900
```

Density wanted 1.1723 but its range stops at 1.0, so the trace shows `raw` 1.1723 and `final` 1.0000. Pressure is in the loop with Expansion and took 33 repeats.

**Warnings** (`result.warnings`; each has `where` and `message`)

| `where` | When |
|---|---|
| `"amplify"` | An amplify entry named a property id (or, via the builder, a name) that does not exist. It was ignored. |
| `"loop <names>"`, for example `"loop Pressure, Expansion"` | A loop did not settle within 1000 repeats. `converged` is `false` and the values are the last ones. Not an exception. |

Always check `result.converged` (or `warnings.empty()`) if your game needs trustworthy numbers.

**Inspecting the model** (these functions write to a stream you pass in; library code never prints by itself):

```cpp
#include <iostream>
#include <utility>

#include "mcre/model/builtin.hpp"
#include "mcre/model/engine.hpp"
#include "mcre/model/spellBuilder.hpp"

int main() {
	using namespace mcre;

	PropertyRegistry properties;
	builtin::addDemoProperties(properties);
	ComponentRegistry components;
	builtin::addDemoComponents(components, properties);

	properties.dump(std::cout);   // the dictionary: id, name, range, mixing rule, number of influences

	BuildResult built = Engine::build(std::move(properties), std::move(components));
	const Engine& engine = *built.engine;

	std::cout << "\nevaluation order:\n";
	for (const EvalStep& step : engine.order()) {
		std::cout << (step.isLoop ? "  loop  " : "  single");
		for (PropertyId id : step.properties) std::cout << " " << engine.properties().byId(id)->propertyName;
		std::cout << "\n";
	}

	std::cout << "\nwho pushes Pressure (in the order they are added up):\n";
	for (const Incoming& in : engine.incoming(engine.properties().idOf("Pressure"))) {
		const property& source = *engine.properties().byId(in.source);
		std::cout << "  " << source.propertyName << " (influence #" << in.edge << " of " << source.propertyName << ")\n";
	}

	std::cout << "\nthe scoreboard of 2 Fire + 1 Water:\n";
	SpellBuilder(engine).combine("Fire", 2).combine("Water").cast().state.dump(std::cout, engine.properties());
}
```

Output:

```text
PropertyRegistry: 8 properties
  id  name                   lower   upper  aggregation   edges
   1  Temperature               -1       1  WeightedMean      4
   2  Density                    0       1  WeightedMean      1
   3  Viscosity                  0       1  WeightedMean      0
   4  Pressure                   0       1  Sum               1
   5  Volatility                 0       1  WeightedMean      2
   6  Expansion                 -1       1  WeightedMean      1
   7  Compression                0       1  WeightedMean      2
   8  PhaseAffinity             -1       1  WeightedMean      2

evaluation order:
  single Temperature
  single Viscosity
  single Volatility
  single Compression
  single PhaseAffinity
  single Density
  loop   Pressure Expansion

who pushes Pressure (in the order they are added up):
  Temperature (influence #1 of Temperature)
  Density (influence #0 of Density)
  Volatility (influence #0 of Volatility)
  Expansion (influence #0 of Expansion)
  Compression (influence #0 of Compression)

the scoreboard of 2 Fire + 1 Water:
PropertyState: 8 slots
  Temperature = 0.533333
  Density = 0.172614
  Viscosity = 0.0266667
  Pressure = 0.702302
  Volatility = 0.746667
  Expansion = 0.865677
  Compression = 0.266667
  PhaseAffinity = 0.882875
```

### 5.7 Saving and sending values: use names

```cpp
using NamedValues = std::vector<std::pair<std::string, double>>;
NamedValues   toNamedValues(const PropertyState&, const PropertyRegistry&);
PropertyState fromNamedValues(const NamedValues&, const PropertyRegistry&);
```

- `toNamedValues` lists only the slots that hold a value, in id order. It throws `std::invalid_argument` if the state has a different number of slots than the registry has properties (it belongs to another model).
- `fromNamedValues` makes a state with one slot per property and fills the named ones. It throws `std::invalid_argument` for an unknown name, a name listed twice, or a value that is not finite. It does not check the range, because a state never clamps.
- Never save a `Spell` or a `PropertyState` as raw ids. Save names, and rebuild the spell with `SpellBuilder` when you load.

---

## 6. How a spell is calculated

### 6.1 Validate

`validateSpell` checks the spell (section 5.5). A bad spell throws before any work is done.

### 6.2 Mix: one base value per property

For each property, for each ingredient whose card has a value for it:

```text
value  = card value * amplify factor of that property (1 if none)
weight = amount * energy budget    (Sum properties)
         amount                    (WeightedMean and Median)
```

The values are combined by the property's aggregation kind (section 5.1) and the result is clamped into the range. That is the **base**. For 2 Fire + 1 Water the base values are:

| Property | Calculation | Base |
|---|---|---|
| Temperature (mean) | (2 x 0.95 + 1 x -0.30) / 3 | 0.5333 |
| Viscosity (mean) | (2 x 0.05 + 1 x 0.78) / 3 | 0.2933 |
| Pressure (sum) | 2 x 0.04 + 1 x 0.03 | 0.11 |
| Expansion (mean) | (2 x 0.72 + 1 x 0.12) / 3 | 0.52 |

Amplify happens here and only here: it changes the base, and every property downstream sees the changed value through its influences. Pushes are never amplified.

### 6.3 What a push is

`push = coefficient * shape(x)`, with `x` the source's final (clamped) value. A property's raw value is its base plus the pushes of all its incoming influences, added in the order given by `engine.incoming(id)`. The final value is the raw value clamped into the range.

### 6.4 The evaluation order (and how ties are broken)

A property must be worked out **after** everything that pushes it. The engine finds this order when you build it, with these two ideas:

1. **Loops are found first** with Tarjan's algorithm. Properties that push each other in a circle are grouped, and each group becomes one step (a loop). Every other property is a step by itself.
2. **The steps are ordered with Kahn's algorithm.** A step is "ready" when everything that pushes it has been placed. The engine repeatedly takes a ready step, places it, and thereby makes later steps ready.

When **several steps are ready at the same time, the one with the lowest property id goes first** (for a loop: the lowest id among its members). That rule makes the order, and therefore every floating-point result, the same on every run and every machine. The order for the demo model, from section 5.6:

```text
single Temperature      (id 1: nothing pushes it, and it has the lowest id)
single Viscosity        (id 3: ready once Temperature is placed; ready steps are now 3, 5, 7, 8)
single Volatility       (id 5)
single Compression      (id 7: ready from the start, but a lower id went first)
single PhaseAffinity    (id 8)
single Density          (id 2: pushed by PhaseAffinity, so it only became ready now)
loop   Pressure Expansion   (waits for Temperature, Density, Volatility and Compression)
```

Any other valid order would give the same numbers. The tie rule only exists so you always get the same one.

### 6.5 Settling a loop: repeat with damping

A loop cannot be worked out in one pass, because each member needs the other. The solver repeats:

1. For every member, work out what it **wants** to be: its base plus all pushes (members' pushes use the values from the **previous** repeat), clamped into its range.
2. Move **70 %** of the way from the current value to that target (the **damping factor** 0.7), clamped again.
3. Stop when no member moved by more than `1e-12`, or after **1000** repeats.

All members are updated from the previous repeat's values together, never from a freshly updated neighbour, so the result does not depend on which member is listed first. Members start at their base. If the loop has not settled after 1000 repeats, the last values are returned, `converged` is `false`, and a warning is added. Nothing is thrown.

Worked result, 2 Fire + 1 Water in the demo model: Pressure settles at 0.7023 and Expansion at 0.8657 after 33 repeats.

### 6.6 The stability check

A loop is only safe if each repeat makes the leftover error smaller. The **growth rate** is the factor by which a small error changes per repeat: below 1 the loop settles; above 1 it grows.

For a loop of two properties with Linear influences of strengths a and b, the growth rate is `0.3 + 0.7 * sqrt(a * b)` (the 0.3 and 0.7 come from the damping). The engine does not do algebra; it measures the rate numerically:

1. It samples 64 operating points spread evenly over the members' ranges (a fixed Halton sequence, so the answer never changes between runs).
2. At each point it replaces every influence between the members by its slope there, adds the damping, and applies that repeatedly to two fixed starting patterns (all ones and alternating signs) for 200 steps. The average growth over the last 100 steps is the rate.
3. The worst rate over all points is the loop's growth rate.

| Growth rate | Decision |
|---|---|
| below 0.9 | accepted |
| 0.9 up to (not including) 1.0 | accepted with a **build warning** |
| 1.0 or more | **refused**: a build problem, and no engine |

| Loop (pushes a and b) | Growth rate | Decision |
|---|---|---|
| 0.3 and 0.3 | 0.51 | accepted |
| 0.9 and 1.0 | 0.96 | accepted with a warning |
| 3.0 and 3.0 | 2.4 | refused |
| 0.9 and -0.9 (sign flips each repeat) | 0.70 | accepted |
| 1.6 and -1.6 | 1.16 | refused |

Only influences **between the loop's members** count. Pushes that come from outside the loop are constants for it. The check samples; it does not prove stability everywhere. Keep a safety margin, and test the extremes of your game.

### 6.7 Tuning constants

These are compile-time constants in `src/model/engine.cpp` (they are not runtime options yet). Change them there and rebuild.

| Constant | Value | Meaning |
|---|---|---|
| `kDamping` | 0.7 | Share of the way a loop member moves each repeat |
| `kTolerance` | 1e-12 | A loop has settled when no member moves more than this |
| `kMaxRepeats` | 1000 | Repeats before giving up |
| `kRejectGrowth` | 1.0 | Growth rate at which a loop is refused |
| `kWarnGrowth` | 0.9 | Growth rate at which a loop gets a warning |

Section 6.6's sampling (64 points, 200 steps, slope step 1e-6) is inside `growthRate` and `growthOf`.

---

## 7. Built-in data

Two ready-made models live in `mcre/model/builtin.hpp`. Tests and demos use them instead of retyping the setup.

### 7.1 The prototype (4 properties, no influences)

```cpp
builtin::addPrototypeProperties(properties);              // ids 1..4
builtin::addPrototypeComponents(components, properties);  // Fire = 1, Water = 2
```

| Id | Property | Range | Mixing |
|---|---|---|---|
| 1 | Temperature | -1 to 1 | WeightedMean |
| 2 | Viscosity | 0 to 1 | WeightedMean |
| 3 | Pressure | 0 to 1 | Sum |
| 4 | Expansion | -1 to 1 | WeightedMean |

| Component | Temperature | Viscosity | Pressure | Expansion |
|---|---|---|---|---|
| Fire | 0.95 | 0.05 | 0.04 | 0.72 |
| Water | -0.30 | 0.78 | 0.03 | 0.12 |

This model has no influences, so a cast gives only the mixed values. Calling either function twice on the same registry throws, because the names exist already. `addPrototypeComponents` throws `std::logic_error` if the properties are missing (a mistake in the calling code).

### 7.2 The demo model (8 properties, 13 influences, 4 elements)

```cpp
builtin::addDemoProperties(properties);              // ids 1..8, and the 13 influences
builtin::addDemoComponents(components, properties);  // Fire = 1, Water = 2, Earth = 3, Wind = 4
```

This is the smallest set of properties that contains the Pressure <-> Expansion loop together with everything that pushes it. It reproduces the numbers of the design document exactly.

| Id | Property | Range | Mixing |
|---|---|---|---|
| 1 | Temperature | -1 to 1 | WeightedMean |
| 2 | Density | 0 to 1 | WeightedMean |
| 3 | Viscosity | 0 to 1 | WeightedMean |
| 4 | Pressure | 0 to 1 | Sum |
| 5 | Volatility | 0 to 1 | WeightedMean |
| 6 | Expansion | -1 to 1 | WeightedMean |
| 7 | Compression | 0 to 1 | WeightedMean |
| 8 | PhaseAffinity | -1 to 1 | WeightedMean |

| Component | Temperature | Density | Viscosity | Pressure | Volatility | Expansion | Compression | PhaseAffinity |
|---|---|---|---|---|---|---|---|---|
| Fire | 0.95 | 0.18 | 0.05 | 0.04 | 0.88 | 0.72 | 0.10 | 0.80 |
| Water | -0.30 | 0.82 | 0.78 | 0.03 | 0.08 | 0.12 | 0.60 | 0.00 |
| Earth | 0.10 | 0.95 | 0.90 | 0.06 | 0.03 | -0.10 | 0.90 | -0.90 |
| Wind | -0.05 | 0.05 | 0.02 | 0.02 | 0.55 | 0.60 | 0.15 | 1.00 |

| Source | Target | Mechanism | Coefficient | Parameters | Reason |
|---|---|---|---|---|---|
| Temperature | Viscosity | Linear | -0.50 | | Heat thins liquids |
| Temperature | Pressure | Linear | 0.25 | | Thermal excitation adds pressure |
| Temperature | Volatility | Linear | 0.25 | | Heat speeds up change |
| Temperature | PhaseAffinity | Threshold | 0.60 | k 10, threshold 0.5 | Phase shifts toward gas past a heat threshold |
| Density | Pressure | Linear | 0.20 | | Dense mass raises pressure |
| Pressure | Expansion | Saturation | 0.30 | k 1 | Pressure pushes volume outward, diminishing |
| Volatility | Pressure | Exponential | 0.04 | k 2 | Volatile reactions escalate rapidly |
| Volatility | Expansion | Linear | 0.15 | | Rapid change spreads volume |
| Expansion | Pressure | Linear | 0.30 | | Confined expansion raises pressure |
| Compression | Pressure | Linear | 0.10 | | Compressed media store pressure |
| Compression | Expansion | Linear | -0.25 | | Resistance to volume change opposes expansion |
| PhaseAffinity | Density | Linear | -0.25 | | Gas-like phase lowers density |
| PhaseAffinity | Expansion | Linear | 0.20 | | Gas-like phase expands |

Pressure and Expansion push each other (the loop). Everything else is a single step.

**Reference results** (computed with an independent Python model; the tests check these to 10 decimals):

| Spell | Temperature | Density | Viscosity | Pressure | Volatility | Expansion | Compression | PhaseAffinity | Repeats |
|---|---|---|---|---|---|---|---|---|---|
| 2 Fire + 1 Water | 0.5333 | 0.1726 | 0.0267 | 0.7023 | 0.7467 | 0.8657 | 0.2667 | 0.8829 | 33 |
| same, amplify Temperature 1.2 | 0.6400 | 0.1433 | 0.0000 | 0.7423 | 0.7733 | 0.8971 | 0.2667 | 1.0000 | 32 |
| same as the first, concentrate 0.8 | 0.5333 | 0.1726 | 0.0267 | 0.7307 | 0.7467 | 0.8686 | 0.2667 | 0.8829 | 32 |
| 1 Fire + 1 Earth + 1 Wind | 0.3333 | 0.2945 | 0.1567 | 0.5606 | 0.5700 | 0.5832 | 0.3833 | 0.3953 | 33 |
| 3 Earth | 0.1000 | 1.0000 | 0.8500 | 0.3759 | 0.0550 | -0.4126 | 0.9000 | -0.8892 | 33 |

---

## 8. Errors, warnings and problems

MCRE reports faults in three different ways, depending on whose fault it is.

| Situation | How it is reported | Where to look |
|---|---|---|
| **Bad data** (a wrong JSON file, a loop that would blow up) | Returned in a list. **Never thrown.** | `LoadResult::problems`, `BuildResult::problems`, `BuildResult::warnings` |
| **Something unusual but not wrong** (a loop did not settle, an unknown amplify property) | A `Warning` in the result. Not thrown. | `Result::warnings`, `Result::converged` |
| **A wrong call by the programmer** (empty name, duplicate name, unknown id, bad number, empty spell) | Logged, then `std::invalid_argument` is thrown. | the exception text, and `mcre.log` |
| **A bug inside the library** (the built-in data does not load) | `std::logic_error` | |
| **A feature not written yet** | `mcre::NotImplementedError` (derives from `std::logic_error`) | the `Custom` mechanism |

**What throws `std::invalid_argument`**

| Call | When |
|---|---|
| `PropertyRegistry::add` | empty or duplicate name; bounds not finite or `lower > upper` |
| `PropertyRegistry::addInfluence` | any rule in section 5.2 is broken |
| `ComponentRegistry::add` | empty or duplicate name; unknown property id; a value not finite or outside the bounds |
| `validateSpell`, `Engine::evaluate` | no ingredients; unknown component id; bad amount, factor or budget; the same property amplified twice. Also, very rarely, numbers so large that a weight or a value overflows. |
| `SpellBuilder::combine`, `amplify`, `concentrate`, `build` | unknown component name; bad number; no ingredients |
| `PropertyState::set` | an id with no slot; a value that is not finite |
| `toNamedValues`, `fromNamedValues` | state from another model; unknown or repeated name; a value that is not finite |
| `aggregate`, `ClampToBounds` | a weight or value that is not finite; NaN |

**The messages you can see in `problems` and `warnings`**

| `where` | Message (shortened) | Meaning |
|---|---|---|
| `file.json#/components/0/properties/Temperature` | `value 1.7 is outside [-1, 1]` | A card value outside the range |
| `file.json#/components/0/properties/Weight` | `unknown property` | The dictionary has no such property |
| `file.json#/components/0/propertys` | `unknown key` | A misspelled key |
| `file.json#/schemaVersion` | `required: the number 1` | Missing or wrong schema version |
| `engine` | `the model has no properties` | `build` on an empty dictionary |
| `loop A, B` (build) | `growth rate 2.4 is 1 or more ...` | The stability check refused the loop |
| `loop A, B` (build warning) | `growth rate 0.96 is close to 1 ...` | Accepted, but it settles slowly |
| `loop A, B` (result) | `did not settle within 1000 repeats ...` | `converged` is `false` |
| `amplify` (result) | `property "Tempature" does not exist; ignored` | A misspelled amplify target |

---

## 9. Recipes

### 9.1 Turn a result into game numbers

MCRE returns properties. A short function in **your game** gives them meaning. Nothing in this function belongs to MCRE.

```cpp
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>

#include "mcre/model/builtin.hpp"
#include "mcre/model/engine.hpp"
#include "mcre/model/spellBuilder.hpp"

// ---- This part is the GAME'S code, not MCRE's. MCRE only returns property values; what they mean is up to you.
struct AttackProfile {
	double damage = 0.0;
	double burn = 0.0;
	double knockback = 0.0;
	std::string note;
};

AttackProfile interpret(const mcre::Engine& engine, const mcre::Result& result) {
	const auto value = [&](const char* name) { return result.state.get(engine.properties().idOf(name)).value(); };

	AttackProfile attack;
	attack.damage = 40.0 * value("Pressure") + 20.0 * std::max(0.0, value("Temperature"));
	attack.burn = std::max(0.0, value("Temperature")) * (1.0 - value("Viscosity"));
	attack.knockback = std::max(0.0, value("Expansion")) * value("Pressure");
	if (!result.converged) attack.note = "unstable spell";
	return attack;
}

int main() {
	using namespace mcre;

	PropertyRegistry properties;
	builtin::addDemoProperties(properties);
	ComponentRegistry components;
	builtin::addDemoComponents(components, properties);
	BuildResult built = Engine::build(std::move(properties), std::move(components));
	const Engine& engine = *built.engine;

	for (const char* element : { "Fire", "Water", "Earth", "Wind" }) {
		const AttackProfile a = interpret(engine, SpellBuilder(engine).combine(element, 2).combine("Fire").cast());
		std::cout << element << " x2 + Fire: damage " << a.damage << ", burn " << a.burn
			<< ", knockback " << a.knockback << "\n";
	}
}
```

Output:

```text
Fire x2 + Fire: damage 55.9225, burn 0.95, knockback 0.923062
Water x2 + Fire: damage 20.3246, burn 0.0608611, knockback 0.187554
Earth x2 + Fire: damage 30.142, burn 0.220417, knockback 0.0821066
Wind x2 + Fire: damage 29.5344, burn 0.283333, knockback 0.596692
```

The same engine can serve attack, defense, support or crafting: only your `interpret` function changes.

### 9.2 Cast many spells

Build the engine once and keep it. `evaluate` is `const` and keeps no state between calls, so casting is cheap and you can cast as often as you like. Several threads may call `evaluate` (or build and cast spells) on **one** engine at the same time; this was checked with ThreadSanitizer. Registries are not safe to modify while another thread reads them, but they are only modified before `build`.

### 9.3 Find out why a value is what it is

Read `result.trace`: find the entry for the property and look at `base`, `pushes`, `raw`, `clamped` and `repeat` (section 5.6). If `raw` is outside the property's range, the value was clamped and something pushed it very hard.

### 9.4 Test a new influence before it ships

1. Add it with `addInfluence` (every rule in section 5.2 is checked immediately).
2. Call `Engine::build` and read `problems` and `warnings`: a new loop shows up there with its growth rate.
3. Cast a few extreme spells (every ingredient at a large amount, with large amplify factors) and check `converged`.

---

## 10. Limits and what is not done yet

| Area | State |
|---|---|
| Properties and influences from JSON | **Not yet.** Only component cards load from JSON (section 5.3). Properties and influences are added in code. A JSON dictionary is planned. |
| The `Custom` mechanism | **Not written.** `addInfluence` rejects it. |
| Tuning constants | Compile-time only (section 6.7); not runtime options yet. |
| Stability check | Samples operating points; it does not prove stability everywhere (design assumption A-12). Test the extremes. |
| `mcre_app` | Shows the prototype data and loads component files. It does not cast spells. |
| Log file path | Always `mcre.log` in the working directory. |
| Constraint hook (design decision D6) | Not decided or written. |
| JSON duplicate keys | If one key appears twice in one JSON object, the last value wins and nothing is reported. |
| Per-mode results (attack, defense, alchemy) | By design the game's job: write your own `interpret` (section 9.1). |
| Saving results | `toNamedValues` and `fromNamedValues` only. There is no file format for a `Result` yet. |

---

## 11. Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `LNK2019: unresolved external symbol` for a function you just wrote | The new `.cpp` file is not in the list in `src/CMakeLists.txt`. Add it. |
| A new test does not run | Add the file to `tests/CMakeLists.txt`. |
| `Engine::build` returns `ok() == false` | Read `problems`. For a loop, make the pushes between its members weaker (lower the coefficients). |
| `result.converged` is `false` | A loop is too close to the limit, or the spell pushes it to a clamped extreme. Check `build` warnings, lower the coefficients, or accept the last values. |
| `std::invalid_argument: validateSpell: ...` | The message names what is wrong (empty spell, unknown component id, bad amount). With the builder you get the name instead of an id. |
| A property has an unexpected value of 0 | Nobody in the spell has a value for it (it is 0, clamped into its range), or an amplify factor of 0 switched it off. |
| An amplify has no effect | Check `result.warnings`: the property name or id may not exist. |
| The values differ from the design document | The 4-property prototype has no influences. Use the demo model (section 7.2) for the document's numbers. |
| A file loads with problems about `unknown key` | A misspelled key. Component keys are exactly `name`, `version` and `properties`. |
| The first call prints `[MCRE] Logging to file ...` | Normal: the logger opens `mcre.log` the first time something is logged. |

---

## 12. API cheat sheet

```cpp
// ---- Dictionary
PropertyRegistry properties;
PropertyId id = properties.add("Name", {lower, upper}, AggregationKind::WeightedMean /*Sum | Median*/);
properties.addInfluence(sourceId, InfluenceEdge{ target, mechanism, coefficient, {k, threshold}, priority, "reason" });
properties.idOf("Name");  properties.byId(id);  properties.byName("Name");  properties.size();  properties.all();
properties.dump(std::cout);

// ---- Ingredient cards
ComponentRegistry components;
component c; c.componentName = "Fire"; c.version = "1"; c.properties = { {propId, value}, ... };
components.add(c, properties);
components.idOf("Fire");  components.byId(id);  components.byName("Fire");  components.size();  components.all();
LoadResult r = loadComponentsFromFile("fire.json", properties);   // or loadComponentsFromText(text, "name", properties)
// r.ok(), r.problems, r.components  ->  components.add(std::move(c), properties) for each

// ---- Build once
BuildResult built = Engine::build(std::move(properties), std::move(components));
built.ok();  built.problems;  built.warnings;  const Engine& engine = *built.engine;
engine.properties();  engine.components();  engine.order();  engine.incoming(propId);

// ---- Cast
Result result = SpellBuilder(engine).combine("Fire", 2).combine("Water").amplify("Temperature", 1.2).concentrate(0.8).cast();
// or
Spell spell; spell.ingredients = { {componentId, amount} }; spell.amplify = { {propId, factor} }; spell.energyBudget = 1.25;
Result result = engine.evaluate(spell);

// ---- Read
result.state.get(propId);  result.state.valueOr(propId, 0.0);  result.state.has(propId);
result.converged;  result.warnings;  result.trace;
toNamedValues(result.state, engine.properties());   // names, for saving and logging
fromNamedValues(namedValues, engine.properties());
result.state.dump(std::cout, engine.properties());
```

| Header | What is in it |
|---|---|
| `mcre/model/model.hpp` | `PropertyId`, `ComponentId`, `Bounds`, `AggregationKind`, `Mechanism`, `ResponseParams`, `InfluenceEdge`, `property`, `component`, `ClampToBounds` |
| `mcre/model/propertyRegistry.hpp` | `PropertyRegistry` |
| `mcre/model/componentRegistry.hpp` | `ComponentRegistry` |
| `mcre/model/loader.hpp` | `loadComponentsFromFile`, `loadComponentsFromText`, `LoadResult`, `Diagnostic` |
| `mcre/model/engine.hpp` | `Engine`, `BuildResult`, `EvalStep`, `Incoming` |
| `mcre/model/spell.hpp` | `Spell`, `Ingredient`, `Amplify`, `validateSpell` |
| `mcre/model/spellBuilder.hpp` | `SpellBuilder` |
| `mcre/model/result.hpp` | `Result`, `TraceEntry`, `Push`, `Warning` |
| `mcre/model/propertyState.hpp` | `PropertyState` |
| `mcre/model/stateNames.hpp` | `toNamedValues`, `fromNamedValues`, `NamedValues` |
| `mcre/model/aggregation.hpp` | `aggregate`, `Contribution` (the mixing function, usable on its own) |
| `mcre/model/response.hpp` | `responseShape` (the curves, usable on their own) |
| `mcre/model/builtin.hpp` | the prototype and demo models |
| `mcre/core/errors.hpp` | `NotImplementedError`, `MCRE_TODO` |
| `mcre/core/logger.hpp` | `mcre::logger::info`, `mcre::logger::error` |
