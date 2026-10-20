#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "mcre/model/builtin.hpp"
#include "mcre/model/componentRegistry.hpp"
#include "mcre/model/engine.hpp"
#include "mcre/model/propertyRegistry.hpp"

using namespace mcre;

namespace {
	struct Registries {
		PropertyRegistry properties;
		ComponentRegistry components;
	};

	// Temperature, Viscosity, Pressure, Expansion and the elements Fire and Water.
	Registries makeSample() {
		Registries r;
		builtin::addPrototypeProperties(r.properties);
		builtin::addPrototypeComponents(r.components, r.properties);
		return r;
	}

	Engine buildSampleEngine() {
		Registries r = makeSample();
		BuildResult built = Engine::build(std::move(r.properties), std::move(r.components));
		return std::move(*built.engine);
	}

	// 2 Fire + 1 Water
	Spell makeSpell() {
		Spell spell;
		spell.ingredients = { { 1, 2.0 }, { 2, 1.0 } };
		return spell;
	}

	// Adds "source pushes target" to a model. The numbers do not matter for the order.
	void addEdge(PropertyRegistry& properties, const char* source, const char* target,
		int priority = 0, Mechanism mechanism = Mechanism::Linear) {
		InfluenceEdge edge;
		edge.target = properties.idOf(target);
		edge.mechanism = mechanism;
		edge.coefficient = 0.5;
		edge.priority = priority;
		edge.reason = "test";
		properties.addInfluence(properties.idOf(source), edge);
	}

	Engine buildEngine(Registries r) {
		BuildResult built = Engine::build(std::move(r.properties), std::move(r.components));
		return std::move(*built.engine);
	}

	// The property ids in the order the engine will evaluate them (every step here is one property).
	std::vector<PropertyId> orderOf(const Engine& engine) {
		std::vector<PropertyId> ids;
		for (const EvalStep& step : engine.order()) {
			ids.insert(ids.end(), step.properties.begin(), step.properties.end());
		}
		return ids;
	}
}

// ---------- building ----------

TEST(Engine, BuildsTheSampleModel) {
	Registries r = makeSample();
	BuildResult built = Engine::build(std::move(r.properties), std::move(r.components));

	ASSERT_TRUE(built.ok());
	EXPECT_TRUE(built.problems.empty());
	EXPECT_EQ(built.engine->properties().size(), 4u);
	EXPECT_EQ(built.engine->components().size(), 2u);
}

TEST(Engine, AModelWithoutPropertiesIsReportedNotThrown) {
	BuildResult built = Engine::build(PropertyRegistry{}, ComponentRegistry{});

	EXPECT_FALSE(built.ok());
	EXPECT_FALSE(built.engine.has_value());
	ASSERT_EQ(built.problems.size(), 1u);
	EXPECT_EQ(built.problems[0].where, "engine");
}

// ---------- order and incoming ----------

TEST(Engine, TheOrderHasOneStepPerPropertyInIdOrder) {
	const Engine engine = buildSampleEngine();

	ASSERT_EQ(engine.order().size(), 4u);
	for (std::size_t i = 0; i < engine.order().size(); ++i) {
		ASSERT_EQ(engine.order()[i].properties.size(), 1u) << "step " << i;
		EXPECT_EQ(engine.order()[i].properties[0], static_cast<PropertyId>(i + 1)) << "step " << i;
		EXPECT_FALSE(engine.order()[i].isLoop) << "step " << i;
	}
}

TEST(Engine, NobodyPushesAnythingWhileThereAreNoInfluences) {
	const Engine engine = buildSampleEngine();

	for (PropertyId id = 1; id <= 4; ++id) {
		EXPECT_TRUE(engine.incoming(id).empty()) << "id " << id;
	}
}

TEST(Engine, IncomingForAnIdWithoutASlotIsEmptyAndSafe) {
	const Engine engine = buildSampleEngine();

	EXPECT_TRUE(engine.incoming(0).empty());
	EXPECT_TRUE(engine.incoming(5).empty());
	EXPECT_TRUE(engine.incoming(9999).empty());
}

// ---------- order and incoming with influences ----------

TEST(Engine, APropertyComesAfterEverythingThatPushesIt) {
	Registries r = makeSample();                       // Temperature 1, Viscosity 2, Pressure 3, Expansion 4
	addEdge(r.properties, "Expansion", "Temperature");
	addEdge(r.properties, "Temperature", "Viscosity");
	const Engine engine = buildEngine(std::move(r));

	// Pressure and Expansion wait for nobody, then Temperature, then Viscosity.
	EXPECT_EQ(orderOf(engine), (std::vector<PropertyId>{ 3, 4, 1, 2 }));
	for (const EvalStep& step : engine.order()) EXPECT_FALSE(step.isLoop);
}

TEST(Engine, PropertiesWithNoRelationKeepTheirIdOrder) {
	Registries r = makeSample();
	addEdge(r.properties, "Temperature", "Pressure");
	const Engine engine = buildEngine(std::move(r));

	EXPECT_EQ(orderOf(engine), (std::vector<PropertyId>{ 1, 2, 3, 4 }));
}

TEST(Engine, AChainOfInfluencesIsOrderedFromTheStart) {
	Registries r = makeSample();
	addEdge(r.properties, "Expansion", "Pressure");
	addEdge(r.properties, "Pressure", "Viscosity");
	addEdge(r.properties, "Viscosity", "Temperature");
	const Engine engine = buildEngine(std::move(r));

	EXPECT_EQ(orderOf(engine), (std::vector<PropertyId>{ 4, 3, 2, 1 }));
}

TEST(Engine, TheOrderDoesNotDependOnTheOrderTheInfluencesWereAdded) {
	Registries a = makeSample();
	addEdge(a.properties, "Temperature", "Viscosity");
	addEdge(a.properties, "Expansion", "Temperature");
	addEdge(a.properties, "Pressure", "Viscosity");

	Registries b = makeSample();
	addEdge(b.properties, "Pressure", "Viscosity");
	addEdge(b.properties, "Expansion", "Temperature");
	addEdge(b.properties, "Temperature", "Viscosity");

	EXPECT_EQ(orderOf(buildEngine(std::move(a))), orderOf(buildEngine(std::move(b))));
}

TEST(Engine, IncomingListsWhoPushesEachProperty) {
	Registries r = makeSample();
	addEdge(r.properties, "Temperature", "Pressure");
	addEdge(r.properties, "Viscosity", "Pressure");
	const Engine engine = buildEngine(std::move(r));

	const std::vector<Incoming>& pushes = engine.incoming(3);    // Pressure
	ASSERT_EQ(pushes.size(), 2u);
	EXPECT_EQ(pushes[0].source, 1u);   // Temperature
	EXPECT_EQ(pushes[0].edge, 0u);     // its first influence
	EXPECT_EQ(pushes[1].source, 2u);   // Viscosity
	EXPECT_EQ(pushes[1].edge, 0u);

	EXPECT_TRUE(engine.incoming(1).empty());   // nobody pushes Temperature
	EXPECT_TRUE(engine.incoming(2).empty());
}

TEST(Engine, IncomingIsSortedByPriorityThenBySourceId) {
	Registries r = makeSample();
	addEdge(r.properties, "Temperature", "Pressure", 5);
	addEdge(r.properties, "Viscosity", "Pressure", 1);
	addEdge(r.properties, "Expansion", "Pressure", 1);
	const Engine engine = buildEngine(std::move(r));

	const std::vector<Incoming>& pushes = engine.incoming(3);
	ASSERT_EQ(pushes.size(), 3u);
	EXPECT_EQ(pushes[0].source, 2u);   // priority 1, lower id
	EXPECT_EQ(pushes[1].source, 4u);   // priority 1, higher id
	EXPECT_EQ(pushes[2].source, 1u);   // priority 5 comes last
}

TEST(Engine, TwoInfluencesFromOneSourceToOneTargetAreBothListed) {
	Registries r = makeSample();
	addEdge(r.properties, "Temperature", "Pressure", 0, Mechanism::Linear);
	addEdge(r.properties, "Temperature", "Pressure", 0, Mechanism::Saturation);
	const Engine engine = buildEngine(std::move(r));

	const std::vector<Incoming>& pushes = engine.incoming(3);
	ASSERT_EQ(pushes.size(), 2u);
	EXPECT_EQ(pushes[0].edge, 0u);
	EXPECT_EQ(pushes[1].edge, 1u);
	EXPECT_EQ(orderOf(engine), (std::vector<PropertyId>{ 1, 2, 3, 4 }));   // the second edge must not block Pressure
}

TEST(Engine, ADuplicateInfluenceDoesNotLetAPropertyGoEarly) {
	Registries r = makeSample();
	addEdge(r.properties, "Temperature", "Pressure", 0, Mechanism::Linear);
	addEdge(r.properties, "Temperature", "Pressure", 0, Mechanism::Saturation);
	addEdge(r.properties, "Expansion", "Pressure");
	const Engine engine = buildEngine(std::move(r));

	// Pressure (3) must wait for Expansion (4), even though Temperature has already pushed it twice.
	EXPECT_EQ(orderOf(engine), (std::vector<PropertyId>{ 1, 2, 4, 3 }));
}

TEST(Engine, IncomingKeepsTheSourceOrderForEqualPrioritiesEvenWithManyInfluences) {
	// Many equal entries, because a plain std::sort of equal entries can shuffle them once the list is long.
	Registries r;
	const int count = 40;
	for (int i = 1; i <= count; ++i) r.properties.add("P" + std::to_string(i), { 0.0, 1.0 });
	r.properties.add("Target", { 0.0, 1.0 });
	for (int i = count; i >= 1; --i) addEdge(r.properties, ("P" + std::to_string(i)).c_str(), "Target");   // added backwards
	const Engine engine = buildEngine(std::move(r));

	const std::vector<Incoming>& pushes = engine.incoming(count + 1);
	ASSERT_EQ(pushes.size(), static_cast<std::size_t>(count));
	for (std::size_t i = 0; i < pushes.size(); ++i) {
		EXPECT_EQ(pushes[i].source, static_cast<PropertyId>(i + 1)) << "place " << i;
	}
}

TEST(Engine, IncomingKeepsTheEdgeOrderOfEachSourceWithManyInfluences) {
	// The registry allows one influence per mechanism between the same two properties, so every source
	// gets two (Linear first, then Saturation).
	Registries r;
	const int count = 30;
	for (int i = 1; i <= count; ++i) r.properties.add("P" + std::to_string(i), { 0.0, 1.0 });
	r.properties.add("Target", { 0.0, 1.0 });
	for (int i = 1; i <= count; ++i) {
		addEdge(r.properties, ("P" + std::to_string(i)).c_str(), "Target", 0, Mechanism::Linear);
		addEdge(r.properties, ("P" + std::to_string(i)).c_str(), "Target", 0, Mechanism::Saturation);
	}
	const Engine engine = buildEngine(std::move(r));

	const std::vector<Incoming>& pushes = engine.incoming(count + 1);
	ASSERT_EQ(pushes.size(), static_cast<std::size_t>(2 * count));
	for (std::size_t i = 0; i < pushes.size(); ++i) {
		EXPECT_EQ(pushes[i].source, static_cast<PropertyId>(i / 2 + 1)) << "place " << i;
		EXPECT_EQ(pushes[i].edge, i % 2) << "place " << i;
	}
}

// ---------- evaluating ----------

TEST(Engine, EvaluateRejectsABadSpellBeforeDoingAnything) {
	const Engine engine = buildSampleEngine();

	EXPECT_THROW(engine.evaluate(Spell{}), std::invalid_argument);
}

// ---------- Result ----------

TEST(Result, StartsEmptyAndConverged) {
	const Result result(4);

	EXPECT_EQ(result.state.size(), 4u);
	EXPECT_EQ(result.state.presentCount(), 0u);
	EXPECT_TRUE(result.trace.empty());
	EXPECT_TRUE(result.warnings.empty());
	EXPECT_TRUE(result.converged);
}
