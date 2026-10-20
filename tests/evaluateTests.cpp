#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "modelHelpers.hpp"

using namespace mcre;
using namespace testmodel;

// Reference tests T-02 and T-03: mixing, amplify, the energy budget, and influences that are not in a loop.
// The numbers come from the Python reference model that the design document was written with.

namespace {
	constexpr double kTight = 1e-12;

	Engine prototypeEngine() { return build(prototype()); }

	// The prototype plus the influences given by 'add'. The influences here never form a loop.
	template <class AddEdges>
	Engine prototypeWith(AddEdges add) {
		Registries r = prototype();
		add(r.properties);
		return build(std::move(r));
	}
}

// ---------- mixing (T-02) ----------

TEST(Evaluate, MixesTwoFireAndOneWaterLikeTheReference) {
	const Engine engine = prototypeEngine();
	const Result result = engine.evaluate(twoFireOneWater());

	EXPECT_NEAR(valueOf(engine, result, "Temperature"), 1.6 / 3.0, kTight);   // (2 * 0.95 - 0.30) / 3
	EXPECT_NEAR(valueOf(engine, result, "Viscosity"), 0.88 / 3.0, kTight);    // (2 * 0.05 + 0.78) / 3
	EXPECT_NEAR(valueOf(engine, result, "Pressure"), 0.11, kTight);           // summed: 2 * 0.04 + 0.03
	EXPECT_NEAR(valueOf(engine, result, "Expansion"), 0.52, kTight);          // (2 * 0.72 + 0.12) / 3
	EXPECT_TRUE(result.converged);
	EXPECT_TRUE(result.warnings.empty());
}

TEST(Evaluate, EveryPropertyHasAValueAfterwards) {
	const Engine engine = prototypeEngine();
	const Result result = engine.evaluate(twoFireOneWater());

	EXPECT_EQ(result.state.size(), 4u);
	EXPECT_EQ(result.state.presentCount(), 4u);
}

TEST(Evaluate, AnIngredientThatSaysNothingAboutAPropertyDoesNotTakePart) {
	// X knows only A. Y knows A and B. B is mixed from Y alone: X is not "0" for B, it is silent.
	Registries r;
	r.properties.add("A", { 0.0, 1.0 });
	r.properties.add("B", { 0.2, 0.9 });
	r.properties.add("C", { -1.0, 1.0 });
	addComponent(r, "X", { { "A", 0.6 } });
	addComponent(r, "Y", { { "A", 0.2 }, { "B", 0.5 } });
	const Engine engine = build(std::move(r));

	const Result both = engine.evaluate(spellOf({ { 1, 1.0 }, { 2, 1.0 } }));
	EXPECT_NEAR(valueOf(engine, both, "A"), 0.4, kTight);
	EXPECT_NEAR(valueOf(engine, both, "B"), 0.5, kTight);
	EXPECT_NEAR(valueOf(engine, both, "C"), 0.0, kTight);   // nobody has C
}

TEST(Evaluate, APropertyNobodyHasIsZeroForcedIntoItsRange) {
	// Decision Q-09: no value means 0, and 0 is then clamped like any other value.
	Registries r;
	r.properties.add("A", { 0.0, 1.0 });
	r.properties.add("B", { 0.2, 0.9 });
	addComponent(r, "X", { { "A", 0.6 } });
	const Engine engine = build(std::move(r));

	const Result result = engine.evaluate(spellOf({ { 1, 1.0 } }));

	EXPECT_NEAR(valueOf(engine, result, "B"), 0.2, kTight);
}

TEST(Evaluate, TheEnergyBudgetScalesSummedPropertiesOnly) {
	const Engine engine = prototypeEngine();
	Spell spell = twoFireOneWater();
	spell.energyBudget = 1.25;   // concentrate(0.8)

	const Result result = engine.evaluate(spell);

	EXPECT_NEAR(valueOf(engine, result, "Pressure"), 0.1375, kTight);
	EXPECT_NEAR(valueOf(engine, result, "Temperature"), 1.6 / 3.0, kTight);
	EXPECT_NEAR(valueOf(engine, result, "Viscosity"), 0.88 / 3.0, kTight);
	EXPECT_NEAR(valueOf(engine, result, "Expansion"), 0.52, kTight);
}

// ---------- amplify ----------

TEST(Evaluate, AmplifyMultipliesTheValuesBeforeMixing) {
	const Engine engine = prototypeEngine();
	Spell spell = twoFireOneWater();
	spell.amplify = { { 1, 1.2 } };   // Temperature * 1.2

	const Result result = engine.evaluate(spell);

	EXPECT_NEAR(valueOf(engine, result, "Temperature"), 1.6 / 3.0 * 1.2, kTight);   // 0.64
	EXPECT_NEAR(valueOf(engine, result, "Viscosity"), 0.88 / 3.0, kTight);          // nothing else changes
	EXPECT_NEAR(valueOf(engine, result, "Pressure"), 0.11, kTight);
}

TEST(Evaluate, AmplifyAppliesToSummedPropertiesAndTogetherWithTheBudget) {
	const Engine engine = prototypeEngine();
	Spell spell = twoFireOneWater();
	spell.amplify = { { 3, 2.0 } };   // Pressure * 2
	EXPECT_NEAR(valueOf(engine, engine.evaluate(spell), "Pressure"), 0.22, kTight);

	spell.energyBudget = 1.25;
	EXPECT_NEAR(valueOf(engine, engine.evaluate(spell), "Pressure"), 0.275, kTight);   // 0.11 * 2 * 1.25
}

TEST(Evaluate, AmplifyByZeroSwitchesAPropertyOff) {
	const Engine engine = prototypeEngine();
	Spell spell = twoFireOneWater();
	spell.amplify = { { 2, 0.0 } };   // Viscosity * 0

	EXPECT_NEAR(valueOf(engine, engine.evaluate(spell), "Viscosity"), 0.0, kTight);
}

TEST(Evaluate, AnAmplifiedValueIsForcedIntoTheRangeLikeAnyOther) {
	const Engine engine = prototypeEngine();
	Spell spell = twoFireOneWater();
	spell.amplify = { { 1, 3.0 } };   // 1.6 would be outside [-1, 1]

	const Result result = engine.evaluate(spell);

	EXPECT_EQ(valueOf(engine, result, "Temperature"), 1.0);
	EXPECT_EQ(traceOf(engine, result, "Temperature").base, 1.0);
}

TEST(Evaluate, AmplifyOfAPropertyThatDoesNotExistIsIgnoredWithAWarning) {
	const Engine engine = prototypeEngine();
	const Result plain = engine.evaluate(twoFireOneWater());

	Spell spell = twoFireOneWater();
	spell.amplify = { { 99, 2.0 }, { 0, 2.0 } };
	const Result result = engine.evaluate(spell);

	EXPECT_EQ(result.state, plain.state);
	EXPECT_TRUE(result.converged);
	ASSERT_EQ(result.warnings.size(), 2u);
	EXPECT_EQ(result.warnings[0].where, "amplify");
	EXPECT_NE(result.warnings[0].message.find("99"), std::string::npos);
	EXPECT_NE(result.warnings[1].message.find("0"), std::string::npos);
}

// ---------- the spell ----------

TEST(Evaluate, TheSameIngredientTwiceIsTheSameAsDoubleTheAmount) {
	const Engine engine = prototypeEngine();

	const Result twice = engine.evaluate(spellOf({ { 1, 1.0 }, { 1, 1.0 }, { 2, 1.0 } }));
	const Result doubled = engine.evaluate(spellOf({ { 1, 2.0 }, { 2, 1.0 } }));

	for (const char* name : { "Temperature", "Viscosity", "Pressure", "Expansion" }) {
		EXPECT_NEAR(valueOf(engine, twice, name), valueOf(engine, doubled, name), 1e-15) << name;
	}
}

TEST(Evaluate, TheOrderOfTheIngredientsDoesNotChangeTheResult) {
	const Engine engine = prototypeEngine();

	const Result fireFirst = engine.evaluate(spellOf({ { 1, 2.0 }, { 2, 1.0 } }));
	const Result waterFirst = engine.evaluate(spellOf({ { 2, 1.0 }, { 1, 2.0 } }));

	EXPECT_EQ(fireFirst.state, waterFirst.state);   // exactly equal, not just close
}

TEST(Evaluate, EvaluatingTwiceGivesExactlyTheSameAnswer) {
	const Engine engine = build(demo());

	const Result first = engine.evaluate(twoFireOneWater());
	const Result second = engine.evaluate(twoFireOneWater());

	EXPECT_EQ(first.state, second.state);
	EXPECT_EQ(first.trace.size(), second.trace.size());
}

TEST(Evaluate, ABadSpellIsStillRejected) {
	const Engine engine = prototypeEngine();

	EXPECT_THROW(engine.evaluate(Spell{}), std::invalid_argument);
	EXPECT_THROW(engine.evaluate(spellOf({ { 9, 1.0 } })), std::invalid_argument);
}

// ---------- the trace ----------

TEST(EvaluateTrace, HasOneEntryPerPropertyAndNothingToAddWithoutInfluences) {
	const Engine engine = prototypeEngine();
	const Result result = engine.evaluate(twoFireOneWater());

	ASSERT_EQ(result.trace.size(), 4u);
	for (std::size_t i = 0; i < result.trace.size(); ++i) {
		const TraceEntry& entry = result.trace[i];
		EXPECT_EQ(entry.property, static_cast<PropertyId>(i + 1));
		EXPECT_TRUE(entry.pushes.empty());
		EXPECT_EQ(entry.raw, entry.base);
		EXPECT_EQ(entry.clamped, entry.base);
		EXPECT_EQ(entry.repeat, 0);
		EXPECT_EQ(result.state.get(entry.property), entry.clamped);
	}
}

// ---------- influences (T-03) ----------

TEST(EvaluateInfluence, AnInfluenceAddsItsPushToTheBase) {
	// Heat thins liquids: Temperature -> Viscosity, Linear, -0.5.
	const Engine engine = prototypeWith([](PropertyRegistry& p) { addEdge(p, "Temperature", "Viscosity", -0.5); });

	const Result result = engine.evaluate(twoFireOneWater());

	EXPECT_NEAR(valueOf(engine, result, "Viscosity"), 0.88 / 3.0 - 0.5 * (1.6 / 3.0), kTight);   // 0.0267
	EXPECT_NEAR(valueOf(engine, result, "Temperature"), 1.6 / 3.0, kTight);                      // the source is untouched

	const TraceEntry& entry = traceOf(engine, result, "Viscosity");
	EXPECT_NEAR(entry.base, 0.88 / 3.0, kTight);
	ASSERT_EQ(entry.pushes.size(), 1u);
	EXPECT_EQ(entry.pushes[0].source, engine.properties().idOf("Temperature"));
	EXPECT_EQ(entry.pushes[0].mechanism, Mechanism::Linear);
	EXPECT_EQ(entry.pushes[0].coefficient, -0.5);
	EXPECT_NEAR(entry.pushes[0].input, 1.6 / 3.0, kTight);
	EXPECT_NEAR(entry.pushes[0].output, -0.5 * (1.6 / 3.0), kTight);
	EXPECT_NEAR(entry.raw, 0.88 / 3.0 - 0.5 * (1.6 / 3.0), kTight);
	EXPECT_EQ(entry.clamped, valueOf(engine, result, "Viscosity"));
	EXPECT_EQ(entry.repeat, 0);
}

TEST(EvaluateInfluence, ThePropertyIsClampedButTheTraceKeepsTheRawValue) {
	// Finding F1: clamping hides how far out the value was, so the trace records both.
	const Engine engine = prototypeWith([](PropertyRegistry& p) { addEdge(p, "Temperature", "Viscosity", -1.5); });

	const Result result = engine.evaluate(twoFireOneWater());

	const TraceEntry& entry = traceOf(engine, result, "Viscosity");
	EXPECT_NEAR(entry.raw, 0.88 / 3.0 - 1.5 * (1.6 / 3.0), kTight);   // about -0.5067
	EXPECT_LT(entry.raw, 0.0);
	EXPECT_EQ(entry.clamped, 0.0);
	EXPECT_EQ(valueOf(engine, result, "Viscosity"), 0.0);
}

TEST(EvaluateInfluence, ASourceIsReadAfterItWasClamped) {
	// Temperature pushes Viscosity far past 1 (clamped to 1), and Viscosity pushes Pressure by 0.5.
	// Pressure must see 1, not the unclamped 1.89.
	const Engine engine = prototypeWith([](PropertyRegistry& p) {
		addEdge(p, "Temperature", "Viscosity", 3.0);
		addEdge(p, "Viscosity", "Pressure", 0.5);
	});

	const Result result = engine.evaluate(twoFireOneWater());

	EXPECT_EQ(valueOf(engine, result, "Viscosity"), 1.0);
	EXPECT_NEAR(valueOf(engine, result, "Pressure"), 0.11 + 0.5 * 1.0, kTight);
	EXPECT_EQ(traceOf(engine, result, "Pressure").pushes.at(0).input, 1.0);
}

TEST(EvaluateInfluence, TheMechanismAndItsParametersAreUsed) {
	// Pressure -> Expansion, Saturation: coefficient * x / (1 + k * |x|), here with k = 2.
	const Engine engine = prototypeWith([](PropertyRegistry& p) {
		addEdge(p, "Pressure", "Expansion", 0.3, Mechanism::Saturation, 0, ResponseParams{ 2.0, 0.5 });
	});

	const Result result = engine.evaluate(twoFireOneWater());

	const double push = 0.3 * 0.11 / (1.0 + 2.0 * 0.11);
	EXPECT_NEAR(valueOf(engine, result, "Expansion"), 0.52 + push, kTight);
}

TEST(EvaluateInfluence, SeveralInfluencesAreAddedUp) {
	const Engine engine = prototypeWith([](PropertyRegistry& p) {
		addEdge(p, "Temperature", "Pressure", 0.25);
		addEdge(p, "Viscosity", "Pressure", 0.5);
	});

	const Result result = engine.evaluate(twoFireOneWater());

	EXPECT_NEAR(valueOf(engine, result, "Pressure"), 0.11 + 0.25 * (1.6 / 3.0) + 0.5 * (0.88 / 3.0), kTight);   // 0.39
	EXPECT_EQ(traceOf(engine, result, "Pressure").pushes.size(), 2u);
}

TEST(EvaluateInfluence, ThePushesInTheTraceFollowThePriorityOrder) {
	const Engine engine = prototypeWith([](PropertyRegistry& p) {
		addEdge(p, "Temperature", "Pressure", 0.25, Mechanism::Linear, 5);   // later
		addEdge(p, "Viscosity", "Pressure", 0.5, Mechanism::Linear, 1);      // first
	});

	const Result result = engine.evaluate(twoFireOneWater());

	const TraceEntry& entry = traceOf(engine, result, "Pressure");
	ASSERT_EQ(entry.pushes.size(), 2u);
	EXPECT_EQ(entry.pushes[0].source, engine.properties().idOf("Viscosity"));
	EXPECT_EQ(entry.pushes[1].source, engine.properties().idOf("Temperature"));
}

TEST(EvaluateInfluence, TheTraceFollowsTheEvaluationOrder) {
	// Expansion pushes Temperature, so Expansion has to be worked out first. There is no loop here.
	const Engine engine = prototypeWith([](PropertyRegistry& p) { addEdge(p, "Expansion", "Temperature", 0.1); });

	const Result result = engine.evaluate(twoFireOneWater());

	// Expansion (4) has to be worked out before Temperature (1).
	ASSERT_EQ(result.trace.size(), 4u);
	std::vector<PropertyId> seen;
	for (const TraceEntry& entry : result.trace) seen.push_back(entry.property);
	std::vector<PropertyId> expected;
	for (const EvalStep& step : engine.order()) expected.push_back(step.properties.front());
	EXPECT_EQ(seen, expected);
	EXPECT_EQ(expected.front(), 2u);   // Viscosity: nobody waits for it and it has the lowest free id
}
