#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "mcre/model/aggregation.hpp"
#include "mcre/model/builtin.hpp"
#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/propertyRegistry.hpp"

using namespace mcre;

namespace {
	constexpr double kTolerance = 1e-9;

	using Items = std::vector<Contribution>;

	// A short way to write one contribution: component id, value, weight.
	Contribution item(ComponentId component, double value, double weight) {
		Contribution c;
		c.component = component;
		c.value = value;
		c.weight = weight;
		return c;
	}

	// Mixes one property of the built-in elements, using that property's own mixing rule.
	// 'amounts' is a list like { {"Fire", 2}, {"Water", 1} }. An element that has no value for the
	// property simply does not contribute.
	double mixSampleProperty(const std::string& propertyName,
		std::initializer_list<std::pair<const char*, double>> amounts) {
		PropertyRegistry properties;
		ComponentRegistry components;
		builtin::addPrototypeProperties(properties);
		builtin::addPrototypeComponents(components, properties);

		const property* p = properties.byName(propertyName);
		Items items;
		for (const auto& [name, amount] : amounts) {
			const component* c = components.byName(name);
			auto value = c->properties.find(p->propertyId);
			if (value != c->properties.end()) {
				items.push_back(item(c->componentId, value->second, amount));
			}
		}
		return aggregate(p->aggregationKind, items);
	}
}

// ---------- Sum ----------

TEST(Aggregate, SumAddsEachValueTimesItsWeight) {
	EXPECT_NEAR(aggregate(AggregationKind::Sum, { item(1, 0.5, 2.0), item(2, 0.25, 1.0) }), 1.25, kTolerance);
}

TEST(Aggregate, SumKeepsNegativeValues) {
	EXPECT_NEAR(aggregate(AggregationKind::Sum, { item(1, -0.5, 2.0), item(2, 0.25, 1.0) }), -0.75, kTolerance);
}

// ---------- WeightedMean ----------

TEST(Aggregate, WeightedMeanGivesHeavierContributorsMoreSay) {
	// (1.0 * 3 + 0.0 * 1) / (3 + 1) = 0.75
	EXPECT_NEAR(aggregate(AggregationKind::WeightedMean, { item(1, 1.0, 3.0), item(2, 0.0, 1.0) }), 0.75, kTolerance);
}

TEST(Aggregate, WeightedMeanOfEqualWeightsIsTheAverage) {
	EXPECT_NEAR(aggregate(AggregationKind::WeightedMean, { item(1, -0.30, 1.0), item(2, 0.30, 1.0) }), 0.0, kTolerance);
}

TEST(Aggregate, WeightedMeanIgnoresAContributorWithWeightZero) {
	EXPECT_NEAR(aggregate(AggregationKind::WeightedMean, { item(1, 0.4, 1.0), item(2, 0.9, 0.0) }), 0.4, kTolerance);
}

// ---------- Median ----------

TEST(Aggregate, MedianOfThreeEqualWeightsIsTheMiddleValue) {
	EXPECT_NEAR(aggregate(AggregationKind::Median, { item(1, 3.0, 1.0), item(2, 1.0, 1.0), item(3, 2.0, 1.0) }), 2.0, kTolerance);
}

TEST(Aggregate, MedianOfAnEvenSplitTakesTheLowerValue) {
	// Documents the tie rule: the first value whose running weight reaches half the total.
	EXPECT_NEAR(aggregate(AggregationKind::Median, { item(1, 1.0, 1.0), item(2, 3.0, 1.0) }), 1.0, kTolerance);
}

TEST(Aggregate, MedianFollowsTheWeights) {
	// The heavy value (5 of the 6 units of weight) is where the running weight passes half,
	// whether it is the larger value or the smaller one.
	EXPECT_NEAR(aggregate(AggregationKind::Median, { item(1, 0.1, 1.0), item(2, 0.9, 5.0) }), 0.9, kTolerance);
	EXPECT_NEAR(aggregate(AggregationKind::Median, { item(1, 0.1, 5.0), item(2, 0.9, 1.0) }), 0.1, kTolerance);
}

// ---------- nothing to mix ----------

TEST(Aggregate, NoContributorsGivesZeroForEveryKind) {
	EXPECT_DOUBLE_EQ(aggregate(AggregationKind::Sum, {}), 0.0);
	EXPECT_DOUBLE_EQ(aggregate(AggregationKind::WeightedMean, {}), 0.0);
	EXPECT_DOUBLE_EQ(aggregate(AggregationKind::Median, {}), 0.0);
}

TEST(Aggregate, OnlyZeroWeightsGivesZeroAndNotNaN) {
	const Items zeroWeights = { item(1, 0.5, 0.0), item(2, 0.7, 0.0) };

	for (AggregationKind kind : { AggregationKind::Sum, AggregationKind::WeightedMean, AggregationKind::Median }) {
		const double result = aggregate(kind, zeroWeights);
		EXPECT_FALSE(std::isnan(result));
		EXPECT_DOUBLE_EQ(result, 0.0);
	}
}

// ---------- input that is not allowed ----------

TEST(Aggregate, RejectsANegativeWeight) {
	EXPECT_THROW(aggregate(AggregationKind::Sum, { item(1, 0.5, -1.0) }), std::invalid_argument);
}

TEST(Aggregate, RejectsAWeightThatIsNotFinite) {
	EXPECT_THROW(aggregate(AggregationKind::Sum, { item(1, 0.5, std::nan("")) }), std::invalid_argument);
	EXPECT_THROW(aggregate(AggregationKind::Sum, { item(1, 0.5, std::numeric_limits<double>::infinity()) }), std::invalid_argument);
}

TEST(Aggregate, RejectsAValueThatIsNotFinite) {
	EXPECT_THROW(aggregate(AggregationKind::WeightedMean, { item(1, std::nan(""), 1.0) }), std::invalid_argument);
	EXPECT_THROW(aggregate(AggregationKind::Median, { item(1, std::numeric_limits<double>::infinity(), 1.0) }), std::invalid_argument);
}

TEST(Aggregate, AnUnknownKindThrows) {
	EXPECT_THROW(aggregate(static_cast<AggregationKind>(99), { item(1, 0.5, 1.0) }), std::invalid_argument);
}

// ---------- repeatable results ----------

TEST(Aggregate, TheOrderOfTheInputDoesNotChangeTheResult) {
	// Plain addition gives 0.1 + 0.2 + 0.3 = 0.6000000000000001 but 0.3 + 0.2 + 0.1 = 0.6.
	// aggregate() sorts its input first, so every order must give exactly the same number.
	Items items = { item(1, 0.1, 1.0), item(2, 0.2, 1.0), item(3, 0.3, 1.0) };
	const double expected = aggregate(AggregationKind::Sum, items);

	std::sort(items.begin(), items.end(), [](const Contribution& a, const Contribution& b) { return a.component < b.component; });
	do {
		EXPECT_EQ(aggregate(AggregationKind::Sum, items), expected);  // exact, not "near"
	} while (std::next_permutation(items.begin(), items.end(),
		[](const Contribution& a, const Contribution& b) { return a.component < b.component; }));
}

TEST(Aggregate, TheOrderDoesNotMatterForAMeanWithTheSameComponentTwice) {
	const Items forward = { item(1, 0.5, 1.0), item(1, 0.5, 2.0), item(1, 0.2, 1.0) };
	Items backward = forward;
	std::reverse(backward.begin(), backward.end());

	EXPECT_EQ(aggregate(AggregationKind::WeightedMean, forward), aggregate(AggregationKind::WeightedMean, backward));
}

TEST(Aggregate, DoesNotChangeTheCallersList) {
	Items items = { item(3, 0.3, 1.0), item(1, 0.1, 1.0) };
	aggregate(AggregationKind::Sum, items);

	EXPECT_EQ(items[0].component, 3u);  // still in the order the caller gave
	EXPECT_EQ(items[1].component, 1u);
}

// ---------- reference test T-02: mixing 2 Fire + 1 Water from the sample data ----------

TEST(Aggregate, MixingTwoFireAndOneWaterGivesTheDesignDocumentNumbers) {
	const auto mix = { std::pair<const char*, double>{ "Fire", 2.0 }, std::pair<const char*, double>{ "Water", 1.0 } };

	EXPECT_NEAR(mixSampleProperty("Temperature", mix), 1.6 / 3.0, kTolerance);   // 0.5333  blended
	EXPECT_NEAR(mixSampleProperty("Viscosity", mix), 0.88 / 3.0, kTolerance);    // 0.2933  blended
	EXPECT_NEAR(mixSampleProperty("Pressure", mix), 0.11, kTolerance);           // 0.11    summed: 2 * 0.04 + 1 * 0.03
	EXPECT_NEAR(mixSampleProperty("Expansion", mix), 0.52, kTolerance);          // 0.52    blended
}

TEST(Aggregate, TheOrderOfTheElementsDoesNotChangeTheMix) {
	const auto fireFirst = { std::pair<const char*, double>{ "Fire", 2.0 }, std::pair<const char*, double>{ "Water", 1.0 } };
	const auto waterFirst = { std::pair<const char*, double>{ "Water", 1.0 }, std::pair<const char*, double>{ "Fire", 2.0 } };

	for (const char* name : { "Temperature", "Viscosity", "Pressure", "Expansion" }) {
		EXPECT_EQ(mixSampleProperty(name, fireFirst), mixSampleProperty(name, waterFirst)) << name;
	}
}
