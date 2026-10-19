#include "mcre/model/componentRegistry.hpp"

#include <cmath>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

#include "mcre/core/logger.hpp"
#include "mcre/model/propertyRegistry.hpp"

namespace mcre {
	namespace {
		// Logs the reason, then throws. Every check in add() goes through it.
		[[noreturn]] void reject(std::string_view where, std::string_view subject, const std::string& why) {
			logger::error(where, " rejected \"", subject, "\": ", why);
			throw std::invalid_argument(std::string(where) + ": " + why);
		}
	}

	ComponentId ComponentRegistry::add(component c, const PropertyRegistry& properties) {
		constexpr std::string_view where = "ComponentRegistry::add";

		if (c.componentName.empty()) reject(where, c.componentName, "the name must not be empty");
		if (byName_.find(c.componentName) != byName_.end()) {
			reject(where, c.componentName, "a component with this name already exists");
		}

		// Every property value must belong to a known property and lie inside its bounds.
		for (const auto& [id, value] : c.properties) {
			const property* p = properties.byId(id);
			if (p == nullptr) {
				reject(where, c.componentName, "property id " + std::to_string(id) + " does not exist");
			}
			if (!std::isfinite(value)) {
				reject(where, c.componentName, "value of " + p->propertyName + " must be a finite number");
			}
			if (value < p->bounds.lower || value > p->bounds.upper) {
				reject(where, c.componentName, "value " + std::to_string(value) + " of " + p->propertyName +
					" is outside [" + std::to_string(p->bounds.lower) + ", " + std::to_string(p->bounds.upper) + "]");
			}
		}

		c.componentId = static_cast<ComponentId>(components_.size() + 1);

		byName_.emplace(c.componentName, c.componentId);
		components_.push_back(std::move(c));

		const component& added = components_.back();
		logger::info("add id=", added.componentId, " name=", added.componentName,
			" version=", added.version, " properties=", added.properties.size());
		return added.componentId;
	}

	const component* ComponentRegistry::byId(ComponentId id) const noexcept {
		if (id == 0 || id > components_.size()) return nullptr;
		return &components_[id - 1];
	}

	const component* ComponentRegistry::byName(std::string_view name) const noexcept {
		const auto it = byName_.find(name);
		if (it == byName_.end()) return nullptr;
		return byId(it->second);
	}

	ComponentId ComponentRegistry::idOf(std::string_view name) const noexcept {
		const component* c = byName(name);
		return c ? c->componentId : 0;
	}

	void ComponentRegistry::dump(std::ostream& out, const PropertyRegistry& properties) const {
		out << "ComponentRegistry: " << components_.size() << " components\n";
		for (const component& c : components_) {
			out << "  #" << c.componentId << " " << c.componentName;
			if (!c.version.empty()) out << " (version " << c.version << ")";
			out << "\n";
			for (const auto& [id, value] : c.properties) {
				const property* p = properties.byId(id);
				out << "        " << (p ? p->propertyName : "?") << " = " << value << "\n";
			}
		}
	}
}
