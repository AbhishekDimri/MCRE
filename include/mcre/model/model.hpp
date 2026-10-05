#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace mcre {
	// Numeric identifiers. The registry will assign these; files refer to names.
	using PropertyId = std::uint32_t;
	using ComponentId = std::uint32_t;

	// enum for the different aggregation methods
	enum AggregationKind {Sum, WeightedMean, Median};
	// enum for the different mechanisms
	enum Mechanism {Linear, Inverse, Saturation, Exponential, Threshold, Sigmoid, Custom};

	// struct for the bounds of the property value
	struct Bounds {
		double lower = -1.0;
		double upper = 1.0;

		inline bool isValid() const { return lower <= upper; }
	};

	// function to clamp a value to the bounds of the property value
	inline double ClampToBounds(double value, Bounds bounds) {
		if (std::isnan(value)) {
			throw std::invalid_argument("clampTo: value is NaN");
		}

		return std::max(bounds.lower, std::min(value, bounds.upper));
	}

	// Extra numbers some mechanisms need.
	//   k:         strength / rate / steepness (meaning depends on the mechanism)
	//   threshold: where Threshold switches on
	struct ResponseParams {
		double k = 1.0;
		double threshold = 0.5;
	};

	// "Source property affects target property". Stored on the SOURCE property.
	struct InfluenceEdge {
		PropertyId target = 0;
		Mechanism mechanism = Mechanism::Linear;
		double coefficient = 0.0;  // sign decides direction: + raises target, - lowers it
		ResponseParams params;
		int priority = 0;          // canonical ordering when several edges hit one target
		std::string reason;        // mandatory human justification (validated later)
	};


	// struct for the property value
	struct property {
		PropertyId propertyId;
		double propertyValue;
		std::string propertyName;
		Bounds bounds;
		std::vector<InfluenceEdge> influences; // list of influences on this property
	};

	// struct for the component value
	struct component {
		ComponentId componentId;
		std::string componentName;
		std::map< PropertyId, double> properties; // map of propertyId to propertyValue
		std::string version;
	};

};