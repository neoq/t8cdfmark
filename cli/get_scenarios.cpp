#include "get_scenarios.hpp"

#include "scenarios/pseudo_random.hpp"

namespace t8cdfmark {
auto get_scenarios() -> std::vector<std::unique_ptr<t8cdfmark::Scenario>> {
	std::vector<std::unique_ptr<t8cdfmark::Scenario>> scenarios;
	scenarios.push_back(
		std::make_unique<scenarios::pseudo_random>("pseudo_random")
	);
	return scenarios;
}
} // namespace t8cdfmark