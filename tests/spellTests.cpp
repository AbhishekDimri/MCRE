#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include "mcre/model/builtin.hpp"
#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/propertyRegistry.hpp"
#include "mcre/model/spell.hpp"

using namespace mcre;

namespace {
	const double kNaN = std::numeric_limits<double>::quiet_NaN();
	const double kInf = std::numeric_limits<double>::infinity();

	// The built-in elements: Fire has id 1 and Water has id 2.
	ComponentRegistry makeComponents() {
		PropertyRegistry properties;
		ComponentRegistry components;
		builtin::addPrototypeProperties(properties);
		builtin::addPrototypeComponents(components, properties);
		return components;
	}

	// 2 Fire + 1 Water
	Spell makeSpell() {
		Spell spell;
		spell.ingredients = { { 1, 2.0 }, { 2, 1.0 } };
		return spell;
	}
}

// ---------- a good spell ----------

TEST(Spell, ANewSpellStartsWithNothingAndABudgetOfOne) {
	const Spell spell;

	EXPECT_TRUE(spell.ingredients.empty());
	EXPECT_TRUE(spell.amplify.empty());
	EXPECT_DOUBLE_EQ(spell.energyBudget, 1.0);
}

TEST(Spell, AValidSpellPasses) {
	const ComponentRegistry components = makeComponents();
	Spell spell = makeSpell();
	spell.amplify = { { 1, 1.2 } };
	spell.energyBudget = 1.25;

	EXPECT_NO_THROW(validateSpell(spell, components));
}

TEST(Spell, TheSameComponentMayAppearMoreThanOnce) {
	const ComponentRegistry components = makeComponents();
	Spell spell;
	spell.ingredients = { { 1, 1.0 }, { 1, 1.0 } };

	EXPECT_NO_THROW(validateSpell(spell, components));
}

// ---------- ingredients ----------

TEST(Spell, ASpellNeedsAtLeastOneIngredient) {
	const ComponentRegistry components = makeComponents();

	EXPECT_THROW(validateSpell(Spell{}, components), std::invalid_argument);
}

TEST(Spell, RejectsAnIngredientThatDoesNotExist) {
	const ComponentRegistry components = makeComponents();
	Spell spell = makeSpell();

	spell.ingredients.push_back({ 0, 1.0 });    // 0 is never a real id
	EXPECT_THROW(validateSpell(spell, components), std::invalid_argument);

	spell.ingredients.back() = { 99, 1.0 };
	EXPECT_THROW(validateSpell(spell, components), std::invalid_argument);
}

TEST(Spell, RejectsAnAmountThatIsNotAFiniteNumberAboveZero) {
	const ComponentRegistry components = makeComponents();

	for (double amount : { 0.0, -1.0, kNaN, kInf }) {
		Spell spell;
		spell.ingredients = { { 1, amount } };
		EXPECT_THROW(validateSpell(spell, components), std::invalid_argument) << "amount " << amount;
	}
}

TEST(Spell, TheErrorMessageNamesWhatIsWrong) {
	const ComponentRegistry components = makeComponents();

	try {
		validateSpell(Spell{}, components);
		FAIL() << "expected std::invalid_argument";
	}
	catch (const std::invalid_argument& e) {
		EXPECT_NE(std::string(e.what()).find("ingredient"), std::string::npos);
	}
}

// ---------- amplify ----------

TEST(Spell, AmplifyingAPropertyThatDoesNotExistIsNotAnError) {
	// evaluate() ignores it and adds a warning to the result (design document, amendment A7).
	const ComponentRegistry components = makeComponents();
	Spell spell = makeSpell();
	spell.amplify = { { 99, 1.2 }, { 0, 1.2 } };

	EXPECT_NO_THROW(validateSpell(spell, components));
}

TEST(Spell, RejectsAnAmplifyFactorThatIsNegativeOrNotFinite) {
	const ComponentRegistry components = makeComponents();

	for (double factor : { -0.1, kNaN, kInf }) {
		Spell spell = makeSpell();
		spell.amplify = { { 1, factor } };
		EXPECT_THROW(validateSpell(spell, components), std::invalid_argument) << "factor " << factor;
	}
}

TEST(Spell, AnAmplifyFactorOfZeroIsAllowed) {
	const ComponentRegistry components = makeComponents();
	Spell spell = makeSpell();
	spell.amplify = { { 1, 0.0 } };   // silences the property

	EXPECT_NO_THROW(validateSpell(spell, components));
}

TEST(Spell, RejectsTheSamePropertyAmplifiedTwice) {
	// Two factors would be multiplied in an order that could change the last digits.
	const ComponentRegistry components = makeComponents();
	Spell spell = makeSpell();
	spell.amplify = { { 1, 1.2 }, { 1, 1.1 } };

	EXPECT_THROW(validateSpell(spell, components), std::invalid_argument);
}

// ---------- energy budget ----------

TEST(Spell, RejectsAnEnergyBudgetThatIsNotAFiniteNumberAboveZero) {
	const ComponentRegistry components = makeComponents();

	for (double budget : { 0.0, -1.0, kNaN, kInf }) {
		Spell spell = makeSpell();
		spell.energyBudget = budget;
		EXPECT_THROW(validateSpell(spell, components), std::invalid_argument) << "budget " << budget;
	}
}
