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
	class PropertyRegistry;  // only the name is needed here; registry.hpp is included in the .cpp

	// The list of components that exist in one engine instance.
	// It assigns each component a numeric id (1, 2, 3, ... in the order they are added) and answers
	// "which component has this name?". Loaded components arrive with componentId 0; add() gives the real id.
	class ComponentRegistry {
	public:
		// Checks the component against 'properties', stores it, and returns its new id.
		// Whatever componentId the caller set is ignored. Throws std::invalid_argument on bad input:
		// empty name, a name that already exists, a property id that does not exist,
		// or a value that is not finite or lies outside its property's bounds.
		ComponentId add(component c, const PropertyRegistry& properties);

		// nullptr when there is no such component. Pointers stay valid until the next add().
		const component* byId(ComponentId id) const noexcept;
		const component* byName(std::string_view name) const noexcept;

		// 0 when there is no such component (0 is never a real id)
		ComponentId idOf(std::string_view name) const noexcept;

		std::size_t size() const noexcept { return components_.size(); }
		const std::vector<component>& all() const noexcept { return components_; }

		// Prints every component with its property values. 'properties' is only used to turn ids into names.
		void dump(std::ostream& out, const PropertyRegistry& properties) const;

	private:
		std::vector<component> components_;                       // components_[id - 1]
		std::map<std::string, ComponentId, std::less<>> byName_;  // std::less<> allows lookup by string_view
	};
}
