#pragma once

#include <cmath>
+ #include <stdexcept>

#include "mcre/core/errors.hpp"
#include "mcre/model/model.hpp"

namespace mcre {

	// Response curve of ONE source value. Pure: no clamping, no state.
	// The caller multiplies by the edge coefficient and clamps the final result.
	inline double responseShape(Mechanism mechanism, double x, const ResponseParams& p = {}) {
		switch (mechanism) {
		case Mechanism::Linear:
			return x;
		case Mechanism::Inverse:  // shrinks as x grows; always finite
			return 1.0 / (1.0 + p.k * std::abs(x));
		case Mechanism::Saturation:  // diminishing returns
			return x / (1.0 + p.k * std::abs(x));
		case Mechanism::Exponential:  // 0 at x = 0, grows fast
			return std::exp(p.k * x) - 1.0;
		case Mechanism::Threshold:  // smooth switch around p.threshold (use a large k, e.g. 10)
			return 1.0 / (1.0 + std::exp(-p.k * (x - p.threshold)));
		case Mechanism::Sigmoid:  // S-curve through 0, range (-1, 1)
			return 2.0 / (1.0 + std::exp(-p.k * x)) - 1.0;
		case Mechanism::Custom:
			MCRE_TODO("custom mechanism");
		}
		// Only reached if a new enum value was added and not handled above.
		throw std::invalid_argument("responseShape: unknown mechanism");
	}
}