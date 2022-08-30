#pragma once

#include <memory>
#include <mpi.h>
#include <netcdf.h>
#include <sc_options.h>
#include <t8.h>
#include <t8_forest.h>

#include "../utils.hpp"

namespace t8cdfmark {
struct Scenario {
	explicit Scenario(std::string name) : name{std::move(name)} {}
	std::string name;
	virtual unique_ptr_sc_options_t make_options() = 0;
	virtual t8_forest_t make_forest(sc_MPI_Comm comm, int num_element_wise_variables) const = 0;
	virtual ~Scenario() = default;
};
} // namespace t8cdfmark