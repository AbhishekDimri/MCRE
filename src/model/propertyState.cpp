#include "mcre/model/propertyState.hpp"
#include "mcre/core/logger.hpp"
#include "mcre/model/propertyRegistry.hpp"

#include <cmath>
#include <stdexcept>
#include <ostream>

namespace mcre {
	 PropertyState::PropertyState(std::size_t propertyCount) 
		: values_(propertyCount, std::nullopt) {}
	

	std::optional<double> PropertyState::get(PropertyId id) const noexcept {
		if (id == 0 || id > size()) 
			return std::nullopt;
		
		return values_[id - 1];
	}

	bool PropertyState::has(PropertyId id) const noexcept {
		return get(id).has_value();
	}

	double PropertyState::valueOr(PropertyId id, double fallback) const noexcept {
		return get(id).value_or(fallback);
	}

	std::size_t PropertyState::presentCount() const noexcept {
		std::size_t count = 0;

		for (const std::optional<double>& value : values_) {
			if (value != std::nullopt) {
				count++;
			}
		}

		return count;
	}

	void PropertyState::set(PropertyId id, double value) {
		if (id == 0 || id > size()) {
			logger::error("PropertyState::set: id ", id, " has no slot (state has ",
				values_.size(), " slots)");
			throw std::invalid_argument("PropertyState::set: id has no slot");
		}

		if (!std::isfinite(value)) {
			logger::error("PropertyState::set: value for id ", id, " is not finite");

			throw std::invalid_argument("PropertyState::set: value is not finite");
		}

		values_[id - 1] = value;
	}

	void PropertyState::dump(std::ostream& out, const PropertyRegistry& properties) const {
		out << "PropertyState: " << size() << " slots\n";
		for (std::size_t i = 0; i < size(); ++i) {
			const property* p = properties.byId(static_cast<PropertyId>(i + 1));
			out << "  " << (p ? p->propertyName : "?") << " = ";
			if (values_[i]) out << *values_[i];
			else out << "(none)";
			out << "\n";
		}
	}

}