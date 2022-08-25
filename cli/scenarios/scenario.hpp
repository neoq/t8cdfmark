#pragma once

#include <memory>
#include <mpi.h>
#include <netcdf.h>
#include <sc_options.h>
#include <t8.h>

namespace t8cdfmark {
struct Scenario {
	explicit Scenario(std::string name) : name{std::move(name)} {}
	std::string name;
	virtual std::unique_ptr<sc_options_t, decltype(sc_options_destroy)>
	make_options() = 0;
	virtual t8_forest_t make_forest(sc_MPI_Comm comm) const = 0;
	virtual ~Scenario() = default;
};
} // namespace t8cdfmark