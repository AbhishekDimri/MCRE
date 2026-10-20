#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "mcre/model/model.hpp"
#include "mcre/model/propertyState.hpp"

namespace mcre {
	// One influence acting on one property. 'output' is coefficient * shape(input), before clamping.
	struct Push {
		PropertyId source = 0;
		Mechanism mechanism = Mechanism::Linear;
		double coefficient = 0.0;
		double input = 0.0;   // the source property's value
		double output = 0.0;
	};

	// How one value was calculated. The solver writes one entry per property, in the order it worked, so
	// the trace cannot disagree with the result: 'clamped' is the value in Result::state.
	// For a property in a loop the entry describes the final values: 'pushes' are what reached it at those
	// values, 'raw' is base plus pushes (what it wanted), and 'repeat' is how many repeats the loop took.
	struct TraceEntry {
		PropertyId property = 0;
		double base = 0.0;          // the mixed value (after amplify), forced into the property's range
		std::vector<Push> pushes;   // every influence that reached this property
		double raw = 0.0;           // base plus all pushes, before any clamping
		double clamped = 0.0;       // the final value, inside the property's range
		int repeat = 0;             // 0 = not in a loop, otherwise the number of repeats of its loop
	};

	// Something the caller should know that is not an error, for example "a loop did not settle".
	struct Warning {
		std::string where;
		std::string message;
	};

	// What evaluate() returns: the finished scoreboard plus the story of how it was made.
	struct Result {
		PropertyState state;
		std::vector<TraceEntry> trace;
		std::vector<Warning> warnings;
		bool converged = true;   // false when a loop did not settle; 'state' then holds the last values

		explicit Result(std::size_t propertyCount) : state(propertyCount) {}
	};
}
