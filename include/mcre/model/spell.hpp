#pragma once

#include <vector>

#include "mcre/model/model.hpp"

namespace mcre {
	class ComponentRegistry;

	// One ingredient of a spell: which component, and how much of it. "2 Fire" is { fireId, 2.0 }.
	// The amount is the component's weight in every blend (design document 4.2).
	struct Ingredient {
		ComponentId component = 0;
		double amount = 1.0;
	};

	// "Make this property stronger or weaker": every ingredient's value for this property is multiplied by
	// 'factor' before mixing, which scales the mixed value by the same factor (design document 5, "scales
	// that property's input values"). amplify(Temperature, 1.2) is { temperatureId, 1.2 }.
	// A property id that does not exist is not an error: evaluate() ignores it and adds a warning
	// to the result (design document, amendment A7).
	struct Amplify {
		PropertyId property = 0;
		double factor = 1.0;
	};

	// Everything the engine needs to know about one cast. It holds ids, not names: the builder
	// (combine("Fire", 2) ...) turns names into ids. Ids are only valid for one Engine, so never save a Spell.
	struct Spell {
		std::vector<Ingredient> ingredients;  // the same component may appear more than once
		std::vector<Amplify> amplify;         // at most one entry per property
		double energyBudget = 1.0;            // only summed properties use it; concentrate(c) sets it to 1 / c
	};

	// Checks a spell and throws std::invalid_argument for a programming mistake (and logs the reason):
	//   - no ingredients
	//   - an ingredient whose component does not exist, or whose amount is not a finite number above 0
	//   - an amplify factor that is not finite or is negative, or two amplify entries for one property
	//   - an energy budget that is not a finite number above 0
	void validateSpell(const Spell& spell, const ComponentRegistry& components);
}
