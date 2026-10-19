#include "mcre/model/builtin.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/loader.hpp"
#include "mcre/model/propertyRegistry.hpp"

namespace mcre::builtin {
	namespace {
		constexpr std::string_view kComponentsJson = R"json({
			"schemaVersion": 1,
			"components": [
				{ "name": "Fire",  "version": "0.1.0",
				  "properties": { "Temperature": 0.95, "Viscosity": 0.05, "Expansion": 0.72 } },
				{ "name": "Water", "version": "0.1.0",
				  "properties": { "Temperature": 0.20, "Viscosity": 0.78, "Expansion": 0.12 } }
			]
		})json";
	}

	void addPrototypeProperties(PropertyRegistry& properties) {
		properties.add("Temperature", { 0.0, 1.0 });
		properties.add("Viscosity", { 0.0, 1.0 });
		properties.add("Pressure", { 0.0, 1.0 });
		properties.add("Expansion", { 0.0, 1.0 });
	}

	void addPrototypeComponents(ComponentRegistry& components, const PropertyRegistry& properties) {
		LoadResult loaded = loadComponentsFromText(kComponentsJson, "builtin", properties);

		if (!loaded.ok()) {
			std::string message = "builtin::addPrototypeComponents: the built-in data is wrong:";
			for (const Diagnostic& d : loaded.problems) message += "\n  " + d.where + ": " + d.message;
			throw std::logic_error(message);
		}

		for (component& c : loaded.components) {
			components.add(std::move(c), properties);
		}
	}
}
