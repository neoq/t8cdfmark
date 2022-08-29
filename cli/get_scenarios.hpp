#pragma once

#include "scenarios/scenario.hpp"

#include <vector>
#include <memory>

namespace t8cdfmark {
auto get_scenarios() -> std::vector<std::unique_ptr<t8cdfmark::Scenario>>;
} // namespace t8cdfmark