#include "mcre/model/stateNames.hpp"

#include <stdexcept>

#include "mcre/core/logger.hpp"

namespace mcre {
	NamedValues toNamedValues(const PropertyState& state, const PropertyRegistry& properties) {
		if (state.size() != properties.size()) {
			logger::error("toNamedValues: the state has ", state.size(), " slots but the registry has ",
				properties.size(), " properties");
			throw std::invalid_argument("toNamedValues: the state does not belong to this registry");
		}

		NamedValues named;
		for (const property& p : properties.all()) {
			if (auto value = state.get(p.propertyId)) {
				named.emplace_back(p.propertyName, *value);
			}
		}
		return named;
	}

	PropertyState fromNamedValues(const NamedValues& values, const PropertyRegistry& properties) {
		PropertyState state(properties.size());

		for (const auto& [name, value] : values) {
			const PropertyId id = properties.idOf(name);
			if (id == 0) {
				logger::error("fromNamedValues: unknown property \"", name, "\"");
				throw std::invalid_argument("fromNamedValues: unknown property \"" + name + "\"");
			}
			if (state.has(id)) {
				logger::error("fromNamedValues: property \"", name, "\" is listed twice");
				throw std::invalid_argument("fromNamedValues: property \"" + name + "\" is listed twice");
			}
			state.set(id, value);   // throws for a value that is not finite
		}
		return state;
	}
}
