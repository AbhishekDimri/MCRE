#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "mcre/model/spellBuilder.hpp"
#include "modelHelpers.hpp"

using namespace mcre;
using namespace testmodel;

namespace {
	constexpr double kInf = std::numeric_limits<double>::infinity();
	constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
}

// ---------- combine ----------

TEST(SpellBuilder, CombineAddsAnIngredientByName) {
	const Engine engine = build(demo());

	const Spell spell = SpellBuilder(engine).combine("Fire", 2.0).combine("Water").build();

	ASSERT_EQ(spell.ingredients.size(), 2u);
	EXPECT_EQ(spell.ingredients[0].component, engine.components().idOf("Fire"));
	EXPECT_EQ(spell.ingredients[0].amount, 2.0);
	EXPECT_EQ(spell.ingredients[1].component, engine.components().idOf("Water"));
	EXPECT_EQ(spell.ingredients[1].amount, 1.0);   // the default amount
	EXPECT_TRUE(spell.amplify.empty());
	EXPECT_EQ(spell.energyBudget, 1.0);
}

TEST(SpellBuilder, TheSameComponentMayBeCombinedTwice) {
	const Engine engine = build(demo());

	const Spell spell = SpellBuilder(engine).combine("Fire").combine("Fire").build();

	EXPECT_EQ(spell.ingredients.size(), 2u);
}

TEST(SpellBuilder, CombineWithAnUnknownComponentThrowsAndNamesIt) {
	const Engine engine = build(demo());
	SpellBuilder builder(engine);

	try {
		builder.combine("Frie");
		FAIL() << "expected std::invalid_argument";
	}
	catch (const std::invalid_argument& e) {
		EXPECT_NE(std::string(e.what()).find("Frie"), std::string::npos);
	}
}

TEST(SpellBuilder, CombineRejectsAnAmountThatIsNotAFiniteNumberAboveZero) {
	const Engine engine = build(demo());
	SpellBuilder builder(engine);

	EXPECT_THROW(builder.combine("Fire", 0.0), std::invalid_argument);
	EXPECT_THROW(builder.combine("Fire", -1.0), std::invalid_argument);
	EXPECT_THROW(builder.combine("Fire", kNaN), std::invalid_argument);
	EXPECT_THROW(builder.combine("Fire", kInf), std::invalid_argument);
}

TEST(SpellBuilder, AFailedCallLeavesTheBuilderAsItWas) {
	const Engine engine = build(demo());
	SpellBuilder builder(engine);
	builder.combine("Fire");

	EXPECT_THROW(builder.combine("Frie"), std::invalid_argument);
	EXPECT_THROW(builder.combine("Water", -1.0), std::invalid_argument);

	EXPECT_EQ(builder.build().ingredients.size(), 1u);
}

// ---------- amplify ----------

TEST(SpellBuilder, AmplifyByName) {
	const Engine engine = build(demo());

	const Spell spell = SpellBuilder(engine).combine("Fire").amplify("Temperature", 1.2).build();

	ASSERT_EQ(spell.amplify.size(), 1u);
	EXPECT_EQ(spell.amplify[0].property, engine.properties().idOf("Temperature"));
	EXPECT_EQ(spell.amplify[0].factor, 1.2);
}

TEST(SpellBuilder, AmplifyAgainForTheSamePropertyReplacesTheFactor) {
	const Engine engine = build(demo());

	const Spell spell = SpellBuilder(engine).combine("Fire")
		.amplify("Temperature", 1.2).amplify("Pressure", 2.0).amplify("Temperature", 1.5).build();

	ASSERT_EQ(spell.amplify.size(), 2u);   // validateSpell would reject two entries for one property
	EXPECT_EQ(spell.amplify[0].factor, 1.5);
	EXPECT_EQ(spell.amplify[1].factor, 2.0);
}

TEST(SpellBuilder, AmplifyByZeroIsAllowedButNotANegativeOrNonFiniteFactor) {
	const Engine engine = build(demo());
	SpellBuilder builder(engine);

	EXPECT_NO_THROW(builder.amplify("Temperature", 0.0));
	EXPECT_THROW(builder.amplify("Temperature", -0.1), std::invalid_argument);
	EXPECT_THROW(builder.amplify("Temperature", kNaN), std::invalid_argument);
	EXPECT_THROW(builder.amplify("Temperature", kInf), std::invalid_argument);
}

TEST(SpellBuilder, AmplifyOfAnUnknownPropertyIsLeftOutAndCastAddsAWarning) {
	const Engine engine = build(demo());
	SpellBuilder builder(engine);
	builder.combine("Fire", 2.0).combine("Water").amplify("Tempature", 1.2).amplify("Tempature", 1.5);

	EXPECT_TRUE(builder.build().amplify.empty());

	const Result result = builder.cast();
	ASSERT_EQ(result.warnings.size(), 1u);   // once, however often it was asked for
	EXPECT_EQ(result.warnings[0].where, "amplify");
	EXPECT_NE(result.warnings[0].message.find("Tempature"), std::string::npos);

	const Result plain = SpellBuilder(engine).combine("Fire", 2.0).combine("Water").cast();
	EXPECT_EQ(result.state, plain.state);
}

// ---------- concentrate ----------

TEST(SpellBuilder, ConcentrateSetsTheEnergyBudgetToOneOverC) {
	const Engine engine = build(demo());

	EXPECT_EQ(SpellBuilder(engine).combine("Fire").concentrate(0.8).build().energyBudget, 1.0 / 0.8);
	EXPECT_EQ(SpellBuilder(engine).combine("Fire").concentrate(2.0).build().energyBudget, 0.5);
	EXPECT_EQ(SpellBuilder(engine).combine("Fire").concentrate(1.0).build().energyBudget, 1.0);
}

TEST(SpellBuilder, ConcentrateRejectsAnythingThatIsNotAFiniteNumberAboveZero) {
	const Engine engine = build(demo());
	SpellBuilder builder(engine);

	EXPECT_THROW(builder.concentrate(0.0), std::invalid_argument);
	EXPECT_THROW(builder.concentrate(-1.0), std::invalid_argument);
	EXPECT_THROW(builder.concentrate(kNaN), std::invalid_argument);
	EXPECT_THROW(builder.concentrate(kInf), std::invalid_argument);
}

// ---------- build and cast ----------

TEST(SpellBuilder, BuildWithoutIngredientsThrows) {
	const Engine engine = build(demo());

	EXPECT_THROW(static_cast<void>(SpellBuilder(engine).build()), std::invalid_argument);
	EXPECT_THROW(static_cast<void>(SpellBuilder(engine).amplify("Temperature", 1.2).cast()), std::invalid_argument);
}

TEST(SpellBuilder, CastIsTheSameAsEvaluatingTheBuiltSpell) {
	const Engine engine = build(demo());
	SpellBuilder builder(engine);
	builder.combine("Fire", 2.0).combine("Water").amplify("Temperature", 1.2).concentrate(0.8);

	const Result cast = builder.cast();
	const Result direct = engine.evaluate(builder.build());

	EXPECT_EQ(cast.state, direct.state);
	EXPECT_EQ(cast.trace.size(), direct.trace.size());
	EXPECT_NEAR(valueOf(engine, cast, "Temperature"), 0.64, 1e-12);   // amplified, as in the reference
	EXPECT_NEAR(traceOf(engine, cast, "Pressure").base, 0.1375, 1e-12);   // concentrated
}

TEST(SpellBuilder, TheOrderOfAmplifyAndConcentrateDoesNotMatter) {
	const Engine engine = build(demo());

	const Result a = SpellBuilder(engine).combine("Fire", 2.0).combine("Water").amplify("Temperature", 1.2).concentrate(0.8).cast();
	const Result b = SpellBuilder(engine).concentrate(0.8).combine("Fire", 2.0).amplify("Temperature", 1.2).combine("Water").cast();

	EXPECT_EQ(a.state, b.state);
}

TEST(SpellBuilder, TheResultCanBeTurnedIntoNames) {
	// The way a game reads a result: by name, never by id.
	const Engine engine = build(demo());

	const Result result = SpellBuilder(engine).combine("Fire", 2.0).combine("Water").cast();

	EXPECT_NEAR(valueOf(engine, result, "Pressure"), 0.7023022652577184, 1e-10);
	EXPECT_NEAR(valueOf(engine, result, "Expansion"), 0.8656765027937234, 1e-10);
}
