#pragma once

// Small helpers shared by the evaluate, loop and spell builder tests.

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>

#include "mcre/model/builtin.hpp"
#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/engine.hpp"
#include "mcre/model/propertyRegistry.hpp"
#include "mcre/model/result.hpp"
#include "mcre/model/spell.hpp"

namespace testmodel {
	struct Registries {
		mcre::PropertyRegistry properties;
		mcre::ComponentRegistry components;
	};

	// Temperature, Viscosity, Pressure, Expansion (ids 1 to 4) and Fire, Water (ids 1, 2). No influences.
	inline Registries prototype() {
		Registries r;
		mcre::builtin::addPrototypeProperties(r.properties);
		mcre::builtin::addPrototypeComponents(r.components, r.properties);
		return r;
	}

	// The eight demo properties with their 13 influences, and Fire, Water, Earth, Wind (ids 1 to 4).
	inline Registries demo() {
		Registries r;
		mcre::builtin::addDemoProperties(r.properties);
		mcre::builtin::addDemoComponents(r.components, r.properties);
		return r;
	}

	// Builds the engine. A model that does not build is a mistake in the test, so this throws.
	inline mcre::Engine build(Registries r) {
		mcre::BuildResult built = mcre::Engine::build(std::move(r.properties), std::move(r.components));
		if (!built.ok()) throw std::runtime_error("the test model did not build");
		return std::move(*built.engine);
	}

	inline mcre::BuildResult tryBuild(Registries r) {
		return mcre::Engine::build(std::move(r.properties), std::move(r.components));
	}

	// "source pushes target"
	inline void addEdge(mcre::PropertyRegistry& properties, const char* source, const char* target, double coefficient,
		mcre::Mechanism mechanism = mcre::Mechanism::Linear, int priority = 0, mcre::ResponseParams params = {}) {
		mcre::InfluenceEdge edge;
		edge.target = properties.idOf(target);
		edge.mechanism = mechanism;
		edge.coefficient = coefficient;
		edge.params = params;
		edge.priority = priority;
		edge.reason = "test";
		properties.addInfluence(properties.idOf(source), edge);
	}

	// A component called 'name' with the given (property name, value) pairs.
	inline void addComponent(Registries& r, const char* name,
		std::initializer_list<std::pair<const char*, double>> values) {
		mcre::component c;
		c.componentName = name;
		c.version = "1.0.0";
		for (const auto& [property, value] : values) c.properties[r.properties.idOf(property)] = value;
		r.components.add(std::move(c), r.properties);
	}

	// Two properties A and B (both from -1 to 1) in a loop: A pushes B by 'aToB' and B pushes A by 'bToA'.
	// The component X has A = baseA and B = 0, so it is the only ingredient and the loop starts from baseA.
	inline Registries pair(double aToB, double bToA, double baseA = 0.05) {
		Registries r;
		r.properties.add("A", { -1.0, 1.0 });
		r.properties.add("B", { -1.0, 1.0 });
		addEdge(r.properties, "A", "B", aToB);
		addEdge(r.properties, "B", "A", bToA);
		addComponent(r, "X", { { "A", baseA }, { "B", 0.0 } });
		return r;
	}

	// 'count' properties P0, P1, ... in a ring: P0 pushes P1, P1 pushes P2, ..., the last pushes P0.
	// The component X has P0 = baseFirst and 0 for the others.
	inline Registries ring(std::size_t count, double coefficient, double baseFirst = 0.01) {
		Registries r;
		for (std::size_t i = 0; i < count; ++i) r.properties.add("P" + std::to_string(i), { -1.0, 1.0 });

		mcre::component x;
		x.componentName = "X";
		x.version = "1.0.0";
		for (std::size_t i = 0; i < count; ++i) {
			const std::string from = "P" + std::to_string(i);
			const std::string to = "P" + std::to_string((i + 1) % count);
			addEdge(r.properties, from.c_str(), to.c_str(), coefficient);
			x.properties[r.properties.idOf(from)] = i == 0 ? baseFirst : 0.0;
		}
		r.components.add(std::move(x), r.properties);
		return r;
	}

	// One ingredient list: spellOf({ {1, 2.0}, {2, 1.0} }) is "2 of component 1 and 1 of component 2".
	inline mcre::Spell spellOf(std::initializer_list<std::pair<mcre::ComponentId, double>> items) {
		mcre::Spell spell;
		for (const auto& [id, amount] : items) spell.ingredients.push_back({ id, amount });
		return spell;
	}

	// 2 Fire + 1 Water in the prototype and in the demo model (Fire is component 1, Water is component 2).
	inline mcre::Spell twoFireOneWater() { return spellOf({ { 1, 2.0 }, { 2, 1.0 } }); }

	// The final value of the property called 'name'.
	inline double valueOf(const mcre::Engine& engine, const mcre::Result& result, const char* name) {
		const auto value = result.state.get(engine.properties().idOf(name));
		if (!value) throw std::runtime_error(std::string("no value for ") + name);
		return *value;
	}

	inline const mcre::TraceEntry& traceOf(const mcre::Engine& engine, const mcre::Result& result, const char* name) {
		const mcre::PropertyId id = engine.properties().idOf(name);
		for (const mcre::TraceEntry& entry : result.trace) {
			if (entry.property == id) return entry;
		}
		throw std::runtime_error(std::string("no trace entry for ") + name);
	}
}
