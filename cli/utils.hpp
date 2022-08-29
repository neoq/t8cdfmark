#pragma once

#include <sc_containers.h>
#include <sc_options.h>
#include <t8_netcdf.h>

#include <memory>

namespace t8cdfmark {
struct sc_options_t_deleter {
	void operator()(sc_options_t* options) const {
		sc_options_destroy(options);
	}
};
using unique_ptr_sc_options_t =
	std::unique_ptr<sc_options_t, sc_options_t_deleter>;
struct sc_array_t_deleter {
	void operator()(sc_array_t* array) const { sc_array_destroy(array); }
};
using unique_ptr_sc_array_t = std::unique_ptr<sc_array_t, sc_array_t_deleter>;
struct t8_netcdf_variable_t_deleter {
	void operator()(t8_netcdf_variable_t* netcdf_variable) const {
		t8_netcdf_variable_destroy(netcdf_variable);
	}
};
using unique_ptr_t8_netcdf_variable_t =
	std::unique_ptr<t8_netcdf_variable_t, t8_netcdf_variable_t_deleter>;

} // namespace t8cdfmark