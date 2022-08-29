#include "pseudo_random.hpp"

#include <t8_cmesh/t8_cmesh_examples.h>
#include <t8_schemes/t8_default/t8_default_cxx.hxx>

#include <random>

namespace {

/** calculates the minimum count of elements needed to consume bytes bytes
 * \param [in] bytes the requested storage
 */
double elements_needed_for_bytes(long long bytes) {
	/* the total forest storage is derived from the ugrid conventions:
	   nMaxMesh3D_vol_nodes = 8
	   nMesh3D_node <= nMesh3D_vol * nMaxMesh3D_vol_nodes
	   `storage = Mesh3D_vol_types + Mesh3D_vol_tree_id + Mesh3D_vol_nodes +
	   Mesh3D_node_x + Mesh3D_node_y + Mesh3D_node_z` `storage = nMesh3D_vol * 4
	   + nMesh3D_vol * 8 + nMesh3D_vol * nMaxMesh3D_vol_nodes * 8 + 3 *
	   (nMesh3D_node * 8)` we don't know the node count of each volume. `storage
	   <= nMesh3D_vol * (4 + 8 + 64 + 192)` `nMesh3D_vol >= storage / 268`
	 */
	return bytes / 268.0;
}

/** This struct contains the refinement information needed to produce a
 * forest of a desired storage size */
struct RefinementConfig {
	double additionally_refined_ratio = 0;
	int initial = 0;
};

/** calculates refinement parameters that you can use to build a forest with
 * roughly the given size in bytes. In practice, the consumed storage will be
 * lower, because not every element/volume has the maximum of 8 nodes that need
 * coordinate storage. \param [in] bytes the desired total storage size of a
 * forest created with the returned RefinementConfig
 */
RefinementConfig config_for_bytes(long long bytes) {
	/* calculate how many volumes/elements we need: */
	const double nMesh3D_vol = elements_needed_for_bytes(bytes);

	/* calculate config to get that many elements. Derivation:
	   i = initial_refinement; a = ratio of further refined
	   nMesh3D_vol $=
	   \left(1-a\right)16\cdot8^{i}+a\cdot16\cdot8^{\left(i+1\right)}$
	   -> `nMesh3D_vol = ((1-a)+a*8)*16*8**i`

	   i = $\lfloor\log_{8}(\text{nMesh3D_vol})-\log_{8}16\rfloor$
	   i = $\lfloor\log_{8}(\text{nMesh3D_vol}/16)\rfloor$

	   $$a = \frac{\text{nMesh3D_vol} - 16\cdot8^i}{7\cdot16\cdot8^i}$$
	   $$a = \frac{\text{nMesh3D_vol}}{7\cdot16\cdot8^i}-\frac{1}{7}$$
	   `a = nMesh3D_vol / (7*16*8**i) - 1/7`
	 */
	RefinementConfig config;
	config.initial = std::max(std::floor(std::log2(nMesh3D_vol / 16) / 3), 0.0);
	config.additionally_refined_ratio =
		nMesh3D_vol / (7 * 16 * std::pow(8.0, config.initial)) - 1 / 7.0;
	return config;
}
extern "C" {
/** forest user data used to adapt the forest to consume more storage
 * contains the ratio of how many elements need to be refined as well as a
 * pseudorandom number generator to decide which elements are refined. */
struct adapt_user_data {
	double additionally_refined_ratio;
	std::mt19937_64 rne;
};

/** models t8_forest_adapt_t. Pseudorandomly refines elements according to the
 * given ratio. */
int pseudo_random_adapt_fn(
	t8_forest_t forest, t8_forest_t forest_from, t8_locidx_t which_tree,
	t8_locidx_t lelement_id, t8_eclass_scheme_c* ts, const int is_family,
	const int num_elements, t8_element_t* elements[]
) {
	adapt_user_data& adapt_data =
		*static_cast<adapt_user_data*>(t8_forest_get_user_data(forest));

	std::bernoulli_distribution should_refine{
		adapt_data.additionally_refined_ratio};
	return should_refine(adapt_data.rne) ? 1 : 0;
} // extern "C"
}

} // namespace

namespace scenarios {
t8cdfmark::unique_ptr_sc_options_t pseudo_random::make_options() {
	auto opts = t8cdfmark::unique_ptr_sc_options_t{sc_options_new("")};
	sc_options_add_size_t(
		opts.get(), 'b', "bytes", &desired_bytes, 1'073'741'824u,
		"desired bytes"
	);
	return opts;
}
t8_forest_t pseudo_random::make_forest(sc_MPI_Comm comm) const {
	const auto refinement_config = config_for_bytes(desired_bytes);

	/* Build a (partitioned) uniform forest */
	t8_forest_t uniform = t8_forest_new_uniform(
		/* Construct a 3D hybrid hypercube as a cmesh */
		t8_cmesh_new_hypercube_hybrid(comm, 1, 0), t8_scheme_new_default_cxx(),
		refinement_config.initial, false, comm
	);

	adapt_user_data adapt_data{
		.additionally_refined_ratio =
			refinement_config.additionally_refined_ratio};
	t8_forest_t result;
	t8_forest_init(&result);
	t8_forest_set_user_data(result, &adapt_data);
	t8_forest_set_adapt(result, uniform, pseudo_random_adapt_fn, false);
	t8_forest_set_partition(result, nullptr, false);
	t8_forest_commit(result);
	// uniform is owned by result and does not need to be unref'd
	return result;
}
} // namespace scenarios