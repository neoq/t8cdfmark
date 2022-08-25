

#include <mpi.h>
#include <netcdf.h>
#include <sc_options.h>
#include <t8.h>

#include "get_scenarios.hpp"
#include "scenarios/scenario.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

using std::string_view_literals;

namespace {

struct Config {
	std::unique_ptr<t8cdfmark::Scenario> scenario;
	int netcdf_var_storage_mode;
	int netcdf_mpi_access = NC_COLLECTIVE;
	int fill_mode;
	int cmode;
	bool file_per_process_mode = false;
};

auto parse_args(int argc, char** argv) {
	auto opts = t8cdfmark::new_sc_options(argv[0]);

	int fill = 0;
	int num_element_wise_variables = 0;
	const char* storage_mode = "chunked";
	const char* mpi_access = "collective";
	const char* netcdf_ver = "netcdf4_hdf5";
	const char* scenario_name = "pseudo_random";

	// TODO: implement help switch myself

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
	sc_options_add_int(
		opts.get(), 'v', "num_element_wise_variables",
		&num_element_wise_variables, 0, "num_element_wise_variables"
	);

	auto scenarios = get_scenarios();
	for (const auto& scenario : scenarios) {
		auto scenario_opts = scenario->make_options();
		// i think this function copies the options and does not take ownership
		sc_options_add_suboptions(
			opts.get(), scenario_opts.get(), scenario->id.c_str()
		);
	}

	const auto parse_result = sc_options_parse(
		t8_get_package_id(), SC_LP_ERROR, opts.get(), argc, argv
	);
	sc_options_print_summary(t8_get_package_id(), SC_LP_PRODUCTION, opts.get());
	if (parse_result == -1) {
		sc_options_print_usage(
			t8_get_package_id(), SC_LP_ERROR, opts.get(),
			"asdfasdfasdfasdfasdfasdf"
		);
		throw std::runtime_error{"bad usage"};
	}

	auto matched_scenario_it = std::find_if(
		scenarios.begin(), scenarios.end(),
		[scenario_name](auto& scenario) {
			return scenario->id == scenario_name;
		}
	);
	if (matched_scenario_it == scenarios.end()) {
		throw std::runtime_error{"scenario not known"};
	}

	Config config{
		.fill_mode = fill ? NC_FILL : NC_NOFILL,
		.scenario = std::move(*matched_scenario_it)};

	if (mpi_access == "NC_INDEPENDENT"sv) {
		config.netcdf_mpi_access = NC_INDEPENDENT;
	} else if (mpi_access == "NC_COLLECTIVE"sv) {
		config.netcdf_mpi_access = NC_COLLECTIVE;
	} else if (mpi_access == "file_per_process"sv) {
		config.file_per_process_mode = true;
	} else {
		throw std::runtime_error{"this argument is either NC_COLLECTIVE, "
		                         "NC_INDEPENDENT, or file_per_process"};
	}
	if (storage_mode == "NC_CONTIGUOUS"sv) {
		config.netcdf_var_storage_mode = NC_CONTIGUOUS;
	} else if (storage_mode == "NC_CHUNKED"sv) {
		config.netcdf_var_storage_mode = NC_CHUNKED;
	} else {
		throw std::runtime_error{
			"storage mode must be one of NC_CONTIGUOUS and NC_CHUNKED"};
	}
	if (netcdf_ver == "cdf5") {
		config.cmode = NC_64BIT_DATA;
	} else if (netcdf_ver == "netcdf4_hdf5") {
		config.cmode = NC_NETCDF4;
	} else {
		throw std::runtime_error{
			"cmode must be one of \"cdf5\" and \"netcdf4_hdf5\""};
	}

	return config;
}

auto ttime_netcdf_writing(t8_forest_t forest, sc_MPI_Comm comm, Config config) {
	sc_MPI_Barrier(comm);
	// Wtime is guaranteed to be monotonic
	const auto start_time = sc_MPI_Wtime();

	/* Write out the forest in netCDF format using the extended function which
	 * allows to set a specific variable storage and access pattern. */
	t8_forest_write_netcdf_ext(
		forest, "t8_netcdf_performance_test",
		"T8 NetCDF writing Performance Test", 3, 0, nullptr, comm,
		config.netcdf_var_storage_mode, nullptr, config.netcdf_mpi_access,
		config.fill_mode, config.cmode, config.file_per_process_mode
	);

	const auto end_time = sc_MPI_Wtime();
	const double seconds = end_time - start_time;
	double global_seconds_max;
	int retval = sc_MPI_Reduce(
		&seconds, &global_seconds_max, 1, sc_MPI_DOUBLE, sc_MPI_MAX, 0, comm
	);
	if (retval != MPI::SUCCESS) {
		throw std::runtime_error{"failed calculating the total runtime"};
	}
}
} // namespace

int main(int argc, char** argv) {
	MPI_Init(&argc, &argv); // TODO: error handling
	sc_init(sc_MPI_COMM_WORLD, true, true, nullptr, SC_LP_ESSENTIAL);
	t8_init(SC_LP_PRODUCTION);
	int exit_code = EXIT_SUCCESS;

	try {
		auto config = parse_args(argc, argv);

		auto forest = config.scenario->make_forest();

		add_variables(config.num_element_wise_variables, forest);

		time_writing_netcdf(config, forest);

	} catch (const std::exception& e) {
		t8_productionf(e.what());
		exit_code = EXIT_FAILURE;
	}

	unref(forest);

	sc_finalize();
	MPI_Finalize();
	return exit_code;
}