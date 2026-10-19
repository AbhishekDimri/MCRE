#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "mcre/model/registry.hpp"
#include "mcre/model/model.hpp"

// Loads components from JSON, for example:
//
//   {
//     "schemaVersion": 1,
//     "components": [
//       { "name": "Fire", "version": "1", "properties": { "Temperature": 0.95, "Viscosity": 0.1 } }
//     ]
//   }
//
// Property names must already exist in the PropertyRegistry, and every value must lie inside that
// property's bounds. Nothing in this header mentions the JSON library; only loader.cpp uses it.

namespace mcre {
	// One problem found in the input.
	//   where:   the address of the bad spot, e.g. "fire.json#/components/0/properties/Temperature"
	//   message: what is wrong, e.g. "value 1.7 is outside [0, 1]"
	struct Diagnostic {
		std::string where;
		std::string message;
	};

	struct LoadResult {
		std::vector<component> components;
		std::vector<Diagnostic> problems;

		[[nodiscard]] bool ok() const noexcept { return problems.empty(); }
	};

	// Reads components from JSON text. 'sourceName' is only used in messages (a file name, "inline", ...).
	// Bad input is reported in LoadResult::problems; this function does not throw for it.
	[[nodiscard]] LoadResult loadComponentsFromText(std::string_view jsonText, std::string_view sourceName,
		const PropertyRegistry& properties);

	// Same, but reads the file first. A file that cannot be opened is reported as a problem too.
	[[nodiscard]] LoadResult loadComponentsFromFile(const std::filesystem::path& file,
		const PropertyRegistry& properties);
}
