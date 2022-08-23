import subprocess
import pathlib
import os

benchmark_executable_path = "/scratch1/users/hpctraining60/master/t8build/example/netcdf/benchmark/t8_write_forest_netcdf"

job_id = None

for repetition in range(3):
	for nodes in [64]:
		for tasks_per_node in [1]:
			for storage_mode in ["NC_CONTIGUOUS", "NC_CHUNKED"]:
				for cmode in ["netcdf4_hdf5", "classic"]:
					for comm_mode in ["NC_INDEPENDENT", "NC_COLLECTIVE", "--multifile"]:
						if cmode == "classic" and comm_mode != "--multifile":
							continue

						workdir = pathlib.Path(f"nodes{nodes}-tasks_per_node{tasks_per_node}/{cmode}-{storage_mode}-{comm_mode}/{repetition}")
						os.makedirs(workdir, exist_ok=True)
						if (workdir/"success").is_file():
							continue
						
						benchmark_command = f"module load netcdf-c openmpi; srun {benchmark_executable_path} 100000000000 NC_NOFILL {cmode} {storage_mode} {comm_mode} && echo 1 >success; du -c --apparent-size --block-size=1 T8_Example_NetCDF_Performance.nc* >apparent_storage; du -c --block-size=1 T8_Example_NetCDF_Performance.nc* >storage; rm T8_Example_NetCDF_Performance.nc*"
						sbatch_command = [
							"sbatch",
							"--parsable",
							"--time=30:00",
							"--constraint=scratch",
							f"--nodes={nodes}",
							f"--ntasks-per-node={tasks_per_node}",
							f"--wrap={benchmark_command}"
						]
						if job_id is not None:
							sbatch_command.append(f"--dependency=afterany:{job_id}")
						try:
							sbatch = subprocess.run(sbatch_command, capture_output=True, cwd=workdir, check=True)
						except subprocess.CalledProcessError as e:
							print(e.stdout, e.stderr)
							raise
						job_id = int(sbatch.stdout)
