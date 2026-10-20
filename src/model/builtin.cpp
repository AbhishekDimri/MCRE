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
  "properties": { "Temperature": 0.95, "Viscosity": 0.05, "Pressure": 0.04, "Expansion": 0.72 } },
{ "name": "Water", "version": "0.1.0",
  "properties": { "Temperature": -0.30, "Viscosity": 0.78, "Pressure": 0.03, "Expansion": 0.12 } }

			]
		})json";

		// The demo model's components: the four elements, with the eight demo properties.
		constexpr std::string_view kDemoComponentsJson = R"json({
			"schemaVersion": 1,
			"components": [
				{ "name": "Fire",  "version": "0.1.0", "properties": {
					"Temperature": 0.95, "Density": 0.18, "Viscosity": 0.05, "Pressure": 0.04,
					"Volatility": 0.88, "Expansion": 0.72, "Compression": 0.10, "PhaseAffinity": 0.80 } },
				{ "name": "Water", "version": "0.1.0", "properties": {
					"Temperature": -0.30, "Density": 0.82, "Viscosity": 0.78, "Pressure": 0.03,
					"Volatility": 0.08, "Expansion": 0.12, "Compression": 0.60, "PhaseAffinity": 0.00 } },
				{ "name": "Earth", "version": "0.1.0", "properties": {
					"Temperature": 0.10, "Density": 0.95, "Viscosity": 0.90, "Pressure": 0.06,
					"Volatility": 0.03, "Expansion": -0.10, "Compression": 0.90, "PhaseAffinity": -0.90 } },
				{ "name": "Wind",  "version": "0.1.0", "properties": {
					"Temperature": -0.05, "Density": 0.05, "Viscosity": 0.02, "Pressure": 0.02,
					"Volatility": 0.55, "Expansion": 0.60, "Compression": 0.15, "PhaseAffinity": 1.00 } }
			]
		})json";

		struct DemoInfluence {
			const char* source;
			const char* target;
			Mechanism mechanism;
			double coefficient;
			ResponseParams params;   // { k, threshold }
			const char* reason;
		};

		// The 13 influences between the eight demo properties. Pressure <-> Expansion is the loop.
		const DemoInfluence kDemoInfluences[] = {
			{ "Temperature",   "Viscosity",     Mechanism::Linear,      -0.50, { 1.0, 0.5 },  "Heat thins liquids" },
			{ "Temperature",   "Pressure",      Mechanism::Linear,       0.25, { 1.0, 0.5 },  "Thermal excitation adds pressure" },
			{ "Temperature",   "Volatility",    Mechanism::Linear,       0.25, { 1.0, 0.5 },  "Heat speeds up change" },
			{ "Temperature",   "PhaseAffinity", Mechanism::Threshold,    0.60, { 10.0, 0.5 }, "Phase shifts toward gas past a heat threshold" },
			{ "Density",       "Pressure",      Mechanism::Linear,       0.20, { 1.0, 0.5 },  "Dense mass raises pressure" },
			{ "Pressure",      "Expansion",     Mechanism::Saturation,   0.30, { 1.0, 0.5 },  "Pressure pushes volume outward, diminishing" },
			{ "Volatility",    "Pressure",      Mechanism::Exponential,  0.04, { 2.0, 0.5 },  "Volatile reactions escalate rapidly" },
			{ "Volatility",    "Expansion",     Mechanism::Linear,       0.15, { 1.0, 0.5 },  "Rapid change spreads volume" },
			{ "Expansion",     "Pressure",      Mechanism::Linear,       0.30, { 1.0, 0.5 },  "Confined expansion raises pressure" },
			{ "Compression",   "Pressure",      Mechanism::Linear,       0.10, { 1.0, 0.5 },  "Compressed media store pressure" },
			{ "Compression",   "Expansion",     Mechanism::Linear,      -0.25, { 1.0, 0.5 },  "Resistance to volume change opposes expansion" },
			{ "PhaseAffinity", "Density",       Mechanism::Linear,      -0.25, { 1.0, 0.5 },  "Gas-like phase lowers density" },
			{ "PhaseAffinity", "Expansion",     Mechanism::Linear,       0.20, { 1.0, 0.5 },  "Gas-like phase expands" },
		};

		// Reads a built-in components file into 'components'. A mistake in the data is a bug in this library.
		void addComponentsFromText(std::string_view json, std::string_view origin,
			ComponentRegistry& components, const PropertyRegistry& properties) {
			LoadResult loaded = loadComponentsFromText(json, origin, properties);

			if (!loaded.ok()) {
				std::string message = "builtin: the built-in data is wrong (" + std::string(origin) + "):";
				for (const Diagnostic& d : loaded.problems) message += "\n  " + d.where + ": " + d.message;
				throw std::logic_error(message);
			}

			for (component& c : loaded.components) {
				components.add(std::move(c), properties);
			}
		}
	}

	void addPrototypeProperties(PropertyRegistry& properties) {
		properties.add("Temperature", { -1.0, 1.0 });
		properties.add("Viscosity", { 0.0, 1.0 });
		properties.add("Pressure", { 0.0, 1.0 }, AggregationKind::Sum);
		properties.add("Expansion", { -1.0, 1.0 });
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

	void addDemoProperties(PropertyRegistry& properties) {
		properties.add("Temperature", { -1.0, 1.0 });
		properties.add("Density", { 0.0, 1.0 });
		properties.add("Viscosity", { 0.0, 1.0 });
		properties.add("Pressure", { 0.0, 1.0 }, AggregationKind::Sum);
		properties.add("Volatility", { 0.0, 1.0 });
		properties.add("Expansion", { -1.0, 1.0 });
		properties.add("Compression", { 0.0, 1.0 });
		properties.add("PhaseAffinity", { -1.0, 1.0 });

		// Every property exists now, so the influences can name both ends.
		for (const DemoInfluence& d : kDemoInfluences) {
			InfluenceEdge edge;
			edge.target = properties.idOf(d.target);
			edge.mechanism = d.mechanism;
			edge.coefficient = d.coefficient;
			edge.params = d.params;
			edge.reason = d.reason;
			properties.addInfluence(properties.idOf(d.source), edge);
		}
	}

	void addDemoComponents(ComponentRegistry& components, const PropertyRegistry& properties) {
		addComponentsFromText(kDemoComponentsJson, "builtin demo", components, properties);
	}
}
