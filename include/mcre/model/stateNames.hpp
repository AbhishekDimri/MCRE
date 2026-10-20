#pragma once

#include <string>
#include <utility>
#include <vector>

#include "mcre/model/propertyRegistry.hpp"
#include "mcre/model/propertyState.hpp"

namespace mcre {
	// Property ids exist only in memory: they are the order in which properties were added, so adding or
	// reordering a property changes them. Anything that is saved, sent over a network or written to a log
	// that someone will read later must use names. These two functions are the way across.

	// (name, value) for every slot that holds a value, in id order. Empty slots are left out.
	// Throws std::invalid_argument when the state does not belong to this registry (different slot count).
	using NamedValues = std::vector<std::pair<std::string, double>>;
	[[nodiscard]] NamedValues toNamedValues(const PropertyState& state, const PropertyRegistry& properties);

	// Builds a state with one slot per property, and fills the slots that are named.
	// Throws std::invalid_argument for an unknown name, a name listed twice or a value that is not finite.
	// It does not check the property bounds: a state never clamps.
	[[nodiscard]] PropertyState fromNamedValues(const NamedValues& values, const PropertyRegistry& properties);
}
