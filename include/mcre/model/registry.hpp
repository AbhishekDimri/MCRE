#pragma once

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "mcre/model/model.hpp"

namespace mcre {
	// The list of properties that exist in one engine instance.
	// It assigns each property a numeric id (1, 2, 3, ... in the order they are added) and answers
	// "which id has this name?" for the loaders. Files use names; the engine uses ids.
	class PropertyRegistry {
	public:
		// Adds a property and returns its id. Throws std::invalid_argument on bad input.
		PropertyId add(std::string_view name, Bounds bounds, AggregationKind aggregation = AggregationKind::WeightedMean);

		// nullptr when there is no such property. Pointers stay valid until the next add().
		const property* byId(PropertyId id) const noexcept;
		const property* byName(std::string_view name) const noexcept;


		// 0 when there is no such property (0 is never a real id)
		PropertyId idOf(std::string_view name) const noexcept;

		std::size_t size() const noexcept { return properties_.size(); }
		const std::vector<property>& all() const noexcept { return properties_; }

		// Prints the whole table (id, name, bounds, aggregation, number of edges). For checking values.
		void dump(std::ostream& out) const;

	private:
		std::vector<property> properties_;                        // properties_[id - 1]
		std::map<std::string, PropertyId, std::less<>> byName_;   // std::less<> allows lookup by string_view
	};
}
