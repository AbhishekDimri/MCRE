#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "mcre/model/builtin.hpp"
#include "mcre/model/propertyRegistry.hpp"
#include "mcre/model/propertyState.hpp"
#include "mcre/model/stateNames.hpp"

using namespace mcre;

namespace {
	// Temperature (1), Viscosity (2), Pressure (3), Expansion (4)
	PropertyRegistry makeProperties() {
		PropertyRegistry properties;
		builtin::addPrototypeProperties(properties);
		return properties;
	}

	// The same four properties added in the opposite order, so every id is different.
	PropertyRegistry makeReversedProperties() {
		PropertyRegistry properties;
		properties.add("Expansion", { -1.0, 1.0 });
		properties.add("Pressure", { 0.0, 1.0 });
		properties.add("Viscosity", { 0.0, 1.0 });
		properties.add("Temperature", { -1.0, 1.0 });
		return properties;
	}
}

// ---------- state to names ----------

TEST(StateNames, ExportsOnlyTheFilledSlotsInIdOrder) {
	const PropertyRegistry properties = makeProperties();
	PropertyState state(properties.size());
	state.set(properties.idOf("Expansion"), 0.5);      // set out of order on purpose
	state.set(properties.idOf("Temperature"), -0.25);

	const NamedValues named = toNamedValues(state, properties);

	ASSERT_EQ(named.size(), 2u);
	EXPECT_EQ(named[0].first, "Temperature");
	EXPECT_DOUBLE_EQ(named[0].second, -0.25);
	EXPECT_EQ(named[1].first, "Expansion");
	EXPECT_DOUBLE_EQ(named[1].second, 0.5);
}

TEST(StateNames, AnEmptyStateExportsNothing) {
	const PropertyRegistry properties = makeProperties();

	EXPECT_TRUE(toNamedValues(PropertyState(properties.size()), properties).empty());
}

TEST(StateNames, RejectsAStateThatBelongsToAnotherRegistry) {
	const PropertyRegistry properties = makeProperties();

	EXPECT_THROW(static_cast<void>(toNamedValues(PropertyState(3), properties)), std::invalid_argument);
	EXPECT_THROW(static_cast<void>(toNamedValues(PropertyState(5), properties)), std::invalid_argument);
}

// ---------- names to state ----------

TEST(StateNames, NothingNamedGivesAnEmptyStateWithOneSlotPerProperty) {
	const PropertyRegistry properties = makeProperties();
	const PropertyState state = fromNamedValues({}, properties);

	EXPECT_EQ(state.size(), 4u);
	EXPECT_EQ(state.presentCount(), 0u);
}

TEST(StateNames, RejectsAnUnknownNameAndSaysWhichOne) {
	const PropertyRegistry properties = makeProperties();

	try {
		static_cast<void>(fromNamedValues({ { "Temprature", 0.5 } }, properties));
		FAIL() << "expected std::invalid_argument";
	}
	catch (const std::invalid_argument& e) {
		EXPECT_NE(std::string(e.what()).find("Temprature"), std::string::npos);   // the misspelt name is in the message
	}
}

TEST(StateNames, RejectsANameListedTwice) {
	const PropertyRegistry properties = makeProperties();

	EXPECT_THROW(static_cast<void>(fromNamedValues({ { "Viscosity", 0.1 }, { "Viscosity", 0.2 } }, properties)), std::invalid_argument);
}

TEST(StateNames, RejectsAValueThatIsNotFinite) {
	const PropertyRegistry properties = makeProperties();

	EXPECT_THROW(static_cast<void>(fromNamedValues({ { "Viscosity", std::nan("") } }, properties)), std::invalid_argument);
	EXPECT_THROW(static_cast<void>(fromNamedValues({ { "Viscosity", std::numeric_limits<double>::infinity() } }, properties)), std::invalid_argument);
}

// ---------- the round trip ----------

TEST(StateNames, ARoundTripKeepsValuesAndEmptySlots) {
	const PropertyRegistry properties = makeProperties();
	PropertyState original(properties.size());
	original.set(properties.idOf("Temperature"), 0.5333);
	original.set(properties.idOf("Pressure"), 0.0);    // a real zero

	const PropertyState copy = fromNamedValues(toNamedValues(original, properties), properties);

	EXPECT_EQ(copy, original);
	EXPECT_TRUE(copy.has(properties.idOf("Pressure")));      // a zero stays a value
	EXPECT_FALSE(copy.has(properties.idOf("Viscosity")));    // empty stays empty
}

TEST(StateNames, NamesStillMeetTheRightValuesWhenTheIdsAreDifferent) {
	// This is the reason ids are never saved: the same names have other ids in the second registry.
	const PropertyRegistry first = makeProperties();
	const PropertyRegistry second = makeReversedProperties();
	ASSERT_NE(first.idOf("Temperature"), second.idOf("Temperature"));

	PropertyState original(first.size());
	original.set(first.idOf("Temperature"), 0.5);
	original.set(first.idOf("Viscosity"), 0.25);

	const PropertyState moved = fromNamedValues(toNamedValues(original, first), second);

	EXPECT_DOUBLE_EQ(*moved.get(second.idOf("Temperature")), 0.5);
	EXPECT_DOUBLE_EQ(*moved.get(second.idOf("Viscosity")), 0.25);
	EXPECT_FALSE(moved.has(second.idOf("Pressure")));
	EXPECT_FALSE(moved.has(second.idOf("Expansion")));
}
