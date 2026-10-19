#include "mcre/model/registry.hpp"

#include <cmath>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <algorithm>
#include <string>

#include "mcre/core/logger.hpp"

namespace mcre {
	namespace {
		// exp(x) overflows a double at about 709.78, so keep k * x below 700.
		constexpr double kMaxExponent = 700.0;

		// Text for an aggregation kind, used by dump() and the log lines.
		const char* toText(AggregationKind kind) {
			switch (kind) {
			case AggregationKind::Sum:          return "Sum";
			case AggregationKind::WeightedMean: return "WeightedMean";
			case AggregationKind::Median:       return "Median";
			}
			return "?";
		}

		const char* toText(Mechanism mechanism) {
			switch (mechanism) {
			case Mechanism::Linear:      return "Linear";
			case Mechanism::Inverse:     return "Inverse";
			case Mechanism::Saturation:  return "Saturation";
			case Mechanism::Exponential: return "Exponential";
			case Mechanism::Threshold:   return "Threshold";
			case Mechanism::Sigmoid:     return "Sigmoid";
			case Mechanism::Custom:      return "Custom";
			}
			return "?";
		}

		// Logs the reason, then throws. Every check in add() goes through it.
		[[noreturn]] void reject(std::string_view where, std::string_view subject, const std::string& why) {
			logger::error(where, " rejected \"", subject, "\": ", why);
			throw std::invalid_argument(std::string(where) + ": " + why);
		}
	}

	PropertyId PropertyRegistry::add(std::string_view name, Bounds bounds, AggregationKind aggregation) {
		if (name.empty()) reject("PropertyRegistry::add", name, "the name must not be empty");
		if(byName_.find(name) != byName_.end()) reject("PropertyRegistry::add", name, "a property with this name already exists");
		if (!std::isfinite(bounds.lower) || !std::isfinite(bounds.upper) || !bounds.isValid()) {
			reject("PropertyRegistry::add", name, "bounds must be finite numbers with lower <= upper");
		}

		property p;
		p.propertyId = static_cast<PropertyId>(properties_.size() + 1);
		p.propertyName = std::string(name);
		p.bounds = bounds;
		p.aggregationKind = aggregation;

		byName_.emplace(p.propertyName, p.propertyId);
		properties_.push_back(std::move(p));

		const property& added = properties_.back();
		logger::info("add id=", added.propertyId, " name=", added.propertyName,
			" bounds=[", added.bounds.lower, ", ", added.bounds.upper, "]",
			" aggregation=", toText(added.aggregationKind));
		return added.propertyId;
	}

	const property* PropertyRegistry::byId(PropertyId id) const noexcept {
		if (id == 0 || id > properties_.size()) return nullptr;
		return &properties_[id - 1];
	}

	const property* PropertyRegistry::byName(std::string_view name) const noexcept {
		const auto it = byName_.find(name);
		if (it == byName_.end()) return nullptr;
		return byId(it->second);
	}

	PropertyId PropertyRegistry::idOf(std::string_view name) const noexcept {
		const property* p = byName(name);
		return p ? p->propertyId : 0;
	}

	void PropertyRegistry::dump(std::ostream& out) const {
		const auto savedFlags = out.flags();

		out << "PropertyRegistry: " << properties_.size() << " properties\n";
		out << "  id  name                   lower   upper  aggregation   edges\n";
		for (const property& p : properties_) {
			out << std::right << std::setw(4) << p.propertyId << "  "
				<< std::left << std::setw(20) << p.propertyName
				<< std::right << std::setw(8) << p.bounds.lower << std::setw(8) << p.bounds.upper << "  "
				<< std::left << std::setw(12) << toText(p.aggregationKind)
				<< std::right << std::setw(7) << p.influences.size() << "\n";
		}

		out.flags(savedFlags);
	}

	void PropertyRegistry::addInfluence(PropertyId source, const InfluenceEdge& edge) {
		constexpr const char* where = "PropertyRegistry::addInfluence";
		const std::string label = "#" + std::to_string(source) + " -> #" + std::to_string(edge.target);
		const property* from = byId(source);

		if(from == nullptr) {
			reject(where, label, "source property does not exist");
		}

		if(byId(edge.target) == nullptr){
			reject(where, label, "target property does not exist");
		}

		if (source == edge.target) {
			reject(where, label, "a property cannot influence itself");
		}

		if (edge.reason.empty()) { 
			reject(where, label, "the reason must not be empty"); 
		}

		if (!std::isfinite(edge.coefficient) || !std::isfinite(edge.params.threshold)) {
			reject(where, label, "coefficient and threshold must be finite numbers");
		}

		if (!std::isfinite(edge.params.k) || edge.params.k < 0.0) { 
			reject(where, label, "k must be a finite number >= 0"); 
		}

		if (edge.mechanism == Mechanism::Custom) { 
			reject(where, label, "the Custom mechanism is not implemented yet"); 
		} 
		if (edge.mechanism == Mechanism::Exponential) {
			const double largest = std::max(std::abs(from->bounds.lower), std::abs(from->bounds.upper));
			if (edge.params.k * largest > kMaxExponent) {
				reject(where, label, "k is too large: exp(k * x) would overflow");
			}
		}

		for (const InfluenceEdge& existing : from -> influences) {
			if (existing.target == edge.target && existing.mechanism == edge.mechanism) {
				reject(where, label, "this source already influences this target with the same mechanism");
			}
		}

		properties_[source - 1].influences.push_back(edge);
		logger::info("addInfluence ", label, " (", from->propertyName, " -> ", byId(edge.target)->propertyName, ")",
			" mechanism=", toText(edge.mechanism), " coefficient=", edge.coefficient, " k=", edge.params.k);
	}
}
