#include "mcre/model/engine.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <string>
#include <utility>

#include "mcre/core/logger.hpp"
#include "mcre/model/aggregation.hpp"
#include "mcre/model/response.hpp"

namespace mcre {
	namespace {
		constexpr std::size_t kNone = static_cast<std::size_t>(-1);

		// First choices that worked on the sample model (design document: assumptions A-04, A-05, A-06).
		constexpr double kDamping = 0.7;        // a loop member moves 70% of the way to where it wants to be
		constexpr double kTolerance = 1e-12;    // a loop has settled when no member moves more than this
		constexpr int kMaxRepeats = 1000;       // give up after this many repeats
		constexpr double kRejectGrowth = 1.0;   // the stability gate: a loop that grows by this much is refused
		constexpr double kWarnGrowth = 0.9;     // ... and one this close to the limit gets a warning

		// ---------- who pushes whom, and in what order ----------

		// incoming[target - 1] lists every influence that pushes the target, in a fixed order:
		// lower priority number first, then lower source id, then the edge's place in its source.
		std::vector<std::vector<Incoming>> makeIncoming(const PropertyRegistry& properties) {
			std::vector<std::vector<Incoming>> incoming(properties.size());

			for (const property& source : properties.all()) {
				for (std::size_t e = 0; e < source.influences.size(); ++e) {
					incoming[source.influences[e].target - 1].push_back({ source.propertyId, e });
				}
			}

			for (std::vector<Incoming>& list : incoming) {
				std::sort(list.begin(), list.end(), [&properties](const Incoming& a, const Incoming& b) {
					const int priorityA = properties.byId(a.source)->influences[a.edge].priority;
					const int priorityB = properties.byId(b.source)->influences[b.edge].priority;
					if (priorityA != priorityB) return priorityA < priorityB;
					if (a.source != b.source) return a.source < b.source;
					return a.edge < b.edge;
				});
			}
			return incoming;
		}

		// Splits the properties into groups that push each other round in a circle (Tarjan's algorithm).
		// Every property is in exactly one group, and most groups hold a single property.
		// It walks with its own stack instead of recursion, so a long chain of properties cannot overflow
		// the call stack. Each group is sorted by id.
		std::vector<std::vector<PropertyId>> findGroups(const PropertyRegistry& properties) {
			const std::size_t count = properties.size();

			std::vector<std::size_t> number(count, kNone);   // order of discovery; kNone = not visited yet
			std::vector<std::size_t> lowest(count, 0);       // the lowest number this property can reach
			std::vector<bool> onStack(count, false);
			std::vector<std::size_t> stack;                  // properties that are not yet in a group
			std::vector<std::pair<std::size_t, std::size_t>> walk;   // (property, next influence to look at)
			std::vector<std::vector<PropertyId>> groups;
			std::size_t counter = 0;

			const auto visit = [&](std::size_t v) {
				number[v] = lowest[v] = counter++;
				stack.push_back(v);
				onStack[v] = true;
				walk.push_back({ v, 0 });
			};

			for (std::size_t first = 0; first < count; ++first) {
				if (number[first] != kNone) continue;
				visit(first);

				while (!walk.empty()) {
					const std::size_t v = walk.back().first;
					const std::vector<InfluenceEdge>& edges = properties.all()[v].influences;

					if (walk.back().second < edges.size()) {
						const std::size_t w = edges[walk.back().second++].target - 1;
						if (number[w] == kNone) visit(w);
						else if (onStack[w]) lowest[v] = std::min(lowest[v], number[w]);
						continue;
					}

					// Every influence of v has been looked at. If v can reach nothing lower than itself,
					// it is the start of a group: everything above it on the stack belongs to it.
					if (lowest[v] == number[v]) {
						std::vector<PropertyId> group;
						std::size_t w = kNone;
						do {
							w = stack.back();
							stack.pop_back();
							onStack[w] = false;
							group.push_back(static_cast<PropertyId>(w + 1));
						} while (w != v);
						std::sort(group.begin(), group.end());
						groups.push_back(std::move(group));
					}

					walk.pop_back();
					if (!walk.empty()) {
						const std::size_t parent = walk.back().first;
						lowest[parent] = std::min(lowest[parent], lowest[v]);
					}
				}
			}
			return groups;
		}

		// Every group after all the groups that push it. Among the groups that are ready, the one with the
		// lowest id goes first, so the same model always gives the same order.
		std::vector<EvalStep> makeOrder(const PropertyRegistry& properties) {
			const std::vector<std::vector<PropertyId>> groups = findGroups(properties);

			std::vector<std::size_t> groupOf(properties.size(), 0);
			for (std::size_t g = 0; g < groups.size(); ++g) {
				for (PropertyId id : groups[g]) groupOf[id - 1] = g;
			}

			// Pushes from other groups that are not yet placed. Counting edges (not sources) is right:
			// two influences from one source are released together when that source is placed.
			std::vector<std::size_t> waitingFor(groups.size(), 0);
			for (const property& p : properties.all()) {
				for (const InfluenceEdge& edge : p.influences) {
					if (groupOf[edge.target - 1] != groupOf[p.propertyId - 1]) ++waitingFor[groupOf[edge.target - 1]];
				}
			}

			const auto later = [&groups](std::size_t a, std::size_t b) { return groups[a].front() > groups[b].front(); };
			std::priority_queue<std::size_t, std::vector<std::size_t>, decltype(later)> ready(later);
			for (std::size_t g = 0; g < groups.size(); ++g) {
				if (waitingFor[g] == 0) ready.push(g);
			}

			std::vector<EvalStep> order;
			while (!ready.empty()) {
				const std::size_t g = ready.top();
				ready.pop();

				// A group is a loop when one of its influences stays inside the group. (Several properties in
				// one group always do that; a single property does it only if it pushes itself.)
				EvalStep step{ groups[g], false };
				for (PropertyId id : groups[g]) {
					for (const InfluenceEdge& edge : properties.byId(id)->influences) {
						const std::size_t targetGroup = groupOf[edge.target - 1];
						if (targetGroup == g) step.isLoop = true;
						else if (--waitingFor[targetGroup] == 0) ready.push(targetGroup);
					}
				}
				order.push_back(std::move(step));
			}
			return order;
		}

		// "Pressure, Expansion" for the messages.
		std::string namesOf(const PropertyRegistry& properties, const std::vector<PropertyId>& ids) {
			std::string text;
			for (PropertyId id : ids) {
				if (!text.empty()) text += ", ";
				text += properties.byId(id)->propertyName;
			}
			return text;
		}

		// ---------- the stability gate (design document 6.4) ----------

		// Number n of the Halton sequence for a prime base: a number between 0 and 1 that is spread evenly
		// and is always the same. Different primes give different, unrelated sequences.
		double halton(std::size_t n, std::size_t base) {
			double fraction = 1.0;
			double result = 0.0;
			while (n > 0) {
				fraction /= static_cast<double>(base);
				result += fraction * static_cast<double>(n % base);
				n /= base;
			}
			return result;
		}

		std::vector<std::size_t> firstPrimes(std::size_t count) {
			std::vector<std::size_t> primes;
			for (std::size_t candidate = 2; primes.size() < count; ++candidate) {
				const bool isPrime = std::none_of(primes.begin(), primes.end(),
					[candidate](std::size_t p) { return candidate % p == 0; });
				if (isPrime) primes.push_back(candidate);
			}
			return primes;
		}

		// How fast a small error grows (above 1) or shrinks (below 1) from one repeat to the next when the
		// matrix 'm' (k by k, row by row) is applied again and again. Two fixed starting patterns are used,
		// all ones and alternating signs, so the answer is the same on every run and a loop that flips sign
		// at every repeat is caught too. The rate is the average growth over the second half of the steps.
		double growthOf(const std::vector<double>& m, std::size_t k) {
			constexpr int kSteps = 200;
			double worst = 0.0;

			for (int pattern = 0; pattern < 2; ++pattern) {
				std::vector<double> v(k), w(k);
				for (std::size_t i = 0; i < k; ++i) {
					v[i] = (pattern == 0 || i % 2 == 0 ? 1.0 : -1.0) / std::sqrt(static_cast<double>(k));
				}

				double logSum = 0.0;
				int counted = 0;
				bool vanished = false;
				for (int step = 0; step < kSteps; ++step) {
					double length = 0.0;
					for (std::size_t i = 0; i < k; ++i) {
						double sum = 0.0;
						for (std::size_t j = 0; j < k; ++j) sum += m[i * k + j] * v[j];
						w[i] = sum;
						length += sum * sum;
					}
					length = std::sqrt(length);
					if (length == 0.0) { vanished = true; break; }   // the error disappears completely

					if (step >= kSteps / 2) {   // 'v' has length 1, so 'length' is the growth of this step
						logSum += std::log(length);
						++counted;
					}
					for (std::size_t i = 0; i < k; ++i) v[i] = w[i] / length;
				}
				if (!vanished) worst = std::max(worst, std::exp(logSum / counted));
			}
			return worst;
		}

		// The growth rate of one loop: the worst growth over a set of sample points spread over the
		// ranges of its members. At each point the influences between the members are replaced by their
		// slopes, and the damping is included, because that is the version of the loop the solver runs.
		double growthRate(const PropertyRegistry& properties, const std::vector<std::vector<Incoming>>& incoming,
			const std::vector<PropertyId>& members) {
			constexpr std::size_t kSamples = 64;
			constexpr double kStep = 1e-6;   // for measuring a slope

			const std::size_t k = members.size();
			std::vector<std::size_t> slot(properties.size(), kNone);   // property -> place in 'members'
			for (std::size_t i = 0; i < k; ++i) slot[members[i] - 1] = i;
			const std::vector<std::size_t> primes = firstPrimes(k);

			double worst = 0.0;
			std::vector<double> point(k);
			std::vector<double> m(k * k);
			for (std::size_t sample = 1; sample <= kSamples; ++sample) {
				for (std::size_t i = 0; i < k; ++i) {
					const Bounds b = properties.byId(members[i])->bounds;
					point[i] = b.lower + halton(sample, primes[i]) * (b.upper - b.lower);
				}

				// One repeat moves x to (1 - d) * x + d * (what the member wants), so the matrix is
				// (1 - d) on the diagonal plus d times the slope of every influence between members.
				std::fill(m.begin(), m.end(), 0.0);
				for (std::size_t i = 0; i < k; ++i) {
					m[i * k + i] = 1.0 - kDamping;
					for (const Incoming& in : incoming[members[i] - 1]) {
						const std::size_t j = slot[in.source - 1];
						if (j == kNone) continue;   // pushed from outside the loop: a constant here
						const InfluenceEdge& edge = properties.byId(in.source)->influences[in.edge];
						const double slope = edge.coefficient
							* (responseShape(edge.mechanism, point[j] + kStep, edge.params)
								- responseShape(edge.mechanism, point[j] - kStep, edge.params)) / (2.0 * kStep);
						m[i * k + j] += kDamping * slope;
					}
				}
				worst = std::max(worst, growthOf(m, k));
			}
			return worst;
		}

		// Looks at every loop. A loop that would grow is a problem; one close to the limit is a warning.
		void checkLoops(const PropertyRegistry& properties, const std::vector<std::vector<Incoming>>& incoming,
			const std::vector<EvalStep>& order, BuildResult& result) {
			for (const EvalStep& step : order) {
				if (!step.isLoop) continue;

				const double rate = growthRate(properties, incoming, step.properties);
				const std::string where = "loop " + namesOf(properties, step.properties);

				if (rate >= kRejectGrowth) {
					const std::string why = "growth rate " + std::to_string(rate)
						+ " is 1 or more: the loop would grow out of control instead of settling";
					logger::error("Engine::build: ", where, ": ", why);
					result.problems.push_back({ where, why });
				}
				else if (rate >= kWarnGrowth) {
					const std::string why = "growth rate " + std::to_string(rate)
						+ " is close to 1: the loop settles very slowly and may not settle at the extremes";
					logger::info("Engine::build: ", where, ": ", why);
					result.warnings.push_back({ where, why });
				}
			}
		}

		// ---------- evaluating ----------

		// Mixes the spell: the value of every property before any influence is added ("base"),
		// indexed by property id - 1. Each ingredient's value is multiplied by the amplify factor first.
		// A property that no ingredient has is 0, and every result is forced into the property's range.
		std::vector<double> mix(const Spell& spell, const PropertyRegistry& properties,
			const ComponentRegistry& components, std::vector<Warning>& warnings) {
			std::vector<double> factor(properties.size(), 1.0);
			for (const Amplify& a : spell.amplify) {
				if (properties.byId(a.property) == nullptr) {
					warnings.push_back({ "amplify", "property id " + std::to_string(a.property) + " does not exist; ignored" });
					continue;
				}
				factor[a.property - 1] = a.factor;
			}

			std::vector<double> base(properties.size(), 0.0);
			std::vector<Contribution> parts;
			for (const property& p : properties.all()) {
				// Only summed properties are scaled by the energy budget (design document 4.2).
				const double budget = p.aggregationKind == AggregationKind::Sum ? spell.energyBudget : 1.0;

				parts.clear();
				for (const Ingredient& ingredient : spell.ingredients) {
					const component& c = *components.byId(ingredient.component);
					const auto found = c.properties.find(p.propertyId);
					if (found == c.properties.end()) continue;   // this component says nothing about p
					parts.push_back({ ingredient.component, found->second * factor[p.propertyId - 1],
						ingredient.amount * budget });
				}
				base[p.propertyId - 1] = ClampToBounds(aggregate(p.aggregationKind, parts), p.bounds);
			}
			return base;
		}

		// 'start' plus everything the incoming influences add, given the current values of the sources.
		// When 'pushes' is given, every push is recorded there for the trace.
		double addPushes(const PropertyRegistry& properties, const std::vector<Incoming>& incoming,
			const std::vector<double>& value, double start, std::vector<Push>* pushes) {
			double total = start;
			for (const Incoming& in : incoming) {
				const InfluenceEdge& edge = properties.byId(in.source)->influences[in.edge];
				const double input = value[in.source - 1];
				const double output = edge.coefficient * responseShape(edge.mechanism, input, edge.params);
				total += output;
				if (pushes != nullptr) pushes->push_back({ in.source, edge.mechanism, edge.coefficient, input, output });
			}
			return total;
		}

		// A property that is not in a loop: base plus pushes, once, then forced into its range.
		void applySingle(const PropertyRegistry& properties, const std::vector<Incoming>& incoming, PropertyId id,
			const std::vector<double>& base, std::vector<double>& value, Result& result) {
			TraceEntry entry;
			entry.property = id;
			entry.base = base[id - 1];
			entry.raw = addPushes(properties, incoming, value, entry.base, &entry.pushes);
			entry.clamped = ClampToBounds(entry.raw, properties.byId(id)->bounds);

			value[id - 1] = entry.clamped;
			result.trace.push_back(std::move(entry));
		}

		// A loop: repeat "work out what each member wants (its base plus all pushes, using the values of the
		// previous repeat), then move 70% of the way there" until nothing moves any more.
		// Every member is worked out from the previous repeat's values and never from a fresh one, so the
		// answer does not depend on which member is listed first (design document 6.3).
		void settleLoop(const PropertyRegistry& properties, const std::vector<std::vector<Incoming>>& incoming,
			const EvalStep& step, const std::vector<double>& base, std::vector<double>& value, Result& result) {
			const std::vector<PropertyId>& members = step.properties;
			std::vector<double> next(members.size());

			int repeat = 0;
			bool settled = false;
			while (!settled && repeat < kMaxRepeats) {
				++repeat;
				double biggestMove = 0.0;
				for (std::size_t i = 0; i < members.size(); ++i) {
					const PropertyId id = members[i];
					const Bounds bounds = properties.byId(id)->bounds;
					const double wanted = ClampToBounds(
						addPushes(properties, incoming[id - 1], value, base[id - 1], nullptr), bounds);
					const double old = value[id - 1];
					next[i] = ClampToBounds(old + kDamping * (wanted - old), bounds);
					biggestMove = std::max(biggestMove, std::abs(next[i] - old));
				}
				for (std::size_t i = 0; i < members.size(); ++i) value[members[i] - 1] = next[i];
				settled = biggestMove < kTolerance;
			}

			// One trace entry per member, explaining the values it ended with: what it was pushed by at
			// those values, and the number of repeats it took.
			for (PropertyId id : members) {
				TraceEntry entry;
				entry.property = id;
				entry.base = base[id - 1];
				entry.raw = addPushes(properties, incoming[id - 1], value, entry.base, &entry.pushes);
				entry.clamped = value[id - 1];
				entry.repeat = repeat;
				result.trace.push_back(std::move(entry));
			}

			if (!settled) {
				result.converged = false;
				result.warnings.push_back({ "loop " + namesOf(properties, members),
					"did not settle within " + std::to_string(kMaxRepeats) + " repeats; the values are the last ones" });
			}
		}
	}

	Engine::Engine(PropertyRegistry properties, ComponentRegistry components)
		: properties_(std::move(properties)), components_(std::move(components)) {}

	BuildResult Engine::build(PropertyRegistry properties, ComponentRegistry components) {
		BuildResult result;

		if (properties.size() == 0) {
			logger::error("Engine::build: the model has no properties");
			result.problems.push_back({ "engine", "the model has no properties" });
			return result;
		}

		Engine engine(std::move(properties), std::move(components));

		engine.incoming_ = makeIncoming(engine.properties_);
		engine.order_ = makeOrder(engine.properties_);

		checkLoops(engine.properties_, engine.incoming_, engine.order_, result);
		if (!result.problems.empty()) return result;   // no engine for a model that cannot be used

		result.engine = std::move(engine);
		return result;
	}

	const std::vector<Incoming>& Engine::incoming(PropertyId target) const noexcept {
		static const std::vector<Incoming> none;
		if (target == 0 || target > incoming_.size()) return none;
		return incoming_[target - 1];
	}

	Result Engine::evaluate(const Spell& spell) const {
		validateSpell(spell, components_);

		Result result(properties_.size());
		const std::vector<double> base = mix(spell, properties_, components_, result.warnings);

		std::vector<double> value = base;   // the current value of every property, indexed by id - 1
		for (const EvalStep& step : order_) {
			if (step.isLoop) {
				settleLoop(properties_, incoming_, step, base, value, result);
			}
			else {
				const PropertyId id = step.properties.front();
				applySingle(properties_, incoming_[id - 1], id, base, value, result);
			}
		}

		for (const property& p : properties_.all()) {
			result.state.set(p.propertyId, value[p.propertyId - 1]);
		}
		return result;
	}
}
