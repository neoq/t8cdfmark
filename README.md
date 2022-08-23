# t8code netcdf benchmark suite (t8cdfmark)
This is a benchmark suite around the netCDF writing functionality of the [t8code](https://github.com/DLR-AMR/t8code) project. t8cdfmark offers a CLI to benchmark a single writing scenario. Scripts are provided to execute a number of benchmarks with the slurm scheduler and accumulate the results.

## CLI
The CLI has multiple scenarios to choose from, which is a coarse mesh in combination with a specific refinement strategy. For some, the CLI can take a storage hint and create a forest of roughly the desired size. Additionally, an arbitrary amount of random element-wise variables can be added to the forest.
