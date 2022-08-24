

#include <t8.h>

int main(int argc, char** argv) {
	sc_MPI_Init(&argc, &argv);
	sc_init(sc_MPI_COMM_WORLD, 1, 1, nullptr, SC_LP_ESSENTIAL);
	t8_init(SC_LP_PRODUCTION);

	sc_finalize();
	sc_MPI_Finalize();
}