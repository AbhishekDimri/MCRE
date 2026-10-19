// mcre_app: a small command-line demo of the MCRE engine.
//
//   mcre_app                   shows the built-in prototype data (4 properties, Fire and Water)
//   mcre_app components.json   loads your file against the prototype properties and shows the result
//   mcre_app --help            shows this text
//
// Exit code: 0 = success, 1 = the file had problems or something went wrong, 2 = wrong usage.

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <utility>

#include "mcre/model/builtin.hpp"
#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/loader.hpp"
#include "mcre/model/propertyRegistry.hpp"

namespace {
	constexpr int kExitOk = 0;
	constexpr int kExitFailed = 1;
	constexpr int kExitBadUsage = 2;

	void printUsage(std::ostream& out) {
		out << "Usage:\n"
			<< "  mcre_app                   show the built-in prototype data\n"
			<< "  mcre_app <components.json> load a components file and show it\n"
			<< "  mcre_app --help            show this text\n";
	}

	// Loads the file and adds its components to 'components'.
	// Returns false, after printing every problem, when the file is not valid.
	bool loadFile(const char* path, const mcre::PropertyRegistry& properties, mcre::ComponentRegistry& components) {
		mcre::LoadResult loaded = mcre::loadComponentsFromFile(path, properties);

		if (!loaded.ok()) {
			std::cerr << path << " has " << loaded.problems.size() << " problem(s):\n";
			for (const mcre::Diagnostic& d : loaded.problems) {
				std::cerr << "  " << d.where << "  ->  " << d.message << "\n";
			}
			return false;
		}

		for (mcre::component& c : loaded.components) {
			components.add(std::move(c), properties);
		}
		return true;
	}
}

int main(int argc, char* argv[]) {
	if (argc > 2) {
		printUsage(std::cerr);
		return kExitBadUsage;
	}
	if (argc == 2 && (std::string_view(argv[1]) == "--help" || std::string_view(argv[1]) == "-h")) {
		printUsage(std::cout);
		return kExitOk;
	}

	try {
		mcre::PropertyRegistry properties;
		mcre::builtin::addPrototypeProperties(properties);

		mcre::ComponentRegistry components;
		if (argc == 2) {
			if (!loadFile(argv[1], properties, components)) return kExitFailed;
		}
		else {
			mcre::builtin::addPrototypeComponents(components, properties);
		}

		properties.dump(std::cout);
		components.dump(std::cout, properties);
		return kExitOk;
	}
	catch (const std::exception& e) {
		std::cerr << "error: " << e.what() << "\n";
		return kExitFailed;
	}
	catch (...) {
		std::cerr << "error: unknown failure\n";
		return kExitFailed;
	}
}
