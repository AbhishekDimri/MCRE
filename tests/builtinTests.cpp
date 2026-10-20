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

TEST(Builtin, PrototypePropertiesHaveTheRangesAndMixingRulesFromTheDesignDocument) {
	PropertyRegistry properties;
	builtin::addPrototypeProperties(properties);

	const property* temperature = properties.byName("Temperature");
	const property* viscosity = properties.byName("Viscosity");
	const property* pressure = properties.byName("Pressure");
	const property* expansion = properties.byName("Expansion");
	ASSERT_NE(temperature, nullptr);
	ASSERT_NE(viscosity, nullptr);
	ASSERT_NE(pressure, nullptr);
	ASSERT_NE(expansion, nullptr);

	// Temperature and Expansion are signed, because a thing can be cold or shrink.
	EXPECT_DOUBLE_EQ(temperature->bounds.lower, -1.0);
	EXPECT_DOUBLE_EQ(temperature->bounds.upper, 1.0);
	EXPECT_DOUBLE_EQ(expansion->bounds.lower, -1.0);
	EXPECT_DOUBLE_EQ(expansion->bounds.upper, 1.0);

	// Viscosity and Pressure run from 0 to 1.
	EXPECT_DOUBLE_EQ(viscosity->bounds.lower, 0.0);
	EXPECT_DOUBLE_EQ(viscosity->bounds.upper, 1.0);
	EXPECT_DOUBLE_EQ(pressure->bounds.lower, 0.0);
	EXPECT_DOUBLE_EQ(pressure->bounds.upper, 1.0);

	// Pressure adds up with the amounts; the others are blended.
	EXPECT_EQ(pressure->aggregationKind, AggregationKind::Sum);
	EXPECT_EQ(temperature->aggregationKind, AggregationKind::WeightedMean);
	EXPECT_EQ(viscosity->aggregationKind, AggregationKind::WeightedMean);
	EXPECT_EQ(expansion->aggregationKind, AggregationKind::WeightedMean);
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
	const PropertyId pressure = properties.idOf("Pressure");
	const PropertyId expansion = properties.idOf("Expansion");

	const component* fire = components.byName("Fire");
	const component* water = components.byName("Water");
	ASSERT_NE(fire, nullptr);
	ASSERT_NE(water, nullptr);

	EXPECT_DOUBLE_EQ(fire->properties.at(temperature), 0.95);
	EXPECT_DOUBLE_EQ(fire->properties.at(viscosity), 0.05);
	EXPECT_DOUBLE_EQ(fire->properties.at(pressure), 0.04);
	EXPECT_DOUBLE_EQ(fire->properties.at(expansion), 0.72);
	EXPECT_DOUBLE_EQ(water->properties.at(temperature), -0.30);   // Water is cold
	EXPECT_DOUBLE_EQ(water->properties.at(viscosity), 0.78);
	EXPECT_DOUBLE_EQ(water->properties.at(pressure), 0.03);
	EXPECT_DOUBLE_EQ(water->properties.at(expansion), 0.12);
}

TEST(Builtin, EveryPrototypePropertyHasAValueOnBothCards) {
	PropertyRegistry properties;
	ComponentRegistry components;
	builtin::addPrototypeProperties(properties);
	builtin::addPrototypeComponents(components, properties);

	for (const property& p : properties.all()) {
		EXPECT_EQ(components.byName("Fire")->properties.count(p.propertyId), 1u) << p.propertyName;
		EXPECT_EQ(components.byName("Water")->properties.count(p.propertyId), 1u) << p.propertyName;
	}
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
