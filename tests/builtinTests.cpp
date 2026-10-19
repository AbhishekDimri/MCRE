#include <gtest/gtest.h>

#include <stdexcept>

#include "mcre/model/builtin.hpp"
#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/propertyRegistry.hpp"

using namespace mcre;

TEST(Builtin, PrototypePropertiesGetTheExpectedIds) {
	PropertyRegistry properties;
	builtin::addPrototypeProperties(properties);

	EXPECT_EQ(properties.size(), 4u);
	EXPECT_EQ(properties.idOf("Temperature"), 1u);
	EXPECT_EQ(properties.idOf("Viscosity"), 2u);
	EXPECT_EQ(properties.idOf("Pressure"), 3u);
	EXPECT_EQ(properties.idOf("Expansion"), 4u);
}

TEST(Builtin, PrototypePropertiesAreNormalizedAndWeightedMean) {
	PropertyRegistry properties;
	builtin::addPrototypeProperties(properties);

	for (const property& p : properties.all()) {
		EXPECT_DOUBLE_EQ(p.bounds.lower, 0.0) << p.propertyName;
		EXPECT_DOUBLE_EQ(p.bounds.upper, 1.0) << p.propertyName;
		EXPECT_EQ(p.aggregationKind, AggregationKind::WeightedMean) << p.propertyName;
	}
}

TEST(Builtin, PrototypeComponentsAreFireAndWater) {
	PropertyRegistry properties;
	ComponentRegistry components;
	builtin::addPrototypeProperties(properties);
	builtin::addPrototypeComponents(components, properties);

	EXPECT_EQ(components.size(), 2u);
	EXPECT_EQ(components.idOf("Fire"), 1u);
	EXPECT_EQ(components.idOf("Water"), 2u);
	EXPECT_EQ(components.byName("Fire")->version, "0.1.0");
}

TEST(Builtin, FireAndWaterHaveTheValuesFromTheDesignDocument) {
	PropertyRegistry properties;
	ComponentRegistry components;
	builtin::addPrototypeProperties(properties);
	builtin::addPrototypeComponents(components, properties);

	const PropertyId temperature = properties.idOf("Temperature");
	const PropertyId viscosity = properties.idOf("Viscosity");
	const PropertyId expansion = properties.idOf("Expansion");

	const component* fire = components.byName("Fire");
	const component* water = components.byName("Water");
	ASSERT_NE(fire, nullptr);
	ASSERT_NE(water, nullptr);

	EXPECT_DOUBLE_EQ(fire->properties.at(temperature), 0.95);
	EXPECT_DOUBLE_EQ(fire->properties.at(viscosity), 0.05);
	EXPECT_DOUBLE_EQ(fire->properties.at(expansion), 0.72);
	EXPECT_DOUBLE_EQ(water->properties.at(temperature), 0.20);
	EXPECT_DOUBLE_EQ(water->properties.at(viscosity), 0.78);
	EXPECT_DOUBLE_EQ(water->properties.at(expansion), 0.12);
}

TEST(Builtin, FireAndWaterDoNotStorePressure) {
	PropertyRegistry properties;
	ComponentRegistry components;
	builtin::addPrototypeProperties(properties);
	builtin::addPrototypeComponents(components, properties);

	const PropertyId pressure = properties.idOf("Pressure");
	EXPECT_EQ(components.byName("Fire")->properties.count(pressure), 0u);   // absent, not 0
	EXPECT_EQ(components.byName("Water")->properties.count(pressure), 0u);
}

TEST(Builtin, CallingTheSameFunctionTwiceThrows) {
	PropertyRegistry properties;
	ComponentRegistry components;
	builtin::addPrototypeProperties(properties);
	builtin::addPrototypeComponents(components, properties);

	EXPECT_THROW(builtin::addPrototypeProperties(properties), std::invalid_argument);        // names exist already
	EXPECT_THROW(builtin::addPrototypeComponents(components, properties), std::invalid_argument);
}

TEST(Builtin, ComponentsNeedTheirPropertiesFirst) {
	const PropertyRegistry noProperties;
	ComponentRegistry components;

	EXPECT_THROW(builtin::addPrototypeComponents(components, noProperties), std::logic_error);
	EXPECT_EQ(components.size(), 0u);
}
