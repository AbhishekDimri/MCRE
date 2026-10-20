#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "mcre/model/engine.hpp"
#include "mcre/model/result.hpp"
#include "mcre/model/spell.hpp"

namespace mcre {
	// Builds a Spell from names, the way a game talks ("2 Fire, 1 Water, a bit hotter"), and turns the
	// names into the ids the engine uses. Every call returns the builder, so calls can be chained:
	//
	//     Result r = SpellBuilder(engine).combine("Fire", 2).combine("Water").amplify("Temperature", 1.2).cast();
	//
	// The builder keeps a pointer to the engine, so the engine must live at least as long as the builder.
	// A name that does not exist, or a number that cannot be used, is a mistake in the calling code:
	// the call logs it and throws std::invalid_argument. The one exception is amplify() with an unknown
	// property name: that is ignored and cast() adds a warning to the result (design document, amendment A7).
	class SpellBuilder {
	public:
		explicit SpellBuilder(const Engine& engine) noexcept : engine_(&engine) {}

		// Adds an ingredient. 'amount' must be a finite number above 0. The same component may be added twice.
		SpellBuilder& combine(std::string_view component, double amount = 1.0);

		// Multiplies the value every ingredient has for this property before mixing. A factor of 0 is allowed
		// (it switches the property off); it must be finite and not negative. Calling it again for the same
		// property replaces the earlier factor.
		SpellBuilder& amplify(std::string_view property, double factor);

		// "Use less energy to get a tighter result": sets the energy budget to 1 / c. Only summed properties
		// (for example Pressure) change. 'c' must be a finite number above 0; 1 changes nothing.
		SpellBuilder& concentrate(double c);

		// The finished spell. Throws std::invalid_argument when it has no ingredients.
		[[nodiscard]] Spell build() const;

		// build() followed by engine.evaluate().
		[[nodiscard]] Result cast() const;

	private:
		const Engine* engine_;
		Spell spell_;
		std::vector<std::string> unknownAmplify_;   // property names that do not exist, for the warning
	};
}
