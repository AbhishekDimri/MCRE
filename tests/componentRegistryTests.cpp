#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/loader.hpp"
#include "mcre/model/propertyRegistry.hpp"

using namespace mcre;

namespace {
	// Temperature = 1, Viscosity = 2 (both [0, 1]) and Pressure = 3 ([-1, 1]).
	PropertyRegistry makeProperties() {
		PropertyRegistry properties;
		properties.add("Temperature", { 0.0, 1.0 });
		properties.add("Viscosity", { 0.0, 1.0 });
		properties.add("Pressure", { -1.0, 1.0 });
		return properties;
	}

	component makeComponent(const std::string& name, const std::map<PropertyId, double>& values = {},
		const std::string& version = "") {
		component c;
		c.componentName = name;
		c.properties = values;
		c.version = version;
		return c;
	}
}

// ---------- add() and lookups ----------

TEST(ComponentRegistry, IdsStartAtOneAndCountUp) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;

	EXPECT_EQ(components.add(makeComponent("Fire"), properties), 1u);
	EXPECT_EQ(components.add(makeComponent("Water"), properties), 2u);
	EXPECT_EQ(components.size(), 2u);
}

TEST(ComponentRegistry, TheCallersOwnIdIsIgnored) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;

	component fire = makeComponent("Fire");
	fire.componentId = 99;

	EXPECT_EQ(components.add(fire, properties), 1u);
	EXPECT_EQ(components.byName("Fire")->componentId, 1u);
}

TEST(ComponentRegistry, FindsComponentsByNameAndId) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;
	components.add(makeComponent("Fire", { {1, 0.95}, {2, 0.1} }, "1"), properties);

	const component* fire = components.byName("Fire");
	ASSERT_NE(fire, nullptr);
	EXPECT_EQ(fire->componentId, 1u);
	EXPECT_EQ(fire->version, "1");
	EXPECT_DOUBLE_EQ(fire->properties.at(1), 0.95);
	EXPECT_EQ(components.byId(1), fire);
	EXPECT_EQ(components.idOf("Fire"), 1u);
}

TEST(ComponentRegistry, UnknownLookupsGiveNullOrZero) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;
	components.add(makeComponent("Fire"), properties);

	EXPECT_EQ(components.byName("Ice"), nullptr);
	EXPECT_EQ(components.byName("fire"), nullptr);  // names are case sensitive
	EXPECT_EQ(components.idOf("Ice"), 0u);
	EXPECT_EQ(components.byId(0), nullptr);         // 0 is never a real id
	EXPECT_EQ(components.byId(2), nullptr);
}

TEST(ComponentRegistry, AComponentWithNoPropertiesIsAllowed) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;

	EXPECT_NO_THROW(components.add(makeComponent("Void"), properties));
	EXPECT_TRUE(components.byName("Void")->properties.empty());
}

// ---------- add() rejects bad components ----------

TEST(ComponentRegistry, RejectsAnEmptyName) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;

	EXPECT_THROW(components.add(makeComponent(""), properties), std::invalid_argument);
}

TEST(ComponentRegistry, RejectsADuplicateName) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;
	components.add(makeComponent("Fire"), properties);

	EXPECT_THROW(components.add(makeComponent("Fire"), properties), std::invalid_argument);
}

TEST(ComponentRegistry, RejectsAPropertyIdThatDoesNotExist) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;

	EXPECT_THROW(components.add(makeComponent("Ghost", { {77, 0.5} }), properties), std::invalid_argument);
	EXPECT_THROW(components.add(makeComponent("Ghost", { {0, 0.5} }), properties), std::invalid_argument);
}

TEST(ComponentRegistry, RejectsValuesOutsideTheBounds) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;

	EXPECT_THROW(components.add(makeComponent("Hot", { {1, 1.5} }), properties), std::invalid_argument);
	EXPECT_THROW(components.add(makeComponent("Cold", { {3, -1.5} }), properties), std::invalid_argument);
}

TEST(ComponentRegistry, ValuesExactlyOnTheBoundsAreAccepted) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;

	EXPECT_NO_THROW(components.add(makeComponent("Edge", { {1, 1.0}, {2, 0.0}, {3, -1.0} }), properties));
}

TEST(ComponentRegistry, RejectsValuesThatAreNotFinite) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;

	EXPECT_THROW(components.add(makeComponent("Nan", { {1, std::nan("")} }), properties), std::invalid_argument);
	EXPECT_THROW(components.add(makeComponent("Inf", { {1, std::numeric_limits<double>::infinity()} }), properties),
		std::invalid_argument);
}

TEST(ComponentRegistry, ARejectedComponentChangesNothing) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;
	components.add(makeComponent("Fire"), properties);

	EXPECT_THROW(components.add(makeComponent("Hot", { {1, 1.5} }), properties), std::invalid_argument);

	EXPECT_EQ(components.size(), 1u);
	EXPECT_EQ(components.byName("Hot"), nullptr);
	EXPECT_EQ(components.add(makeComponent("Water"), properties), 2u);  // the id was not used up
}

// ---------- dump() and the loader ----------

TEST(ComponentRegistry, DumpShowsNamesAndValues) {
	const PropertyRegistry properties = makeProperties();
	ComponentRegistry components;
	components.add(makeComponent("Fire", { {1, 0.95} }, "1"), properties);

	std::ostringstream text;
	components.dump(text, properties);

	EXPECT_NE(text.str().find("1 components"), std::string::npos);
	EXPECT_NE(text.str().find("Fire"), std::string::npos);
	EXPECT_NE(text.str().find("Temperature = 0.95"), std::string::npos);  // id 1 printed as its name
}

TEST(ComponentRegistry, AcceptsComponentsStraightFromTheLoader) {
	const PropertyRegistry properties = makeProperties();
	const LoadResult loaded = loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [
			{"name": "Earth", "properties": {"Temperature": 0.3}},
			{"name": "Wind", "properties": {"Pressure": -0.5}} ]})",
		"inline", properties);
	ASSERT_TRUE(loaded.ok());

	ComponentRegistry components;
	for (const component& c : loaded.components) {
		components.add(c, properties);
	}

	EXPECT_EQ(components.idOf("Earth"), 1u);
	EXPECT_EQ(components.idOf("Wind"), 2u);
	EXPECT_DOUBLE_EQ(components.byName("Wind")->properties.at(properties.idOf("Pressure")), -0.5);
}
