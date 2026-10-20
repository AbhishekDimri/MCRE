#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "modelHelpers.hpp"

using namespace mcre;
using namespace testmodel;

// Loops: finding them, putting them in order, settling them (reference tests T-05 to T-07), and the
// stability gate that refuses a loop that would run out of control (design document 6).

namespace {
	struct Expected {
		const char* name;
		double value;
	};

	// The eight demo properties of 'result' against numbers from the Python reference model.
	void expectDemoValues(const Engine& engine, const Result& result, std::initializer_list<Expected> expected) {
		for (const Expected& e : expected) {
			EXPECT_NEAR(valueOf(engine, result, e.name), e.value, 1e-10) << e.name;
		}
	}

	// The steps of the order as lists of property ids, with a marker for loops.
	std::vector<std::pair<std::vector<PropertyId>, bool>> stepsOf(const Engine& engine) {
		std::vector<std::pair<std::vector<PropertyId>, bool>> steps;
		for (const EvalStep& step : engine.order()) steps.push_back({ step.properties, step.isLoop });
		return steps;
	}

	using Steps = std::vector<std::pair<std::vector<PropertyId>, bool>>;
}

// ---------- finding loops and ordering them ----------

TEST(Loops, AMutualInfluenceIsOneLoopStep) {
	Registries r = prototype();   // Temperature 1, Viscosity 2, Pressure 3, Expansion 4
	addEdge(r.properties, "Pressure", "Expansion", 0.3);
	addEdge(r.properties, "Expansion", "Pressure", 0.3);
	const Engine engine = build(std::move(r));

	EXPECT_EQ(stepsOf(engine), (Steps{ { { 1 }, false }, { { 2 }, false }, { { 3, 4 }, true } }));
}

TEST(Loops, TheLoopComesAfterWhatPushesItAndBeforeWhatItPushes) {
	Registries r = prototype();
	addEdge(r.properties, "Temperature", "Pressure", 0.25);   // feeds the loop
	addEdge(r.properties, "Pressure", "Expansion", 0.3);
	addEdge(r.properties, "Expansion", "Pressure", 0.3);
	addEdge(r.properties, "Expansion", "Viscosity", 0.1);     // fed by the loop
	const Engine engine = build(std::move(r));

	EXPECT_EQ(stepsOf(engine), (Steps{ { { 1 }, false }, { { 3, 4 }, true }, { { 2 }, false } }));
}

TEST(Loops, ARingOfThreeIsOneLoopStep) {
	const Engine engine = build(ring(3, 0.5));

	EXPECT_EQ(stepsOf(engine), (Steps{ { { 1, 2, 3 }, true } }));
}

TEST(Loops, TwoSeparateLoopsAreTwoSteps) {
	Registries r = prototype();
	addEdge(r.properties, "Temperature", "Viscosity", 0.2);
	addEdge(r.properties, "Viscosity", "Temperature", 0.2);
	addEdge(r.properties, "Pressure", "Expansion", 0.2);
	addEdge(r.properties, "Expansion", "Pressure", 0.2);
	const Engine engine = build(std::move(r));

	EXPECT_EQ(stepsOf(engine), (Steps{ { { 1, 2 }, true }, { { 3, 4 }, true } }));
}

TEST(Loops, AMemberOfALoopStillListsThePushesFromOutsideTheLoop) {
	Registries r = prototype();
	addEdge(r.properties, "Temperature", "Pressure", 0.25);
	addEdge(r.properties, "Pressure", "Expansion", 0.3);
	addEdge(r.properties, "Expansion", "Pressure", 0.3);
	const Engine engine = build(std::move(r));

	const std::vector<Incoming>& pushes = engine.incoming(3);
	ASSERT_EQ(pushes.size(), 2u);
	EXPECT_EQ(pushes[0].source, 1u);   // Temperature, from outside
	EXPECT_EQ(pushes[1].source, 4u);   // Expansion, from inside
}

TEST(Loops, ALongChainIsOrderedWithoutRunningOutOfStack) {
	// Finding the loops walks the whole chain. It does so with its own list, not with recursion, so
	// a very long chain cannot overflow the call stack.
	constexpr std::size_t kLength = 20000;
	Registries r;
	for (std::size_t i = 0; i < kLength; ++i) r.properties.add("P" + std::to_string(i), { -1.0, 1.0 });
	for (std::size_t i = 0; i + 1 < kLength; ++i) {
		InfluenceEdge edge;
		edge.target = static_cast<PropertyId>(i + 2);
		edge.coefficient = 0.5;
		edge.reason = "test";
		r.properties.addInfluence(static_cast<PropertyId>(i + 1), edge);
	}
	addComponent(r, "X", { { "P0", 0.5 } });

	const Engine engine = build(std::move(r));

	ASSERT_EQ(engine.order().size(), kLength);
	EXPECT_FALSE(engine.order().front().isLoop);
	EXPECT_EQ(engine.order().front().properties.front(), 1u);
	EXPECT_EQ(engine.order().back().properties.front(), static_cast<PropertyId>(kLength));
}

// ---------- settling a loop ----------

TEST(LoopSolver, ASimpleLoopSettlesAtItsFixedPoint) {
	// B wants 0.3 * A and A wants 0.05 + 0.3 * B, so A = 0.05 / (1 - 0.09).
	const Engine engine = build(pair(0.3, 0.3, 0.05));

	const Result result = engine.evaluate(spellOf({ { 1, 1.0 } }));

	const double a = 0.05 / (1.0 - 0.09);
	EXPECT_NEAR(valueOf(engine, result, "A"), a, 1e-10);
	EXPECT_NEAR(valueOf(engine, result, "B"), 0.3 * a, 1e-10);
	EXPECT_TRUE(result.converged);
	EXPECT_TRUE(result.warnings.empty());
	EXPECT_EQ(traceOf(engine, result, "A").repeat, 35);   // the number of repeats in the reference
}

TEST(LoopSolver, AMemberCanEndUpClampedAtItsLimit) {
	// A wants 0.9 + 0.7071 * B, which is more than 1 once B has grown: A stops at 1 and B at 0.7071.
	const Engine engine = build(pair(0.7071067811865476, 0.7071067811865476, 0.9));

	const Result result = engine.evaluate(spellOf({ { 1, 1.0 } }));

	EXPECT_NEAR(valueOf(engine, result, "A"), 1.0, 1e-10);
	EXPECT_NEAR(valueOf(engine, result, "B"), 0.7071067811865476, 1e-10);
	EXPECT_TRUE(result.converged);

	// A member moves towards what it wants *after* that was forced into its range (1, not 1.4), which is
	// why this takes 26 repeats in the reference; moving towards 1.4 and clamping afterwards takes 24.
	EXPECT_EQ(traceOf(engine, result, "A").repeat, 26);
}

TEST(LoopSolver, TheTraceHasOneEntryPerMemberWithTheNumberOfRepeats) {
	const Engine engine = build(pair(0.3, 0.3, 0.05));

	const Result result = engine.evaluate(spellOf({ { 1, 1.0 } }));

	ASSERT_EQ(result.trace.size(), 2u);
	const TraceEntry& a = traceOf(engine, result, "A");
	const TraceEntry& b = traceOf(engine, result, "B");
	EXPECT_GT(a.repeat, 1);
	EXPECT_LT(a.repeat, 100);
	EXPECT_EQ(a.repeat, b.repeat);
	EXPECT_NEAR(a.base, 0.05, 1e-15);
	EXPECT_EQ(b.base, 0.0);
}

TEST(LoopSolver, TheTraceExplainsTheFinalValues) {
	const Engine engine = build(pair(0.3, 0.3, 0.05));

	const Result result = engine.evaluate(spellOf({ { 1, 1.0 } }));

	const TraceEntry& a = traceOf(engine, result, "A");
	const TraceEntry& b = traceOf(engine, result, "B");
	EXPECT_EQ(a.clamped, valueOf(engine, result, "A"));   // the trace agrees with the result
	EXPECT_EQ(b.clamped, valueOf(engine, result, "B"));

	// At the settled values, base plus pushes is the value (that is what settled means).
	EXPECT_NEAR(a.raw, a.clamped, 1e-9);
	EXPECT_NEAR(b.raw, b.clamped, 1e-9);
	ASSERT_EQ(a.pushes.size(), 1u);
	EXPECT_EQ(a.pushes[0].source, engine.properties().idOf("B"));
	EXPECT_EQ(a.pushes[0].input, b.clamped);
	EXPECT_NEAR(a.raw, a.base + a.pushes[0].output, 1e-15);
}

TEST(LoopSolver, TheAnswerDoesNotDependOnWhichMemberIsListedFirst) {
	// Every member is worked out from the previous repeat's values, so listing B before A changes nothing,
	// not even the number of repeats.
	Registries ab;
	ab.properties.add("A", { -1.0, 1.0 });
	ab.properties.add("B", { -1.0, 1.0 });
	Registries ba;
	ba.properties.add("B", { -1.0, 1.0 });
	ba.properties.add("A", { -1.0, 1.0 });
	for (Registries* r : { &ab, &ba }) {
		addEdge(r->properties, "A", "B", 0.5);
		addEdge(r->properties, "B", "A", 0.3);
		addComponent(*r, "X", { { "A", 0.2 }, { "B", 0.1 } });
	}
	const Engine first = build(std::move(ab));
	const Engine second = build(std::move(ba));

	const Result r1 = first.evaluate(spellOf({ { 1, 1.0 } }));
	const Result r2 = second.evaluate(spellOf({ { 1, 1.0 } }));

	EXPECT_EQ(valueOf(first, r1, "A"), valueOf(second, r2, "A"));
	EXPECT_EQ(valueOf(first, r1, "B"), valueOf(second, r2, "B"));
	EXPECT_EQ(traceOf(first, r1, "A").repeat, traceOf(second, r2, "A").repeat);
}

TEST(LoopSolver, ALoopThatDoesNotSettleReturnsTheLastValuesWithAWarning) {
	// This loop is stable (growth rate about 0.99) but so slow that 1000 repeats are not enough.
	const BuildResult built = tryBuild(pair(0.985, 0.985, 0.01));
	ASSERT_TRUE(built.ok());
	ASSERT_EQ(built.warnings.size(), 1u);   // the gate saw that it is close to the limit
	const Engine& engine = *built.engine;

	const Result result = engine.evaluate(spellOf({ { 1, 1.0 } }));

	EXPECT_FALSE(result.converged);
	ASSERT_EQ(result.warnings.size(), 1u);
	EXPECT_EQ(result.warnings[0].where, "loop A, B");
	EXPECT_NE(result.warnings[0].message.find("1000"), std::string::npos);
	EXPECT_EQ(result.state.presentCount(), 2u);   // there is still a value for every property
	EXPECT_EQ(traceOf(engine, result, "A").repeat, 1000);

	// The values are the last ones, close to the fixed point 0.01 / (1 - 0.985^2) but not equal to it.
	const double fixedPoint = 0.01 / (1.0 - 0.985 * 0.985);
	EXPECT_NEAR(valueOf(engine, result, "A"), fixedPoint, 1e-3);
	EXPECT_NE(valueOf(engine, result, "A"), fixedPoint);
}

TEST(LoopSolver, ASettledLoopHasNoWarningAndNothingElseIsAffected) {
	const Engine engine = build(demo());

	const Result result = engine.evaluate(twoFireOneWater());

	EXPECT_TRUE(result.converged);
	EXPECT_TRUE(result.warnings.empty());
	EXPECT_EQ(traceOf(engine, result, "Temperature").repeat, 0);
	EXPECT_EQ(traceOf(engine, result, "Density").repeat, 0);
	EXPECT_GT(traceOf(engine, result, "Pressure").repeat, 0);
}

// ---------- the demo model against the reference (T-05) ----------

TEST(DemoModel, HasTheDocumentedShape) {
	const Registries r = demo();

	ASSERT_EQ(r.properties.size(), 8u);
	const std::vector<std::string> names = { "Temperature", "Density", "Viscosity", "Pressure",
		"Volatility", "Expansion", "Compression", "PhaseAffinity" };
	for (std::size_t i = 0; i < names.size(); ++i) {
		EXPECT_EQ(r.properties.idOf(names[i]), static_cast<PropertyId>(i + 1)) << names[i];
	}

	std::size_t influences = 0;
	for (const property& p : r.properties.all()) influences += p.influences.size();
	EXPECT_EQ(influences, 13u);

	// Ranges and how each property is mixed: Pressure is the only summed one; Temperature, Expansion and
	// PhaseAffinity are signed.
	for (const property& p : r.properties.all()) {
		const bool signedRange = p.propertyName == "Temperature" || p.propertyName == "Expansion"
			|| p.propertyName == "PhaseAffinity";
		EXPECT_EQ(p.bounds.lower, signedRange ? -1.0 : 0.0) << p.propertyName;
		EXPECT_EQ(p.bounds.upper, 1.0) << p.propertyName;
		EXPECT_EQ(p.aggregationKind, p.propertyName == "Pressure" ? AggregationKind::Sum : AggregationKind::WeightedMean)
			<< p.propertyName;
	}

	ASSERT_EQ(r.components.size(), 4u);
	EXPECT_EQ(r.components.idOf("Fire"), 1u);
	EXPECT_EQ(r.components.idOf("Water"), 2u);
	EXPECT_EQ(r.components.idOf("Earth"), 3u);
	EXPECT_EQ(r.components.idOf("Wind"), 4u);
}

TEST(DemoModel, BuildsWithPressureAndExpansionAsTheOnlyLoop) {
	const BuildResult built = tryBuild(demo());

	ASSERT_TRUE(built.ok());
	EXPECT_TRUE(built.warnings.empty());

	std::size_t loops = 0;
	for (const EvalStep& step : built.engine->order()) {
		if (!step.isLoop) continue;
		++loops;
		EXPECT_EQ(step.properties, (std::vector<PropertyId>{ 4, 6 }));   // Pressure, Expansion
	}
	EXPECT_EQ(loops, 1u);
}

TEST(DemoModel, TwoFireAndOneWaterMatchTheReference) {
	const Engine engine = build(demo());

	const Result result = engine.evaluate(twoFireOneWater());

	expectDemoValues(engine, result, {
		{ "Temperature", 0.5333333333333333 }, { "Density", 0.17261446903065278 },
		{ "Viscosity", 0.026666666666666672 }, { "Pressure", 0.7023022652577184 },
		{ "Volatility", 0.7466666666666667 }, { "Expansion", 0.8656765027937234 },
		{ "Compression", 0.26666666666666666 }, { "PhaseAffinity", 0.8828754572107221 } });
	EXPECT_EQ(traceOf(engine, result, "Pressure").repeat, 33);
	EXPECT_EQ(traceOf(engine, result, "Expansion").repeat, 33);
}

TEST(DemoModel, AmplifyingTemperatureMatchesTheReference) {
	const Engine engine = build(demo());
	Spell spell = twoFireOneWater();
	spell.amplify = { { 1, 1.2 } };

	const Result result = engine.evaluate(spell);

	expectDemoValues(engine, result, {
		{ "Temperature", 0.64 }, { "Density", 0.1433333333333333 },
		{ "Viscosity", 0.0 }, { "Pressure", 0.7423094122978291 },
		{ "Volatility", 0.7733333333333334 }, { "Expansion", 0.897148072849148 },
		{ "Compression", 0.26666666666666666 }, { "PhaseAffinity", 1.0 } });
	EXPECT_EQ(traceOf(engine, result, "Pressure").repeat, 32);
}

TEST(DemoModel, ConcentratingTheSpellMatchesTheReference) {
	const Engine engine = build(demo());
	Spell spell = twoFireOneWater();
	spell.energyBudget = 1.0 / 0.8;   // concentrate(0.8)

	const Result result = engine.evaluate(spell);

	EXPECT_NEAR(traceOf(engine, result, "Pressure").base, 0.1375, 1e-12);
	expectDemoValues(engine, result, {
		{ "Temperature", 0.5333333333333333 }, { "Density", 0.17261446903065278 },
		{ "Viscosity", 0.026666666666666672 }, { "Pressure", 0.7306688250425883 },
		{ "Volatility", 0.7466666666666667 }, { "Expansion", 0.8685650354108747 },
		{ "Compression", 0.26666666666666666 }, { "PhaseAffinity", 0.8828754572107221 } });
	EXPECT_EQ(traceOf(engine, result, "Pressure").repeat, 32);
}

TEST(DemoModel, FireEarthAndWindMatchTheReference) {
	const Engine engine = build(demo());

	const Result result = engine.evaluate(spellOf({ { 1, 1.0 }, { 3, 1.0 }, { 4, 1.0 } }));

	expectDemoValues(engine, result, {
		{ "Temperature", 0.3333333333333333 }, { "Density", 0.29450296760119604 },
		{ "Viscosity", 0.1566666666666667 }, { "Pressure", 0.5605866628318676 },
		{ "Volatility", 0.57 }, { "Expansion", 0.5831622267929352 },
		{ "Compression", 0.3833333333333333 }, { "PhaseAffinity", 0.3953214629285491 } });
	EXPECT_EQ(traceOf(engine, result, "Pressure").repeat, 33);
}

TEST(DemoModel, EarthAndWindWithAmplifyAndABiggerBudgetMatchTheReference) {
	// Expansion ends up negative here, so this also covers a loop member below zero.
	const Engine engine = build(demo());
	Spell spell = spellOf({ { 3, 2.0 }, { 4, 1.0 } });
	spell.amplify = { { 7, 1.5 } };   // Compression * 1.5
	spell.energyBudget = 2.0;         // concentrate(0.5)

	const Result result = engine.evaluate(spell);

	expectDemoValues(engine, result, {
		{ "Temperature", 0.05000000000000001 }, { "Density", 0.7150186252720777 },
		{ "Viscosity", 0.5816666666666667 }, { "Pressure", 0.5474182321751309 },
		{ "Volatility", 0.21583333333333338 }, { "Expansion", -0.023927877020630126 },
		{ "Compression", 0.9750000000000001 }, { "PhaseAffinity", -0.26007450108831076 } });
	EXPECT_EQ(traceOf(engine, result, "Pressure").repeat, 29);
}

TEST(DemoModel, ThreeEarthMatchTheReferenceIncludingAClampedDensity) {
	// PhaseAffinity is about -0.89, which pushes Density to 1.17, and Density is clamped to 1.
	const Engine engine = build(demo());

	const Result result = engine.evaluate(spellOf({ { 3, 3.0 } }));

	expectDemoValues(engine, result, {
		{ "Temperature", 0.10000000000000002 }, { "Density", 1.0 },
		{ "Viscosity", 0.85 }, { "Pressure", 0.3758599938858097 },
		{ "Volatility", 0.05500000000000001 }, { "Expansion", -0.41263709644308194 },
		{ "Compression", 0.9 }, { "PhaseAffinity", -0.889208274022745 } });
	const TraceEntry& density = traceOf(engine, result, "Density");
	EXPECT_GT(density.raw, 1.0);
	EXPECT_EQ(density.clamped, 1.0);
}

// ---------- the stability gate (T-07) ----------

TEST(StabilityGate, AStrongLoopIsRefused) {
	// Growth rate 0.3 + 0.7 * sqrt(3 * 3) = 2.4.
	const BuildResult built = tryBuild(pair(3.0, 3.0));

	EXPECT_FALSE(built.ok());
	EXPECT_FALSE(built.engine.has_value());
	ASSERT_EQ(built.problems.size(), 1u);
	EXPECT_EQ(built.problems[0].where, "loop A, B");
	EXPECT_NE(built.problems[0].message.find("1 or more"), std::string::npos);
}

TEST(StabilityGate, ALoopNearTheLimitIsAcceptedWithAWarning) {
	// Growth rate 0.3 + 0.7 * sqrt(0.9 * 1.0) = 0.964.
	const BuildResult built = tryBuild(pair(0.9, 1.0));

	EXPECT_TRUE(built.ok());
	EXPECT_TRUE(built.problems.empty());
	ASSERT_EQ(built.warnings.size(), 1u);
	EXPECT_EQ(built.warnings[0].where, "loop A, B");
}

TEST(StabilityGate, AWeakLoopPassesWithoutAWarning) {
	// Growth rate 0.3 + 0.7 * 0.3 = 0.51.
	const BuildResult built = tryBuild(pair(0.3, 0.3));

	EXPECT_TRUE(built.ok());
	EXPECT_TRUE(built.warnings.empty());
}

TEST(StabilityGate, ALoopThatFlipsSignIsJudgedByHowFastItGrows) {
	// One push is positive and the other negative: the error turns round and round. The growth rate is
	// sqrt(0.3^2 + (0.7 * 0.9)^2) = 0.70 for 0.9 and 1.16 for 1.6.
	EXPECT_TRUE(tryBuild(pair(0.9, -0.9)).ok());
	EXPECT_TRUE(tryBuild(pair(0.9, -0.9)).warnings.empty());
	EXPECT_FALSE(tryBuild(pair(1.6, -1.6)).ok());
}

TEST(StabilityGate, ATwoPropertyLoopIsRefusedWhicheverWayTheErrorTurns) {
	// Growth rate 0.3 + 0.7 * 1.01 = 1.007, only just over the limit. With positive pushes a small error
	// grows with both properties moving the same way, with negative pushes with the two moving in opposite
	// ways; the gate must see both.
	EXPECT_FALSE(tryBuild(pair(1.01, 1.01)).ok());
	EXPECT_FALSE(tryBuild(pair(-1.01, -1.01)).ok());
}

TEST(StabilityGate, ALoopWithOneStrongAndOneWeakPushIsJudgedByItsRealGrowthRate) {
	// The two pushes are very different (0.05 and 19.15) but together they give 0.3 + 0.7 * sqrt(0.9575)
	// = 0.985, so the loop is accepted with a warning. The rate has to be measured after the first few
	// repeats: the early repeats of such a loop look worse than it really is.
	const BuildResult built = tryBuild(pair(0.05, 19.15));

	EXPECT_TRUE(built.ok());
	EXPECT_EQ(built.warnings.size(), 1u);
}

TEST(StabilityGate, ARingOfThreeIsJudgedLikeAnyOtherLoop) {
	EXPECT_TRUE(tryBuild(ring(3, 0.5)).ok());              // 0.3 + 0.7 * 0.5 = 0.65
	EXPECT_TRUE(tryBuild(ring(3, 0.5)).warnings.empty());
	EXPECT_FALSE(tryBuild(ring(3, 1.5)).ok());             // 0.3 + 0.7 * 1.5 = 1.35
	EXPECT_TRUE(tryBuild(ring(4, 0.9)).ok());              // 0.3 + 0.7 * 0.9 = 0.93
	EXPECT_EQ(tryBuild(ring(4, 0.9)).warnings.size(), 1u);
	EXPECT_FALSE(tryBuild(ring(4, 1.2)).ok());             // 0.3 + 0.7 * 1.2 = 1.14
}

TEST(StabilityGate, ARingWithAnOddNumberOfNegativeInfluencesIsCaught) {
	// Five negative pushes in a ring: the growth comes from a turning pair of directions, not from the
	// all-ones pattern, so this needs the real growth rate and not just one test direction.
	EXPECT_FALSE(tryBuild(ring(5, -1.5)).ok());
}

TEST(StabilityGate, ACurvedLoopIsJudgedWhereItIsSteepest) {
	// A pushes B by 0.5 * (exp(2x) - 1), which is flat near 0 and steep near 1. At x = 0 this loop looks
	// harmless, at the top of the range it is not: the gate samples the whole range.
	for (const double coefficient : { 0.5, 0.1 }) {
		Registries r;
		r.properties.add("A", { -1.0, 1.0 });
		r.properties.add("B", { -1.0, 1.0 });
		addEdge(r.properties, "A", "B", coefficient, Mechanism::Exponential, 0, ResponseParams{ 2.0, 0.5 });
		addEdge(r.properties, "B", "A", 0.5);
		addComponent(r, "X", { { "A", 0.05 }, { "B", 0.0 } });

		const BuildResult built = tryBuild(std::move(r));

		if (coefficient == 0.5) EXPECT_FALSE(built.ok());   // 0.3 + 0.7 * sqrt(0.5 * exp(2)) = 1.64 at x = 1
		else EXPECT_TRUE(built.ok());                       // 0.3 + 0.7 * sqrt(0.1 * 0.5 * exp(2)) = 0.73
	}
}

TEST(StabilityGate, InfluencesFromOutsideALoopDoNotCountAsGrowth) {
	// Temperature pushes Pressure hard (4.0), but only the Pressure <-> Expansion push-backs make the loop.
	Registries r = prototype();
	addEdge(r.properties, "Temperature", "Pressure", 4.0);
	addEdge(r.properties, "Pressure", "Expansion", 0.3);
	addEdge(r.properties, "Expansion", "Pressure", 0.3);

	const BuildResult built = tryBuild(std::move(r));

	EXPECT_TRUE(built.ok());
	EXPECT_TRUE(built.warnings.empty());
}
