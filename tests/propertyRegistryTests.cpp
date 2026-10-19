#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "mcre/model/propertyRegistry.hpp"

using namespace mcre;

namespace {
	// A short way to build an edge: "source affects target".
	InfluenceEdge makeEdge(PropertyId target, Mechanism mechanism, double coefficient, const std::string& reason) {
		InfluenceEdge edge;
		edge.target = target;
		edge.mechanism = mechanism;
		edge.coefficient = coefficient;
		edge.reason = reason;
		return edge;
	}

	// Four properties with ids 1 to 4: Temperature, Viscosity, Pressure, Expansion (all [0, 1]).
	PropertyRegistry makePrototype() {
		PropertyRegistry registry;
		registry.add("Temperature", { 0.0, 1.0 });
		registry.add("Viscosity", { 0.0, 1.0 });
		registry.add("Pressure", { 0.0, 1.0 });
		registry.add("Expansion", { 0.0, 1.0 });
		return registry;
	}
}

// ---------- add() and lookups ----------

TEST(PropertyRegistry, IdsStartAtOneAndCountUp) {
	PropertyRegistry registry;
	EXPECT_EQ(registry.add("Temperature", { 0.0, 1.0 }), 1u);
	EXPECT_EQ(registry.add("Pressure", { -1.0, 1.0 }), 2u);
	EXPECT_EQ(registry.size(), 2u);
}

TEST(PropertyRegistry, FindsPropertiesByNameAndId) {
	PropertyRegistry registry;
	registry.add("Strength", { 0.0, 10.0 }, AggregationKind::Sum);

	const property* p = registry.byName("Strength");
	ASSERT_NE(p, nullptr);  // ASSERT stops this test at once, so the lines below can trust p
	EXPECT_EQ(p->propertyId, 1u);
	EXPECT_EQ(p->propertyName, "Strength");
	EXPECT_DOUBLE_EQ(p->bounds.upper, 10.0);
	EXPECT_EQ(p->aggregationKind, AggregationKind::Sum);
	EXPECT_EQ(registry.byId(1), p);
	EXPECT_EQ(registry.idOf("Strength"), 1u);
}

TEST(PropertyRegistry, UnknownLookupsGiveNullOrZero) {
	PropertyRegistry registry = makePrototype();

	EXPECT_EQ(registry.byName("Mana"), nullptr);
	EXPECT_EQ(registry.byName("temperature"), nullptr);  // names are case sensitive
	EXPECT_EQ(registry.idOf("Mana"), 0u);
	EXPECT_EQ(registry.byId(0), nullptr);                // 0 is never a real id
	EXPECT_EQ(registry.byId(5), nullptr);
}

TEST(PropertyRegistry, RejectsBadInputAndStaysUnchanged) {
	PropertyRegistry registry;
	registry.add("Pressure", { 0.0, 1.0 });

	const double inf = std::numeric_limits<double>::infinity();
	EXPECT_THROW(registry.add("", { 0.0, 1.0 }), std::invalid_argument);          // empty name
	EXPECT_THROW(registry.add("Pressure", { 0.0, 1.0 }), std::invalid_argument);  // duplicate name
	EXPECT_THROW(registry.add("Mana", { 5.0, 1.0 }), std::invalid_argument);      // lower > upper
	EXPECT_THROW(registry.add("Mana", { 0.0, inf }), std::invalid_argument);      // infinite bound
	EXPECT_THROW(registry.add("Mana", { std::nan(""), 1.0 }), std::invalid_argument);
	EXPECT_EQ(registry.size(), 1u);                                               // nothing half-added
	EXPECT_EQ(registry.byName("Mana"), nullptr);
}

// ---------- addInfluence() ----------

TEST(PropertyRegistry, AddInfluenceStoresTheEdgeOnTheSource) {
	PropertyRegistry registry = makePrototype();

	registry.addInfluence(1, makeEdge(2, Mechanism::Inverse, -0.70, "Heat makes liquids flow more easily"));

	const property* temperature = registry.byName("Temperature");
	ASSERT_NE(temperature, nullptr);
	ASSERT_EQ(temperature->influences.size(), 1u);
	EXPECT_EQ(temperature->influences[0].target, 2u);
	EXPECT_EQ(temperature->influences[0].mechanism, Mechanism::Inverse);
	EXPECT_DOUBLE_EQ(temperature->influences[0].coefficient, -0.70);
	EXPECT_TRUE(registry.byName("Viscosity")->influences.empty());  // stored on the SOURCE only
}

TEST(PropertyRegistry, AddInfluenceRejectsUnknownSourceOrTarget) {
	PropertyRegistry registry = makePrototype();

	EXPECT_THROW(registry.addInfluence(9, makeEdge(1, Mechanism::Linear, 0.5, "why")), std::invalid_argument);
	EXPECT_THROW(registry.addInfluence(0, makeEdge(1, Mechanism::Linear, 0.5, "why")), std::invalid_argument);
	EXPECT_THROW(registry.addInfluence(1, makeEdge(9, Mechanism::Linear, 0.5, "why")), std::invalid_argument);
}

TEST(PropertyRegistry, AddInfluenceRejectsASelfLoop) {
	PropertyRegistry registry = makePrototype();
	EXPECT_THROW(registry.addInfluence(1, makeEdge(1, Mechanism::Linear, 0.5, "why")), std::invalid_argument);
}

TEST(PropertyRegistry, AddInfluenceNeedsAReason) {
	PropertyRegistry registry = makePrototype();
	EXPECT_THROW(registry.addInfluence(1, makeEdge(2, Mechanism::Linear, 0.5, "")), std::invalid_argument);
}

TEST(PropertyRegistry, AddInfluenceRejectsNumbersThatAreNotFinite) {
	PropertyRegistry registry = makePrototype();
	const double nan = std::nan("");
	const double inf = std::numeric_limits<double>::infinity();

	EXPECT_THROW(registry.addInfluence(1, makeEdge(2, Mechanism::Linear, nan, "why")), std::invalid_argument);
	EXPECT_THROW(registry.addInfluence(1, makeEdge(2, Mechanism::Linear, inf, "why")), std::invalid_argument);

	InfluenceEdge badThreshold = makeEdge(2, Mechanism::Threshold, 0.5, "why");
	badThreshold.params.threshold = inf;
	EXPECT_THROW(registry.addInfluence(1, badThreshold), std::invalid_argument);

	InfluenceEdge negativeK = makeEdge(2, Mechanism::Saturation, 0.5, "why");
	negativeK.params.k = -1.0;
	EXPECT_THROW(registry.addInfluence(1, negativeK), std::invalid_argument);

	InfluenceEdge nanK = makeEdge(2, Mechanism::Saturation, 0.5, "why");
	nanK.params.k = nan;
	EXPECT_THROW(registry.addInfluence(1, nanK), std::invalid_argument);
}

TEST(PropertyRegistry, AddInfluenceRejectsTheCustomMechanismForNow) {
	PropertyRegistry registry = makePrototype();
	EXPECT_THROW(registry.addInfluence(1, makeEdge(2, Mechanism::Custom, 0.5, "why")), std::invalid_argument);
}

TEST(PropertyRegistry, ExponentialIsRefusedWhenItWouldOverflow) {
	PropertyRegistry registry = makePrototype();

	InfluenceEdge tooSteep = makeEdge(2, Mechanism::Exponential, 0.5, "why");
	tooSteep.params.k = 1000.0;  // exp(1000 * 1) is far beyond what a double can hold
	EXPECT_THROW(registry.addInfluence(1, tooSteep), std::invalid_argument);

	InfluenceEdge fine = makeEdge(2, Mechanism::Exponential, 0.5, "why");
	fine.params.k = 3.0;
	EXPECT_NO_THROW(registry.addInfluence(1, fine));
}

TEST(PropertyRegistry, ARejectedEdgeIsNotStored) {
	PropertyRegistry registry = makePrototype();
	EXPECT_THROW(registry.addInfluence(1, makeEdge(1, Mechanism::Linear, 0.5, "why")), std::invalid_argument);
	EXPECT_TRUE(registry.byId(1)->influences.empty());
}

TEST(PropertyRegistry, SameTargetAndMechanismTwiceIsRefusedButAnotherMechanismIsFine) {
	PropertyRegistry registry = makePrototype();
	registry.addInfluence(1, makeEdge(2, Mechanism::Linear, 0.5, "first"));

	EXPECT_THROW(registry.addInfluence(1, makeEdge(2, Mechanism::Linear, 0.9, "again")), std::invalid_argument);
	EXPECT_NO_THROW(registry.addInfluence(1, makeEdge(2, Mechanism::Saturation, 0.9, "different mechanism")));
	EXPECT_EQ(registry.byId(1)->influences.size(), 2u);
}

TEST(PropertyRegistry, TwoPropertiesMayInfluenceEachOther) {
	PropertyRegistry registry = makePrototype();  // Pressure is 3, Expansion is 4

	registry.addInfluence(3, makeEdge(4, Mechanism::Linear, 0.4, "Pressure pushes things outward"));
	EXPECT_NO_THROW(registry.addInfluence(4, makeEdge(3, Mechanism::Linear, 0.3, "Expansion raises pressure")));
}

// ---------- dump() ----------

TEST(PropertyRegistry, DumpListsEveryProperty) {
	PropertyRegistry registry = makePrototype();

	std::ostringstream text;
	registry.dump(text);

	EXPECT_NE(text.str().find("4 properties"), std::string::npos);
	EXPECT_NE(text.str().find("Temperature"), std::string::npos);
	EXPECT_NE(text.str().find("Expansion"), std::string::npos);
}
