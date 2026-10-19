// loader.cpp: JSON text -> checked components (or a list of problems).
// This is the ONLY file that includes the JSON library.

#include "mcre/model/loader.hpp"

#include <cmath>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <set>
#include <sstream>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "mcre/core/logger.hpp"

namespace mcre {
	// Helper functions
	namespace {
		using json = nlohmann::json;

		// Keys allowed inside ONE component.
		constexpr std::string_view kNameKey = "name";
		constexpr std::string_view kVersionKey = "version";
		constexpr std::string_view kPropertiesKey = "properties";
		constexpr std::array<std::string_view, 3> kComponentKeys = { kNameKey, kVersionKey, kPropertiesKey };

		// Keys allowed at the top level of the file.
		constexpr std::string_view kSchemaKey = "$schema";
		constexpr std::string_view kSchemaVersionKey = "schemaVersion";
		constexpr std::string_view kComponentsKey = "components";
		constexpr std::array<std::string_view, 3> kTopLevelKeys = { kSchemaKey, kSchemaVersionKey, kComponentsKey };

		// 0.95 -> "0.95" (used in messages)
		std::string numberText(double value) {
			std::ostringstream out;
			out << value;
			return out.str();
		}

		// Records one problem in the result, and also writes it to the log file.
		void addProblem(LoadResult& result, std::string where, std::string message) {
			logger::error(where, ": ", message);
			result.problems.push_back({ std::move(where), std::move(message) });
		}

		// Reports every key of 'object' that is not in the 'allowed' list.
		// This catches typos such as "propertys" instead of "properties".
		void rejectUnknownKeys(const json& object, std::span<const std::string_view> allowed,
			const std::string& where, LoadResult& result) {
			for (auto it = object.begin(); it != object.end(); ++it) {
				bool known = false;
				for (const std::string_view name : allowed) {
					if (it.key() == name) known = true;
				}
				if (!known) addProblem(result, where + "/" + it.key(), "unknown key");
			}
		}
	}

	void readComponent(const json& item, std::size_t index, const std::string& source,
		const PropertyRegistry& registry, std::set<std::string>& namesSeen, LoadResult& result) {
		 
		// "where" is the address used in messages, e.g.  fire.json#/components/0
		const std::string where = source + "#/components/" + std::to_string(index);

		if (!item.is_object()) {
			addProblem(result, where, "a component must be an object");
			return;
		}

		rejectUnknownKeys(item, kComponentKeys, where, result);

		component c;  // componentId stays 0: the component registry will assign ids later

		// ---- name: required, non-empty text, and no two components may share a name
		const auto nameIt = item.find("name");

		if (nameIt == item.end() || !nameIt->is_string() || nameIt->get<std::string>().empty()) {
			addProblem(result, where + "/name", "required: a non-empty string");
		}
		else {
			c.componentName = nameIt->get<std::string>();
			if (!namesSeen.insert(c.componentName).second) {  // insert() says false if it was already there
				addProblem(result, where + "/name", "duplicate component name '" + c.componentName + "'");
			}
		}

		// ---- version: optional text
		const auto versionIt = item.find("version");
		if (versionIt != item.end()) {
			if (versionIt->is_string()) c.version = versionIt->get<std::string>();
			else addProblem(result, where + "/version", "must be a string");
		}

		// ---- properties: required object of  "property name" -> number
		const auto propsIt = item.find("properties");
		if (propsIt == item.end() || !propsIt->is_object()) {
			addProblem(result, where + "/properties", "required: an object of property name -> number");
		}
		else {
			for (auto it = propsIt->begin(); it != propsIt->end(); ++it) {
				const std::string here = where + "/properties/" + it.key();

				// 1. does the registry know this name?   (name -> property)
				const property* p = registry.byName(it.key());
				if (p == nullptr) { addProblem(result, here, "unknown property"); continue; }

				// 2. is the value a number?
				if (!it->is_number()) { addProblem(result, here, "must be a number"); continue; }

				// 3. is it a normal number, and inside the property's range?
				const double value = it->get<double>();
				if (!std::isfinite(value)) { addProblem(result, here, "must be a finite number"); continue; }
				if (value < p->bounds.lower || value > p->bounds.upper) {
					addProblem(result, here, "value " + numberText(value) + " is outside [" +
						numberText(p->bounds.lower) + ", " + numberText(p->bounds.upper) + "]");
					continue;
				}

				// 4. all good: store it under the property's id   (name -> id: this is the conversion)
				c.properties[p->propertyId] = value;
			}
		}

		result.components.push_back(std::move(c));  // thrown away at the end if any problem was found
	}

	LoadResult loadComponentsFromText(std::string_view jsonText, std::string_view sourceName,
		const PropertyRegistry& properties) {
		LoadResult result;
		const std::string source(sourceName);

		// ---- 1. parse the text into a json tree
		json doc;
		try {
			doc = json::parse(jsonText.begin(), jsonText.end());
		}
		catch (const json::exception& e) {  // bad syntax, and also numbers like 1e999
			addProblem(result, source, std::string("not valid JSON: ") + e.what());
			return result;
		}

		// ---- 2. check the top level
		if (!doc.is_object()) {
			addProblem(result, source, "the top level must be an object");
			return result;
		}

		rejectUnknownKeys(doc, kTopLevelKeys, source + "#", result);

		const auto versionIt = doc.find("schemaVersion");
		if (versionIt == doc.end() || !versionIt->is_number_integer() || versionIt->get<long long>() != 1) {
			addProblem(result, source + "#/schemaVersion", "required: the number 1");
		}

		const auto componentsIt = doc.find("components");
		if (componentsIt == doc.end() || !componentsIt->is_array()) {
			addProblem(result, source + "#/components", "required: an array of components");
			return result;
		}

		// ---- 3. check every component
		std::set<std::string> namesSeen;
		std::size_t index = 0;
		for (const json& item : *componentsIt) {
			readComponent(item, index, source, properties, namesSeen, result);
			++index;
		}

		// ---- 4. all or nothing
		if (!result.problems.empty()) result.components.clear();
		else logger::info("loaded ", result.components.size(), " component(s) from ", source);
		return result;
	}


	LoadResult loadComponentsFromFile(const std::filesystem::path& file, const PropertyRegistry& properties) {
		std::ifstream in(file, std::ios::binary);
		if (!in) {
			LoadResult result;
			addProblem(result, file.string(), "cannot open the file");
			return result;
		}
		std::ostringstream text;
		text << in.rdbuf();  // read the whole file
		return loadComponentsFromText(text.str(), file.string(), properties);
	}

}
