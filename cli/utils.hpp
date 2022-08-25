#pragma once

#include <sc_options.h>

#include <memory>

namespace t8cdfmark {
inline auto new_sc_options(const char* program_path) {
	return std::unique_ptr{sc_options_new(program_path), sc_options_destroy};
}
} // namespace t8cdfmark