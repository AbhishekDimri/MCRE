#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "mcre/model/loader.hpp"
#include "mcre/model/propertyRegistry.hpp"

using namespace mcre;

namespace {
	// True when the result holds a problem at exactly 'where' whose message contains 'messagePart'.
	// On failure it prints every problem it did find, which makes a broken test easy to read.
	::testing::AssertionResult hasProblem(const LoadResult& result, const std::string& where,
		const std::string& messagePart) {
		for (const Diagnostic& d : result.problems) {
			if (d.where == where && d.message.find(messagePart) != std::string::npos) {
				return ::testing::AssertionSuccess();
			}
		}
		::testing::AssertionResult failure = ::testing::AssertionFailure();
		failure << "no problem at '" << where << "' containing '" << messagePart << "'. Problems found:";
		for (const Diagnostic& d : result.problems) failure << "\n    " << d.where << "  ->  " << d.message;
		return failure;
	}

	// Writes 'bytes' into a file in the temp folder and returns its path.
	std::filesystem::path writeTempFile(const std::string& name, const std::string& bytes) {
		const std::filesystem::path path = std::filesystem::path(::testing::TempDir()) / name;
		std::ofstream out(path, std::ios::binary);
		out << bytes;
		return path;
	}

	// A tiny valid file, used by several tests.
	const std::string kFireJson =
		R"json({"schemaVersion": 1, "components": [{"name": "Fire", "properties": {"Temperature": 0.95}}]})json";
}

// A fixture: a fresh PropertyRegistry named 'registry' is built before every TEST_F.
// Temperature = 1, Viscosity = 2, Pressure = 3 ([-1, 1]), Expansion = 4.
class Loader : public ::testing::Test {
protected:
	void SetUp() override {
		registry.add("Temperature", { 0.0, 1.0 });
		registry.add("Viscosity", { 0.0, 1.0 });
		registry.add("Pressure", { -1.0, 1.0 });
		registry.add("Expansion", { 0.0, 1.0 });
	}

	PropertyRegistry registry;
};

// ---------- a good file ----------

TEST_F(Loader, LoadsAGoodFile) {
	const LoadResult result = loadComponentsFromText(R"json({
		"schemaVersion": 1,
		"components": [
			{ "name": "Fire", "version": "1", "properties": { "Temperature": 0.95, "Viscosity": 0.1, "Pressure": 0.4 } },
			{ "name": "Water", "properties": { "Temperature": 0.1, "Viscosity": 0.7 } }
		]})json", "good", registry);

	ASSERT_TRUE(result.ok());
	ASSERT_EQ(result.components.size(), 2u);

	const component& fire = result.components[0];
	EXPECT_EQ(fire.componentName, "Fire");
	EXPECT_EQ(fire.version, "1");
	EXPECT_EQ(fire.properties.size(), 3u);
	EXPECT_DOUBLE_EQ(fire.properties.at(registry.idOf("Temperature")), 0.95);
	EXPECT_EQ(fire.componentId, 0u);  // ids are assigned later, by the ComponentRegistry

	const component& water = result.components[1];
	EXPECT_EQ(water.componentName, "Water");
	EXPECT_TRUE(water.version.empty());  // version is optional
	EXPECT_EQ(water.properties.size(), 2u);
}

TEST_F(Loader, AMissingPropertyStaysAbsentInsteadOfBecomingZero) {
	const LoadResult result = loadComponentsFromText(
		R"json({"schemaVersion": 1, "components": [{"name": "Water", "properties": {"Temperature": 0.1}}]})json",
		"absent", registry);

	ASSERT_TRUE(result.ok());
	EXPECT_EQ(result.components[0].properties.count(registry.idOf("Pressure")), 0u);
}

TEST_F(Loader, IntegersCountAsNumbers) {
	const LoadResult result = loadComponentsFromText(
		R"json({"schemaVersion": 1, "components": [{"name": "A", "properties": {"Temperature": 1, "Viscosity": 0}}]})json",
		"int", registry);

	ASSERT_TRUE(result.ok());
	EXPECT_DOUBLE_EQ(result.components[0].properties.at(registry.idOf("Temperature")), 1.0);
}

TEST_F(Loader, LoadingDoesNotChangeTheRegistry) {
	(void)loadComponentsFromText(kFireJson, "same", registry);
	EXPECT_EQ(registry.size(), 4u);
}

// ---------- all or nothing ----------

TEST_F(Loader, OneBadComponentMeansNoComponentsAtAll) {
	const LoadResult result = loadComponentsFromText(R"json({
		"schemaVersion": 1,
		"components": [
			{ "name": "Fire", "properties": { "Temperature": 0.5 } },
			{ "name": "Bad",  "properties": { "Temperature": 9 } }
		]})json", "mixed", registry);

	EXPECT_FALSE(result.ok());
	EXPECT_TRUE(result.components.empty());  // Fire was fine, but it is thrown away too
	EXPECT_TRUE(hasProblem(result, "mixed#/components/1/properties/Temperature", "outside [0, 1]"));
}

TEST_F(Loader, ReportsManyProblemsInOneRun) {
	const LoadResult result = loadComponentsFromText(R"json({
		"schemaVersion": 1,
		"components": [
			{ "name": "A", "propertys": {},
			  "properties": { "Temperature": true, "Nope": 1, "Viscosity": "hot", "Pressure": null, "Expansion": -0.1 } },
			{ "name": "A", "version": 3, "properties": {} },
			{ "properties": {} },
			{ "name": "", "properties": [] },
			7
		]})json", "multi", registry);

	EXPECT_FALSE(result.ok());
	EXPECT_TRUE(hasProblem(result, "multi#/components/0/propertys", "unknown key"));
	EXPECT_TRUE(hasProblem(result, "multi#/components/0/properties/Temperature", "must be a number"));  // true is not a number
	EXPECT_TRUE(hasProblem(result, "multi#/components/0/properties/Nope", "unknown property"));
	EXPECT_TRUE(hasProblem(result, "multi#/components/0/properties/Viscosity", "must be a number"));
	EXPECT_TRUE(hasProblem(result, "multi#/components/0/properties/Pressure", "must be a number"));     // null
	EXPECT_TRUE(hasProblem(result, "multi#/components/0/properties/Expansion", "outside [0, 1]"));
	EXPECT_TRUE(hasProblem(result, "multi#/components/1/name", "duplicate component name 'A'"));
	EXPECT_TRUE(hasProblem(result, "multi#/components/1/version", "must be a string"));
	EXPECT_TRUE(hasProblem(result, "multi#/components/2/name", "required"));
	EXPECT_TRUE(hasProblem(result, "multi#/components/3/name", "required"));
	EXPECT_TRUE(hasProblem(result, "multi#/components/3/properties", "required"));
	EXPECT_TRUE(hasProblem(result, "multi#/components/4", "must be an object"));
}

// ---------- the top level of the file ----------

TEST_F(Loader, TheTopLevelMustBeAnObject) {
	EXPECT_TRUE(hasProblem(loadComponentsFromText("[]", "t", registry), "t", "top level must be an object"));
}

TEST_F(Loader, SchemaVersionMustBeTheIntegerOne) {
	EXPECT_TRUE(hasProblem(loadComponentsFromText("{}", "t", registry), "t#/schemaVersion", "required"));
	EXPECT_TRUE(hasProblem(loadComponentsFromText(R"({"schemaVersion": 2, "components": []})", "t", registry),
		"t#/schemaVersion", "required"));
	EXPECT_TRUE(hasProblem(loadComponentsFromText(R"({"schemaVersion": "1", "components": []})", "t", registry),
		"t#/schemaVersion", "required"));
	EXPECT_TRUE(hasProblem(loadComponentsFromText(R"({"schemaVersion": 1.0, "components": []})", "t", registry),
		"t#/schemaVersion", "required"));
}

TEST_F(Loader, ComponentsMustBeAnArray) {
	EXPECT_TRUE(hasProblem(loadComponentsFromText(R"({"schemaVersion": 1})", "t", registry),
		"t#/components", "required"));
	EXPECT_TRUE(hasProblem(loadComponentsFromText(R"({"schemaVersion": 1, "components": {}})", "t", registry),
		"t#/components", "required"));
}

TEST_F(Loader, UnknownTopLevelKeysAreReported) {
	const LoadResult result = loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [], "extra": 1})", "t", registry);
	EXPECT_TRUE(hasProblem(result, "t#/extra", "unknown key"));
}

TEST_F(Loader, TheSchemaKeyIsAllowedAndAnEmptyListIsFine) {
	const LoadResult result = loadComponentsFromText(
		R"({"$schema": "x", "schemaVersion": 1, "components": []})", "t", registry);
	EXPECT_TRUE(result.ok());
	EXPECT_TRUE(result.components.empty());
}

// ---------- bad text and bad numbers ----------

TEST_F(Loader, SyntaxErrorsAreReportedNotThrown) {
	LoadResult result;
	ASSERT_NO_THROW(result = loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [ {"name": "A",} ]})", "syntax.json", registry));

	EXPECT_FALSE(result.ok());
	ASSERT_EQ(result.problems.size(), 1u);
	EXPECT_EQ(result.problems[0].where, "syntax.json");
	EXPECT_NE(result.problems[0].message.find("not valid JSON"), std::string::npos);
}

TEST_F(Loader, NumbersTooBigForADoubleAreReported) {
	const LoadResult result = loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [{"name": "A", "properties": {"Temperature": 1e999}}]})",
		"big", registry);
	EXPECT_FALSE(result.ok());
}

TEST_F(Loader, EmptyOrBlankTextIsAnError) {
	EXPECT_FALSE(loadComponentsFromText("", "empty", registry).ok());
	EXPECT_FALSE(loadComponentsFromText("   ", "blank", registry).ok());
}

TEST_F(Loader, JsonCannotSpellNanOrInfinity) {
	EXPECT_FALSE(loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [{"name": "A", "properties": {"Temperature": NaN}}]})", "nan", registry).ok());
	EXPECT_FALSE(loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [{"name": "A", "properties": {"Temperature": Infinity}}]})", "inf", registry).ok());
}

// ---------- bounds and names ----------

TEST_F(Loader, ValuesExactlyOnTheBoundsAreAcceptedAndJustOutsideIsNot) {
	const LoadResult onTheEdge = loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [{"name": "A", "properties": {"Temperature": 0, "Viscosity": 1, "Pressure": -1}}]})",
		"edge", registry);
	EXPECT_TRUE(onTheEdge.ok());

	const LoadResult justOutside = loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [{"name": "A", "properties": {"Temperature": -0.0000001}}]})",
		"outside", registry);
	EXPECT_FALSE(justOutside.ok());
}

TEST_F(Loader, PropertyNamesAreCaseSensitive) {
	const LoadResult result = loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [{"name": "A", "properties": {"temperature": 0.5}}]})", "c", registry);
	EXPECT_TRUE(hasProblem(result, "c#/components/0/properties/temperature", "unknown property"));
}

TEST_F(Loader, WithoutAnyPropertiesEveryNameIsUnknown) {
	const PropertyRegistry nothing;
	const LoadResult result = loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [{"name": "A", "properties": {"Temperature": 0.5}}]})", "e", nothing);
	EXPECT_FALSE(result.ok());
}

TEST_F(Loader, ProblemAddressesStartWithTheSourceName) {
	const LoadResult result = loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [{"name": "A", "properties": {"Temperature": 3}}]})",
		"my file.json", registry);
	ASSERT_EQ(result.problems.size(), 1u);
	EXPECT_EQ(result.problems[0].where, "my file.json#/components/0/properties/Temperature");
}

// Known limitation, written down as a test: the JSON library keeps only the LAST of two equal keys,
// so a file that lists "Temperature" twice is NOT reported. If this is ever fixed, change this test.
TEST_F(Loader, KnownLimitationTheSameKeyTwiceKeepsTheLastValue) {
	const LoadResult result = loadComponentsFromText(
		R"({"schemaVersion": 1, "components": [{"name": "A", "properties": {"Temperature": 0.2, "Temperature": 0.9}}]})",
		"dup", registry);

	ASSERT_TRUE(result.ok());
	EXPECT_DOUBLE_EQ(result.components[0].properties.at(registry.idOf("Temperature")), 0.9);
}

// ---------- files ----------

TEST_F(Loader, ReadsAFileFromDisk) {
	const std::filesystem::path file = writeTempFile("mcre_loader_good.json", kFireJson);

	const LoadResult result = loadComponentsFromFile(file, registry);
	std::filesystem::remove(file);

	ASSERT_TRUE(result.ok());
	EXPECT_EQ(result.components[0].componentName, "Fire");
}

TEST_F(Loader, ReadsAFileThatStartsWithAByteOrderMark) {
	// Some Windows editors put these three bytes at the start of UTF-8 files.
	const std::filesystem::path file = writeTempFile("mcre_loader_bom.json", "\xEF\xBB\xBF" + kFireJson);

	const LoadResult result = loadComponentsFromFile(file, registry);
	std::filesystem::remove(file);

	EXPECT_TRUE(result.ok());
}

TEST_F(Loader, ReadsAFileWithWindowsLineEndings) {
	const std::filesystem::path file = writeTempFile("mcre_loader_crlf.json",
		"{\r\n \"schemaVersion\": 1,\r\n \"components\": [ {\"name\": \"Fire\",\r\n"
		" \"properties\": {\"Temperature\": 0.95}} ]\r\n}\r\n");

	const LoadResult result = loadComponentsFromFile(file, registry);
	std::filesystem::remove(file);

	EXPECT_TRUE(result.ok());
}

TEST_F(Loader, AMissingFileIsAProblemNotACrash) {
	const LoadResult result = loadComponentsFromFile("this_file_does_not_exist.json", registry);

	EXPECT_FALSE(result.ok());
	ASSERT_EQ(result.problems.size(), 1u);
	EXPECT_EQ(result.problems[0].message, "cannot open the file");
}

TEST_F(Loader, AnEmptyFileIsAProblem) {
	const std::filesystem::path file = writeTempFile("mcre_loader_empty.json", "");

	const LoadResult result = loadComponentsFromFile(file, registry);
	std::filesystem::remove(file);

	EXPECT_FALSE(result.ok());
}

TEST_F(Loader, AFolderInPlaceOfAFileIsAProblem) {
	EXPECT_FALSE(loadComponentsFromFile(::testing::TempDir(), registry).ok());
}
