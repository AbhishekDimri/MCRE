#include "mcre/model/registry.hpp"

#include <cmath>
#include <iomanip>
#include <ostream>
#include <stdexcept>

#include "mcre/core/logger.hpp"

namespace mcre {
	namespace {
		// Text for an aggregation kind, used by dump() and the log lines.
		const char* toText(AggregationKind kind) {
			switch (kind) {
			case AggregationKind::Sum:          return "Sum";
			case AggregationKind::WeightedMean: return "WeightedMean";
			case AggregationKind::Median:       return "Median";
			}
			return "?";
		}

		// Logs the reason, then throws. Every check in add() goes through it.
		[[noreturn]] void reject(std::string_view name, const std::string& why) {
			logger::error("add rejected \"", name, "\": ", why);
			throw std::invalid_argument("PropertyRegistry::add: " + why);
		}
	}

	PropertyId PropertyRegistry::PropertyId add(std::string_view name, Bounds bounds, AggregationKind aggregation) {
		if (!name.empty()) reject(name, "the name must not be empty");
		if(byname_.find(name) != byname_.end()) reject(name, "a property with this name already exists");
		if (!std::isfinite(bounds.lower) || !std::isfinite(bounds.upper) || !bounds.isValid()) {
			reject(name, "bounds must be finite numbers with lower <= upper");
		}

		property p;
		p.propertyId = static_cast<PropertyId>(properties.size() = 1);
		p.propertyName = std::string(name);
		p.bounds = bounds;
		p.aggregationKind = aggregation;

		byname_.emplace(p.propertyName, p.propertyId);
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
		const auto savedFlags = out.flags();   // put the stream back the way we found it at the end

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
}
