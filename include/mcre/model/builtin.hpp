#pragma once

// The prototype data from the design document (section 63): four properties and two components.
// Tests and demos call these two functions instead of typing the same setup every time.

namespace mcre {
	class PropertyRegistry;
	class ComponentRegistry;

	namespace builtin {
		// Adds Temperature, Viscosity, Pressure and Expansion (ids 1, 2, 3, 4), each with bounds [0, 1].
		// Call it once per registry: a second call throws because the names already exist.
		void addPrototypeProperties(PropertyRegistry& properties);

		// Adds Fire and Water (ids 1 and 2). The four properties must already be in 'properties'.
		// Throws std::logic_error if the built-in data is wrong (a bug in this library, not bad user input).
		void addPrototypeComponents(ComponentRegistry& components, const PropertyRegistry& properties);
	}
}
