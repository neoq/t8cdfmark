

#include <netcdf.h>
#include <netcdf_par.h>
#include <sc_options.h>
#include <t8.h>
#include <t8_forest_netcdf.h>

// for node calc
#include <t8_element_cxx.hxx>

#include "get_scenarios.hpp"
#include "scenarios/scenario.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <string_view>
#include <vector>

using namespace std::string_view_literals;

namespace {
constexpr auto result_fname = "results.json";

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
	const char* storage_mode = "NC_CHUNKED";
	const char* mpi_access = "NC_COLLECTIVE";
	const char* netcdf_ver = "netcdf4_hdf5";
	const char* scenario_name = "pseudo_random";

	// TODO: implement help switch myself

	// add common args:
	sc_options_add_switch(opts.get(), 'f', "fill", &fill, "Activate NC_FILL");
	sc_options_add_string(
		opts.get(), 's', "storage_mode", &storage_mode, "NC_CHUNKED",
		"storage mode"
	);
	sc_options_add_string(
		opts.get(), 'm', "mpi_access", &mpi_access, "NC_COLLECTIVE",
		"mpi access"
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
	if (netcdf_ver == "cdf5"sv) {
		config.cmode = NC_64BIT_DATA;
	} else if (netcdf_ver == "netcdf4_hdf5"sv) {
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
	std::unique_ptr<std::string> var_name;
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
	const auto mpi_status = sc_MPI_Reduce(
		&seconds, &global_seconds_max, 1, sc_MPI_DOUBLE, sc_MPI_MAX, 0, comm
	);
	if (mpi_status != sc_MPI_SUCCESS) {
		throw std::runtime_error{"failed calculating the total runtime"};
	}
	return global_seconds_max;
}

template <typename Rne>
auto make_element_wise_variable(
	t8_locidx_t num_local_elements, Rne& rne, std::string var_name
) {
	element_wise_variable result{
		.data = std::vector<t8_nc_int64_t>(num_local_elements),
		.var_name = std::make_unique<std::string>(std::move(var_name))};

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
			T8_NETCDF_INT64, result.var_name->c_str(), "random values", "units",
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
	std::ptrdiff_t i = 0;
	for (auto& variable : element_wise_variables) {
		variable = make_element_wise_variable(
			num_local_elements, rne, "random_values" + std::to_string(i++)
		);
	}
	return element_wise_variables;
}

auto get_local_num_nodes(t8_forest_t forest) {
	std::ptrdiff_t num_local_nodes = 0;
	const auto num_local_trees = t8_forest_get_num_local_trees(forest);
	for (t8_locidx_t ltree_id = 0; ltree_id < num_local_trees; ++ltree_id) {
		const auto scheme = t8_forest_get_eclass_scheme(
			forest, t8_forest_get_tree_class(forest, ltree_id)
		);
		const auto num_local_tree_elem =
			t8_forest_get_tree_num_elements(forest, ltree_id);
		for (t8_locidx_t local_elem_id = 0; local_elem_id < num_local_tree_elem;
		     ++local_elem_id) {
			num_local_nodes += t8_element_shape_num_vertices(
				scheme->t8_element_shape(t8_forest_get_element_in_tree(
					forest, ltree_id, local_elem_id
				))
			);
		}
	}

	return num_local_nodes;
}

/*
 * collective
 */
auto get_global_num_nodes(t8_forest_t forest) {
	const long long local_num_nodes = get_local_num_nodes(forest);
	long long global_num_nodes = 0;
	// TODO: error handling
	const auto mpi_status = sc_MPI_Reduce(
		&local_num_nodes, &global_num_nodes, 1, sc_MPI_LONG_LONG_INT,
		sc_MPI_SUM, 0, t8_forest_get_mpicomm(forest)
	);
	return global_num_nodes;
}

/*
 * collective
 */
long long calculate_actual_storage(
	t8_forest_t forest, int num_element_wise_variables, int mpirank
) {
	const long long nMesh3D_node = get_global_num_nodes(forest);
	t8_global_productionf("forest has %lld nodes\n", nMesh3D_node);
	const long long nMesh3D_vol = t8_forest_get_global_num_elements(forest);
	if (mpirank == 0) {
		return nMesh3D_vol * 4 + nMesh3D_vol * 8 + nMesh3D_vol * 8 * 8 +
		       3 * nMesh3D_node * 8 +
		       num_element_wise_variables * nMesh3D_vol * 8;
	}
	return 0;
}

template <typename Sec, typename Bytes>
auto output_results(Sec seconds, Bytes storage) {
	std::ofstream results{result_fname};
	// enough digits to achieve exact rountrip conversion
	results << std::setprecision(std::numeric_limits<double>::max_digits10)
			<< R"({"actual_information_bytes":)" << storage << R"(,"seconds":)"
			<< seconds << R"(,"throughput_B/s":)" << (double(storage) / seconds)
			<< '}';
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

		forest = config.scenario->make_forest(
			sc_MPI_COMM_WORLD, config.num_element_wise_variables
		);

		const long long storage = calculate_actual_storage(
			forest, config.num_element_wise_variables, mpirank
		);

		t8_global_productionf(
			"the constructed forest will consume approximately %lld bytes\n",
			storage
		);

		auto element_wise_variables = make_element_wise_variables(
			t8_forest_get_local_num_elements(forest),
			config.num_element_wise_variables
		);

		t8_global_productionf("finished creating element wise variables\n");

		const double time_taken = time_writing_netcdf(
			forest, sc_MPI_COMM_WORLD, config, element_wise_variables
		);

		t8_global_productionf(
			"took %fs. Results written to %s\n", time_taken, result_fname
		);
		if (mpirank == 0) {
			output_results(time_taken, storage);
		}

	} catch (const std::exception& e) {
		t8_global_productionf(e.what());
		exit_code = EXIT_FAILURE;
	}

	if (forest != nullptr) {
		t8_forest_unref(&forest);
	}

	sc_finalize();
	MPI_Finalize();
	return exit_code;
}