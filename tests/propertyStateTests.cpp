#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "mcre/model/propertyRegistry.hpp"
#include "mcre/model/propertyState.hpp"

using namespace mcre;

// ---------- a new state ----------

TEST(PropertyState, StartsWithEveryValueEmpty) {
	const PropertyState state(4);

	EXPECT_EQ(state.size(), 4u);
	EXPECT_EQ(state.presentCount(), 0u);
	for (PropertyId id = 1; id <= 4; ++id) {
		EXPECT_FALSE(state.has(id)) << "id " << id;
		EXPECT_FALSE(state.get(id).has_value()) << "id " << id;
	}
}

TEST(PropertyState, AStateWithNoSlotsHoldsNothing) {
	PropertyState state(0);

	EXPECT_EQ(state.size(), 0u);
	EXPECT_EQ(state.presentCount(), 0u);
	EXPECT_FALSE(state.has(1));
	EXPECT_THROW(state.set(1, 0.5), std::invalid_argument);
}

// ---------- set() and get() ----------

TEST(PropertyState, SetThenGetGivesTheValueBack) {
	PropertyState state(4);
	state.set(2, 0.75);

	EXPECT_TRUE(state.has(2));
	ASSERT_TRUE(state.get(2).has_value());
	EXPECT_DOUBLE_EQ(*state.get(2), 0.75);
	EXPECT_EQ(state.presentCount(), 1u);
	EXPECT_FALSE(state.has(1));  // the other slots are untouched
	EXPECT_FALSE(state.has(3));
}

TEST(PropertyState, TheFirstAndLastSlotBothWork) {
	PropertyState state(3);
	state.set(1, 0.1);
	state.set(3, 0.3);

	EXPECT_DOUBLE_EQ(*state.get(1), 0.1);
	EXPECT_DOUBLE_EQ(*state.get(3), 0.3);
	EXPECT_FALSE(state.has(2));
}

TEST(PropertyState, SetReplacesAnEarlierValue) {
	PropertyState state(1);
	state.set(1, 0.2);
	state.set(1, 0.9);

	EXPECT_DOUBLE_EQ(*state.get(1), 0.9);
	EXPECT_EQ(state.presentCount(), 1u);
}

TEST(PropertyState, EmptyIsNotTheSameAsZero) {
	PropertyState state(2);
	state.set(1, 0.0);  // a real zero

	EXPECT_TRUE(state.has(1));
	EXPECT_DOUBLE_EQ(*state.get(1), 0.0);
	EXPECT_FALSE(state.has(2));  // never set: empty, not 0
}

TEST(PropertyState, ValuesAreStoredAsGivenWithoutClamping) {
	// The state only holds numbers. Keeping them inside the property bounds is the caller's job.
	PropertyState state(2);
	state.set(1, -0.5);
	state.set(2, 7.0);

	EXPECT_DOUBLE_EQ(*state.get(1), -0.5);
	EXPECT_DOUBLE_EQ(*state.get(2), 7.0);
}

// ---------- valueOr() and presentCount() ----------

TEST(PropertyState, ValueOrUsesTheFallbackOnlyForEmptySlots) {
	PropertyState state(3);
	state.set(1, 0.4);
	state.set(3, 0.0);

	EXPECT_DOUBLE_EQ(state.valueOr(1, -1.0), 0.4);
	EXPECT_DOUBLE_EQ(state.valueOr(3, -1.0), 0.0);   // a stored zero is a value, not a gap
	EXPECT_DOUBLE_EQ(state.valueOr(2, -1.0), -1.0);  // empty
	EXPECT_DOUBLE_EQ(state.valueOr(9, -1.0), -1.0);  // no such slot
}

TEST(PropertyState, PresentCountCountsOnlyFilledSlots) {
	PropertyState state(4);
	EXPECT_EQ(state.presentCount(), 0u);

	state.set(1, 0.1);
	state.set(3, 0.3);
	EXPECT_EQ(state.presentCount(), 2u);

	state.set(3, 0.4);  // replacing a value does not add a slot
	EXPECT_EQ(state.presentCount(), 2u);
}

// ---------- ids and values that are not allowed ----------

TEST(PropertyState, IdsWithoutASlotAreNeverPresent) {
	PropertyState state(3);
	state.set(3, 0.5);

	EXPECT_FALSE(state.has(0));  // 0 is never a real id
	EXPECT_FALSE(state.has(4));
	EXPECT_FALSE(state.get(0).has_value());
	EXPECT_FALSE(state.get(4).has_value());
}

TEST(PropertyState, SetRejectsAnIdWithoutASlot) {
	PropertyState state(3);

	EXPECT_THROW(state.set(0, 0.5), std::invalid_argument);
	EXPECT_THROW(state.set(4, 0.5), std::invalid_argument);
	EXPECT_EQ(state.presentCount(), 0u);
}

TEST(PropertyState, SetRejectsValuesThatAreNotFiniteAndKeepsTheOldValue) {
	PropertyState state(2);
	state.set(1, 0.3);

	EXPECT_THROW(state.set(1, std::nan("")), std::invalid_argument);
	EXPECT_THROW(state.set(1, std::numeric_limits<double>::infinity()), std::invalid_argument);
	EXPECT_THROW(state.set(1, -std::numeric_limits<double>::infinity()), std::invalid_argument);
	EXPECT_DOUBLE_EQ(*state.get(1), 0.3);  // the failed calls changed nothing

	EXPECT_THROW(state.set(2, std::nan("")), std::invalid_argument);
	EXPECT_FALSE(state.has(2));  // an empty slot stays empty
}

// ---------- copying and comparing ----------

TEST(PropertyState, ACopyIsIndependentOfTheOriginal) {
	PropertyState original(2);
	original.set(1, 0.5);

	PropertyState copy = original;
	copy.set(1, 0.9);
	copy.set(2, 0.1);

	EXPECT_DOUBLE_EQ(*original.get(1), 0.5);
	EXPECT_FALSE(original.has(2));
}

TEST(PropertyState, StatesAreEqualWhenSlotsAndValuesMatch) {
	PropertyState a(3);
	PropertyState b(3);
	EXPECT_EQ(a, b);

	a.set(2, 0.5);
	EXPECT_NE(a, b);  // one has a value, the other does not

	b.set(2, 0.5);
	EXPECT_EQ(a, b);

	b.set(2, 0.6);
	EXPECT_NE(a, b);  // different value

	EXPECT_NE(PropertyState(3), PropertyState(4));  // different number of slots
}

// ---------- dump() ----------

TEST(PropertyState, DumpShowsNamesValuesAndNone) {
	PropertyRegistry properties;
	properties.add("Temperature", { 0.0, 1.0 });
	properties.add("Pressure", { 0.0, 1.0 });

	PropertyState state(properties.size());
	state.set(1, 0.95);

	std::ostringstream text;
	state.dump(text, properties);

	EXPECT_NE(text.str().find("Temperature = 0.95"), std::string::npos);
	EXPECT_NE(text.str().find("Pressure = (none)"), std::string::npos);
}

TEST(PropertyState, DumpListsTheSlotsInIdOrder) {
	PropertyRegistry properties;
	properties.add("Temperature", { 0.0, 1.0 });
	properties.add("Viscosity", { 0.0, 1.0 });
	properties.add("Pressure", { 0.0, 1.0 });

	PropertyState state(properties.size());
	std::ostringstream text;
	state.dump(text, properties);
	const std::string out = text.str();

	const auto temperature = out.find("Temperature");
	const auto viscosity = out.find("Viscosity");
	const auto pressure = out.find("Pressure");
	ASSERT_NE(temperature, std::string::npos);
	ASSERT_NE(viscosity, std::string::npos);
	ASSERT_NE(pressure, std::string::npos);
	EXPECT_LT(temperature, viscosity);
	EXPECT_LT(viscosity, pressure);
}

TEST(PropertyState, DumpShowsAQuestionMarkForASlotWithoutAProperty) {
	PropertyRegistry properties;
	properties.add("Temperature", { 0.0, 1.0 });

	PropertyState state(2);  // one slot more than the registry has properties
	state.set(2, 0.5);

	std::ostringstream text;
	state.dump(text, properties);

	EXPECT_NE(text.str().find("? = 0.5"), std::string::npos);
}
