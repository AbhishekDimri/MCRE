#include "mcre/model/spellBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "mcre/core/logger.hpp"

namespace mcre {
	namespace {
		// Logs the reason, then throws. Every check in this file goes through it.
		[[noreturn]] void reject(const std::string& message) {
			logger::error(message);
			throw std::invalid_argument(message);
		}
	}

	SpellBuilder& SpellBuilder::combine(std::string_view component, double amount) {
		const ComponentId id = engine_->components().idOf(component);
		if (id == 0) reject("SpellBuilder::combine: unknown component \"" + std::string(component) + "\"");
		if (!std::isfinite(amount) || amount <= 0.0) {
			reject("SpellBuilder::combine: the amount of \"" + std::string(component) + "\" must be a finite number above 0");
		}

		spell_.ingredients.push_back({ id, amount });
		return *this;
	}

	SpellBuilder& SpellBuilder::amplify(std::string_view property, double factor) {
		if (!std::isfinite(factor) || factor < 0.0) {
			reject("SpellBuilder::amplify: the factor for \"" + std::string(property) + "\" must be a finite number, not negative");
		}

		const PropertyId id = engine_->properties().idOf(property);
		if (id == 0) {
			const std::string name(property);
			if (std::find(unknownAmplify_.begin(), unknownAmplify_.end(), name) == unknownAmplify_.end()) {
				unknownAmplify_.push_back(name);
			}
			return *this;
		}

		for (Amplify& a : spell_.amplify) {
			if (a.property == id) {
				a.factor = factor;
				return *this;
			}
		}
		spell_.amplify.push_back({ id, factor });
		return *this;
	}

	SpellBuilder& SpellBuilder::concentrate(double c) {
		if (!std::isfinite(c) || c <= 0.0) reject("SpellBuilder::concentrate: c must be a finite number above 0");

		spell_.energyBudget = 1.0 / c;
		return *this;
	}

	Spell SpellBuilder::build() const {
		validateSpell(spell_, engine_->components());
		return spell_;
	}

	Result SpellBuilder::cast() const {
		Result result = engine_->evaluate(build());
		for (const std::string& name : unknownAmplify_) {
			result.warnings.push_back({ "amplify", "property \"" + name + "\" does not exist; ignored" });
		}
		return result;
	}
}
