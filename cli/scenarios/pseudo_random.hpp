#pragma once

#include <mpi.h>
#include <sc_options.h>
#include <t8.h>

#include "scenario.hpp"

namespace scenarios {
struct pseudo_random : t8cdfmark::Scenario {

	explicit pseudo_random(std::string name)
		: t8cdfmark::Scenario(std::move(name)) {}

	std::size_t desired_bytes = 1'073'741'824u;

	std::unique_ptr<sc_options_t, decltype(sc_options_destroy)>
	make_options() override;
	t8_forest_t make_forest(sc_MPI_Comm comm) const override;
};
} // namespace scenarios