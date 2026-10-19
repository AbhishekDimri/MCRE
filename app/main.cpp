// Manual test of the component loader (until real unit tests exist).

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>

#include "mcre/model/loader.hpp"
#include "mcre/model/propertyRegistry.hpp"
#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/builtin.hpp"

namespace {
	// Prints what a load produced: every component, or every problem.
	void show(std::string_view title, const mcre::LoadResult& result, const mcre::PropertyRegistry& registry) {
		std::cout << "\n=== " << title << " ===\n";
		std::cout << "ok? " << (result.ok() ? "yes" : "no")
			<< ", components: " << result.components.size()
			<< ", problems: " << result.problems.size() << "\n";

		for (const mcre::component& c : result.components) {
			std::cout << "  " << c.componentName << " (version '" << c.version << "')\n";
			for (const auto& [id, value] : c.properties) {
				const mcre::property* p = registry.byId(id);   // id -> property, to print its name
				std::cout << "      " << (p ? p->propertyName : "?") << " = " << value << "\n";
			}
		}
		for (const mcre::Diagnostic& d : result.problems) {
			std::cout << "  PROBLEM  " << d.where << "  ->  " << d.message << "\n";
		}
	}
}

int main() {
	// The properties that exist. The loader checks the file against these.
	mcre::PropertyRegistry registry;
	mcre::builtin::addPrototypeProperties(registry);

	// ---- Test 1: good text. Expect: ok, 2 components, 0 problems.
	const std::string goodText = R"json({
		"schemaVersion": 1,
		"components": [
			{ "name": "Fire",  "version": "1", "properties": { "Temperature": 0.95, "Viscosity": 0.1, "Pressure": 0.4, "Expansion": 0.7 } },
			{ "name": "Water", "properties": { "Temperature": 0.2, "Viscosity": 0.6 } }
		]
	})json";
	show("Test 1: good text", mcre::loadComponentsFromText(goodText, "good-inline", registry), registry);

	// ---- Test 2: bad text. Expect: not ok, 0 components, 6 problems.
	const std::string badText = R"json({
		"schemaVersion": 1,
		"components": [
			{ "name": "Fire", "colour": "red",
			  "properties": { "Temperature": 1.7, "Mana": 0.5, "Viscosity": "hot" } },
			{ "name": "Fire", "properties": {} },
			{ "properties": {} }
		]
	})json";
	show("Test 2: bad text", mcre::loadComponentsFromText(badText, "bad-inline", registry), registry);

	// ---- Test 3: text that is not JSON at all. Expect: one 'not valid JSON' problem.
	show("Test 3: broken JSON", mcre::loadComponentsFromText("{ \"components\": [ ", "broken-inline", registry), registry);

	// ---- Test 4: a file that does not exist. Expect: 'cannot open the file'.
	show("Test 4: missing file", mcre::loadComponentsFromFile("this_file_does_not_exist.json", registry), registry);

	// ---- Test 5: a real file. We write one into the temp folder, load it, delete it.
	const std::filesystem::path file = std::filesystem::temp_directory_path() / "mcre_fire_test.json";
	{
		std::ofstream out(file);
		out << goodText;
	}
	show("Test 5: file on disk", mcre::loadComponentsFromFile(file, registry), registry);
	std::filesystem::remove(file);

	// ---- Test 6: loaded components go into the ComponentRegistry and get ids.
	{
		mcre::LoadResult loaded = mcre::loadComponentsFromText(goodText, "good-inline", registry);
		mcre::ComponentRegistry components;
		for (mcre::component& c : loaded.components) {
			const mcre::ComponentId id = components.add(std::move(c), registry);
			std::cout << "\nregistered '" << components.byId(id)->componentName << "' with id " << id;
		}
		std::cout << "\n";
		components.dump(std::cout, registry);
		std::cout << "idOf(Water) = " << components.idOf("Water") << ", idOf(Ice) = " << components.idOf("Ice") << "\n";

		// A duplicate name is refused: add() throws.
		try {
			mcre::component again;
			again.componentName = "Fire";
			components.add(again, registry);
		}
		catch (const std::invalid_argument& e) {
			std::cout << "rejected as expected: " << e.what() << "\n";
		}
	}

	// ---- Test 7: the built-in prototype data (4 properties, Fire, Water) from builtin.cpp.
	{
		mcre::PropertyRegistry prototypeProperties;
		mcre::ComponentRegistry prototypeComponents;
		mcre::builtin::addPrototypeProperties(prototypeProperties);
		mcre::builtin::addPrototypeComponents(prototypeComponents, prototypeProperties);
		std::cout << "\n=== Test 7: built-in prototype data ===\n";
		prototypeProperties.dump(std::cout);
		prototypeComponents.dump(std::cout, prototypeProperties);
	}

	std::cout << "\nPress Enter to exit...";
	std::cin.get();
	return EXIT_SUCCESS;
}