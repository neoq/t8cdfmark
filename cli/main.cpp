

#include <netcdf.h>
#include <netcdf_par.h>
#include <sc_options.h>
#include <t8.h>
#include <t8_forest_netcdf.h>

#include "get_scenarios.hpp"
#include "scenarios/scenario.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <string_view>
#include <vector>

using namespace std::string_view_literals;

namespace {

struct Config {
	std::unique_ptr<t8cdfmark::Scenario> scenario;
	int netcdf_var_storage_mode;
	int netcdf_mpi_access = NC_COLLECTIVE;
	int fill_mode;
	int cmode;
	int num_element_wise_variables = 0;
	bool file_per_process_mode = false;
};

auto parse_args(int argc, char** argv) {
	auto opts = t8cdfmark::unique_ptr_sc_options_t(sc_options_new(argv[0]));

	int fill = 0;
	int num_element_wise_variables = 0;
	const char* storage_mode = "chunked";
	const char* mpi_access = "collective";
	const char* netcdf_ver = "netcdf4_hdf5";
	const char* scenario_name = "pseudo_random";

	// TODO: implement help switch myself

	// add common args:
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

	auto scenarios = t8cdfmark::get_scenarios();
	for (const auto& scenario : scenarios) {
		auto scenario_opts = scenario->make_options();
		// i think this function copies the options and does not take ownership
		sc_options_add_suboptions(
			opts.get(), scenario_opts.get(), scenario->name.c_str()
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
			return scenario->name == scenario_name;
		}
	);
	if (matched_scenario_it == scenarios.end()) {
		throw std::runtime_error{"scenario not known"};
	}

	Config config{
		.scenario = std::move(*matched_scenario_it),
		.fill_mode = fill ? NC_FILL : NC_NOFILL,
		.num_element_wise_variables = num_element_wise_variables};

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

struct element_wise_variable {
	std::vector<t8_nc_int64_t> data;
	t8cdfmark::unique_ptr_sc_array_t data_view;
	t8cdfmark::unique_ptr_t8_netcdf_variable_t netcdf_variable;
};

auto time_writing_netcdf(
	t8_forest_t forest, sc_MPI_Comm comm, const Config& config,
	const std::vector<element_wise_variable>& element_wise_variables
) {
	std::vector<t8_netcdf_variable_t*> netcdf_variables(
		element_wise_variables.size()
	);
	std::transform(
		element_wise_variables.cbegin(), element_wise_variables.cend(),
		netcdf_variables.begin(),
		[](const element_wise_variable& variable) {
			return variable.netcdf_variable.get();
		}
	);

	sc_MPI_Barrier(comm);
	// Wtime is guaranteed to be monotonic
	const auto start_time = sc_MPI_Wtime();

	t8_forest_write_netcdf_ext(
		forest, "t8_netcdf_performance_test",
		"T8 NetCDF writing Performance Test", 3, netcdf_variables.size(),
		netcdf_variables.data(), comm, config.netcdf_var_storage_mode, nullptr,
		config.netcdf_mpi_access, config.fill_mode, config.cmode,
		config.file_per_process_mode
	);

	const auto end_time = sc_MPI_Wtime();
	const double seconds = end_time - start_time;
	double global_seconds_max = NAN;
	int retval = sc_MPI_Reduce(
		&seconds, &global_seconds_max, 1, sc_MPI_DOUBLE, sc_MPI_MAX, 0, comm
	);
	if (retval != sc_MPI_SUCCESS) {
		throw std::runtime_error{"failed calculating the total runtime"};
	}
	return global_seconds_max;
}

template <typename Rne>
auto make_element_wise_variable(t8_locidx_t num_local_elements, Rne& rne) {
	element_wise_variable result{
		std::vector<t8_nc_int64_t>(num_local_elements)};

	{
		auto dist = std::uniform_int_distribution<long long>{
			std::numeric_limits<t8_nc_int64_t>::min(),
			std::numeric_limits<t8_nc_int64_t>::max()};

		for (auto& a : result.data) {
			a = dist(rne);
		}
	}

	result.data_view = t8cdfmark::unique_ptr_sc_array_t{sc_array_new_data(
		result.data.data(), sizeof(t8_nc_int64_t), result.data.size()
	)};
	result.netcdf_variable =
		t8cdfmark::unique_ptr_t8_netcdf_variable_t{t8_netcdf_create_var(
			T8_NETCDF_INT64, "random_values", "random values", "units",
			result.data_view.get()
		)};
	return result;
}

auto make_element_wise_variables(
	t8_locidx_t num_local_elements, int num_element_wise_variables
) {
	std::vector<element_wise_variable> element_wise_variables(
		num_element_wise_variables
	);
	std::mt19937_64 rne;
	for (auto& variable : element_wise_variables) {
		variable = make_element_wise_variable(num_local_elements, rne);
	}
	return element_wise_variables;
}

} // namespace

int main(int argc, char** argv) {
	MPI_Init(&argc, &argv); // TODO: error handling
	sc_init(sc_MPI_COMM_WORLD, true, true, nullptr, SC_LP_ESSENTIAL);
	t8_init(SC_LP_PRODUCTION);
	int exit_code = EXIT_SUCCESS;

	t8_forest_t forest = nullptr;
	try {
		int mpirank;
		if (sc_MPI_Comm_rank(sc_MPI_COMM_WORLD, &mpirank) != sc_MPI_SUCCESS) {
			throw std::runtime_error{"failed to determine rank"};
		}

		auto config = parse_args(argc, argv);

		forest = config.scenario->make_forest(sc_MPI_COMM_WORLD);

		// const auto storage =
		// 	calculate_actual_storage(forest, config.num_element_wise_variables);

		// if (mpirank == 0) {
		// 	print_storage(storage)
		// }

		auto element_wise_variables = make_element_wise_variables(
			t8_forest_get_local_num_elements(forest),
			config.num_element_wise_variables
		);

		const auto time_taken =
			time_writing_netcdf(forest, sc_MPI_COMM_WORLD, config, element_wise_variables);

		t8_productionf("%f", time_taken);

	} catch (const std::exception& e) {
		t8_productionf(e.what());
		exit_code = EXIT_FAILURE;
	}

	if (forest != nullptr) {
		t8_forest_unref(&forest);
	}

	sc_finalize();
	MPI_Finalize();
	return exit_code;
}