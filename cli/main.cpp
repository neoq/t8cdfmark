

#include <mpi.h>
#include <netcdf.h>
#include <sc_options.h>
#include <t8.h>

#include "scenarios/pseudo_random.hpp"
#include "scenarios/scenario.hpp"

#include <cstdio>
#include <iostream>
#include <memory>
#include <vector>

namespace {
struct Config {
	int netcdf_var_storage_mode;
	int netcdf_mpi_access = NC_COLLECTIVE;
	int fill_mode;
	int cmode;
	bool file_per_process_mode = false;
};

auto get_scenarios() {
	std::vector<std::unique_ptr<t8cdfmark::Scenario>> scenarios;
	scenarios.push_back(
		std::make_unique<scenarios::pseudo_random>("pseudo_random")
	);
	return scenarios;
}

auto parse_args(int argc, char** argv) {
	auto opts = std::unique_ptr{sc_options_new(argv[0]), sc_options_destroy};

	int fill = 0;
	const char* storage_mode = "chunked";
	const char* mpi_access = "collective";
	const char* netcdf_ver = "netcdf4_hdf5";
	const char* scenario_name = "pseudo_random";

	sc_options_add_switch(opts.get(), 'f', "fill", &fill, "Activate NC_FILL");
	sc_options_add_string(
		opts.get(), 's', "storage_mode", &storage_mode, "chunked",
		"storage mode"
	);
	sc_options_add_string(
		opts.get(), 'm', "mpi_access", &mpi_access, "collective", "mpi access"
	);
	sc_options_add_string(
		opts.get(), 'c', "netcdf_version", &netcdf_ver, "netcdf4_hdf5",
		"netcdf version"
	);
	sc_options_add_string(
		opts.get(), 'z', "scenario", &scenario_name, "pseudo_random", "scenario"
	);

	auto scenarios = get_scenarios();
	for (const auto& scenario : scenarios) {
		sc_options_add_
	}

	sc_options_print_usage(
		t8_get_package_id(), SC_LP_ERROR, opts.get(), "asdfasdfasdfasdfasdfasdf"
	);
	const auto parse_result = sc_options_parse(
		t8_get_package_id(), SC_LP_ERROR, opts.get(), argc, argv
	);
	if (parse_result == -1) {
		return EXIT_FAILURE;
	}
	sc_options_print_summary(t8_get_package_id(), SC_LP_PRODUCTION, opts.get());

	/*
	Config<Scenarios...> config;
	// general config options
	// string option for scenario
	(void(scenarios.add_options(opts)), ...);
	// parse options

	[] -> var_t {
	    scenario == sce
	}
	*/
}
} // namespace

int main(int argc, char** argv) {
	MPI_Init(&argc, &argv); // TODO: error handling
	sc_init(sc_MPI_COMM_WORLD, true, true, nullptr, SC_LP_ESSENTIAL);
	t8_init(SC_LP_PRODUCTION);

	auto config = parse_args(argc, argv);

	auto forest = make_forest(config);

	add_variables(forest);

	time_writing_netcdf(forest);

	unref(forest);

	sc_finalize();
	MPI_Finalize();
}