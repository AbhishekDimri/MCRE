#pragma once

#include <algorithm>
#include <vector>

#include "mcre/model/model.hpp"

namespace mcre {

	// One component's contribution to one property.
	struct Contribution {
		ComponentId component = 0;
		double value = 0.0;   // the component's intrinsic value for that property
		double weight = 1.0;  // how strongly it takes part (amount, concentration, ...)
	};

	// Combines contributions to ONE property. Taken by value so we can sort a copy:
	// a full (component, value, weight) ordering keeps floating-point results identical
	// between runs, even when the same component id appears more than once.
	inline double aggregate(AggregationKind kind, std::vector<Contribution> items) {
		for (const auto& c : items) {
			if (!(c.weight >= 0.0)) {  // also rejects NaN
				throw std::invalid_argument("aggregate: weight must be non-negative");
			}
		}

		std::sort(items.begin(), items.end(),
			[](const Contribution& a, const Contribution& b) {
				if (a.component != b.component) return a.component < b.component;
				if (a.value != b.value) return a.value < b.value;
				return a.weight < b.weight;
			});

		switch (kind) {
		case AggregationKind::Sum: {
			double total = 0.0;
			for (const auto& c : items) total += c.value * c.weight;
			return total;
		}
		case AggregationKind::WeightedMean: {
			double weighted = 0.0;
			double weights = 0.0;
			for (const auto& c : items) {
				weighted += c.value * c.weight;
				weights += c.weight;
			}
			return weights == 0.0 ? 0.0 : weighted / weights;  // no contributors -> 0
		}
		case AggregationKind::Median: {
			// Weighted median: the value where the running weight first reaches half the total.
			// Ties in value are broken by component id, so the result is deterministic.
			std::stable_sort(items.begin(), items.end(),
				[](const Contribution& a, const Contribution& b) { return a.value < b.value; });
			double total = 0.0;
			for (const auto& c : items) total += c.weight;
			if (total == 0.0) return 0.0;  // no contributors -> 0
			double running = 0.0;
			for (const auto& c : items) {
				running += c.weight;
				if (running >= total / 2.0) return c.value;
			}
			return items.back().value;  // unreachable except for rounding
		}
		}
		throw std::invalid_argument("aggregate: unknown aggregation kind");
	}

}