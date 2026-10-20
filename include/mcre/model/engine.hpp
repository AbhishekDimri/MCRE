#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/loader.hpp"   // for Diagnostic
#include "mcre/model/model.hpp"
#include "mcre/model/propertyRegistry.hpp"
#include "mcre/model/result.hpp"
#include "mcre/model/spell.hpp"

namespace mcre {
	// "This influence pushes me": the source property, and which of the source's influences it is
	// (an index into property::influences of that source).
	struct Incoming {
		PropertyId source = 0;
		std::size_t edge = 0;
	};

	// One step of an evaluation: a single property, or a group of properties that depend on each
	// other in a circle (a loop) and are settled together.
	struct EvalStep {
		std::vector<PropertyId> properties;   // in id order
		bool isLoop = false;
	};

	struct BuildResult;

	// The finished model: the dictionary, the ingredient cards, and everything worked out from them
	// (the order, the loops, who pushes whom). It is built once and never changes afterwards, so it is
	// safe to share between threads, and no property can be added in the middle of an evaluation.
	//
	// Build it with Engine::build(). Ids inside an Engine are only valid for that Engine: anything that is
	// saved or sent must use names (see stateNames.hpp).
	class Engine {
	public:
		// Checks the model and works out the order and the loops. Bad data is reported in
		// BuildResult::problems; this function does not throw for it. The registries are moved in, so the
		// Engine owns its own copy.
		//
		// A loop that would run out of control is a problem (no engine is built). A loop that is close to the
		// limit is accepted, with a warning in BuildResult::warnings (design document 6.4).
		[[nodiscard]] static BuildResult build(PropertyRegistry properties, ComponentRegistry components);

		const PropertyRegistry& properties() const noexcept { return properties_; }
		const ComponentRegistry& components() const noexcept { return components_; }

		// The order to evaluate in: each step is one property, or one loop. A step always comes after
		// every step that pushes it.
		const std::vector<EvalStep>& order() const noexcept { return order_; }

		// Who pushes this property. Empty for an id that has no slot.
		const std::vector<Incoming>& incoming(PropertyId target) const noexcept;

		// Mixes the spell, applies the influences (settling every loop by repeating) and returns the
		// property sheet and its trace. Every property has a value afterwards (design document, decision Q-09).
		// A loop that does not settle is not an error: the last values are returned, 'converged' is false
		// and there is a warning.
		// Throws std::invalid_argument for a bad spell (see validateSpell).
		Result evaluate(const Spell& spell) const;

	private:
		Engine(PropertyRegistry properties, ComponentRegistry components);

		PropertyRegistry properties_;
		ComponentRegistry components_;
		std::vector<EvalStep> order_;
		std::vector<std::vector<Incoming>> incoming_;   // incoming_[target - 1]
	};

	struct BuildResult {
		std::optional<Engine> engine;        // empty when there are problems
		std::vector<Diagnostic> problems;    // the model cannot be used
		std::vector<Diagnostic> warnings;    // the model works, but look at these

		[[nodiscard]] bool ok() const noexcept { return engine.has_value() && problems.empty(); }
	};
}
