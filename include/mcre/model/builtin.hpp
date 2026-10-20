#pragma once

// The prototype data from the design document (section 6.3): four properties and two components, and the
// larger demo model with influences. Tests and demos call these functions instead of typing the same
// setup every time.

namespace mcre {
	class PropertyRegistry;
	class ComponentRegistry;

	namespace builtin {
		// Adds Temperature, Viscosity, Pressure and Expansion (ids 1, 2, 3, 4). Pressure is summed, the others
		// are mixed by weighted mean; Temperature and Expansion run from -1 to 1, the others from 0 to 1.
		// Call it once per registry: a second call throws because the names already exist.
		// This model has no influences, so there is nothing to push and nothing to loop.
		void addPrototypeProperties(PropertyRegistry& properties);

		// Adds Fire and Water (ids 1 and 2). The four properties must already be in 'properties'.
		// Throws std::logic_error if the built-in data is wrong (a bug in this library, not bad user input).
		void addPrototypeComponents(ComponentRegistry& components, const PropertyRegistry& properties);

		// ---- The demo model: the prototype with its feedback loop (design document 6 and 8, question Q-10) ----
		//
		// Eight properties and the 13 influences between them. It is the smallest set that holds the
		// Pressure <-> Expansion loop together with everything that pushes it, so it gives exactly the
		// numbers of the design document: 2 Fire + 1 Water settles at Pressure 0.7023 and Expansion 0.8657
		// after 33 repeats. The ids follow the order below.

		// Adds Temperature, Density, Viscosity, Pressure, Volatility, Expansion, Compression and PhaseAffinity
		// (ids 1 to 8) and the 13 influences between them. Call it once per registry (it throws otherwise).
		void addDemoProperties(PropertyRegistry& properties);

		// Adds Fire, Water, Earth and Wind (ids 1 to 4). The demo properties must already be in 'properties'.
		// Throws std::logic_error if the built-in data is wrong (a bug in this library, not bad user input).
		void addDemoComponents(ComponentRegistry& components, const PropertyRegistry& properties);
	}
}
