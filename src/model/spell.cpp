#include "mcre/model/spell.hpp"

#include <cmath>
#include <set>
#include <stdexcept>
#include <string>

#include "mcre/core/logger.hpp"
#include "mcre/model/componentRegistry.hpp"

namespace mcre {
	namespace {
		// Logs the reason, then throws, the same way the registries do.
		[[noreturn]] void reject(const std::string& why) {
			logger::error("validateSpell rejected the spell: ", why);
			throw std::invalid_argument("validateSpell: " + why);
		}
	}

	void validateSpell(const Spell& spell, const ComponentRegistry& components) {
		if (spell.ingredients.empty()) reject("a spell needs at least one ingredient");

		for (const Ingredient& ingredient : spell.ingredients) {
			if (components.byId(ingredient.component) == nullptr) {
				reject("component id " + std::to_string(ingredient.component) + " does not exist");
			}
			if (!std::isfinite(ingredient.amount) || ingredient.amount <= 0.0) {
				reject("the amount of component id " + std::to_string(ingredient.component)
					+ " must be a finite number above 0");
			}
		}

		std::set<PropertyId> amplified;
		for (const Amplify& amplify : spell.amplify) {
			if (!std::isfinite(amplify.factor) || amplify.factor < 0.0) {
				reject("the amplify factor for property id " + std::to_string(amplify.property)
					+ " must be a finite number that is not negative");
			}
			if (!amplified.insert(amplify.property).second) {
				reject("property id " + std::to_string(amplify.property) + " is amplified twice");
			}
		}

		if (!std::isfinite(spell.energyBudget) || spell.energyBudget <= 0.0) {
			reject("the energy budget must be a finite number above 0");
		}
	}
}
