#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <vector>

#include "mcre/model/model.hpp"

namespace mcre {
	class PropertyRegistry;  // only needed by dump(), so the full class is included in the .cpp

	// The current value of every property in one situation, for example "Fire and Water combined".
	// There is one slot for each property id (1, 2, ... up to the number of properties).
	// A slot is either empty ("no value yet") or holds a finite number. Empty is not the same as 0.
	// Values are not checked against the property bounds here: whoever computes a value clamps it first.
	class PropertyState {
	public:
		// A state with 'propertyCount' slots, all empty. Use PropertyRegistry::size() for the count.
		explicit PropertyState(std::size_t propertyCount);

		std::size_t size() const noexcept { return values_.size(); }  // number of slots
		std::size_t presentCount() const noexcept;                    // how many slots hold a value

		// False for an empty slot, and also for an id that has no slot (0, or larger than size()).
		bool has(PropertyId id) const noexcept;

		// The value, or std::nullopt (an "empty box") when there is none.
		std::optional<double> get(PropertyId id) const noexcept;

		// The value, or 'fallback' when there is none.
		double valueOr(PropertyId id, double fallback) const noexcept;

		// Stores a value (replacing an earlier one). Throws std::invalid_argument for an id with no slot
		// or a value that is not finite.
		void set(PropertyId id, double value);

		// Two states are equal when they have the same slots and the same values.
		bool operator==(const PropertyState&) const = default;

		// Prints every property by name, with its value or "(none)". 'properties' is only used for the names.
		void dump(std::ostream& out, const PropertyRegistry& properties) const;

	private:
		std::vector<std::optional<double>> values_;
	};
}
