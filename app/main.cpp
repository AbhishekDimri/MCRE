#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "mcre/model/registry.hpp"

namespace {
	mcre::InfluenceEdge makeEdge(mcre::PropertyId target, mcre::Mechanism mechanism, double coefficient, std::string reason) {
		mcre::InfluenceEdge e;
		e.target = target;
		e.mechanism = mechanism;
		e.coefficient = coefficient;
		e.reason = std::move(reason);
		return e;
	}
}

int main() {
	using namespace mcre;

	PropertyRegistry reg;
	const PropertyId temperature = reg.add("Temperature", { 0.0, 1.0 });
	const PropertyId viscosity = reg.add("Viscosity", { 0.0, 1.0 });
	const PropertyId pressure = reg.add("Pressure", { 0.0, 1.0 });
	const PropertyId expansion = reg.add("Expansion", { 0.0, 1.0 });

	reg.addInfluence(temperature, makeEdge(viscosity, Mechanism::Inverse, -0.8, "heat thins liquids"));
	reg.addInfluence(temperature, makeEdge(pressure, Mechanism::Linear, 0.6, "heat raises pressure"));
	reg.addInfluence(pressure, makeEdge(expansion, Mechanism::Linear, 0.5, "pressure pushes things outward"));
	reg.addInfluence(expansion, makeEdge(pressure, Mechanism::Linear, 0.3, "expansion adds back pressure"));

	reg.dump(std::cout);

	try {
		reg.addInfluence(temperature, makeEdge(temperature, Mechanism::Linear, 1.0, "self loop"));
	}
	catch (const std::invalid_argument& e) {
		std::cout << "rejected as expected: " << e.what() << "\n";
	}

	std::cin.get();

	return 0;
}