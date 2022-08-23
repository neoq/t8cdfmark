import subprocess
import pathlib
import os
import sys

print("nodes,tasks_per_node,storage_mode,cmode,comm_mode,repetition,seconds")
for repetition in range(3):
	for nodes in [64]:
		for tasks_per_node in [1]:
			for storage_mode in ["NC_CONTIGUOUS", "NC_CHUNKED"]:
				for cmode in ["netcdf4_hdf5", "classic"]:
					for comm_mode in ["NC_INDEPENDENT", "NC_COLLECTIVE", "--multifile"]:
						if cmode == "classic" and comm_mode != "--multifile":
							continue

						workdir = pathlib.Path(f"nodes{nodes}-tasks_per_node{tasks_per_node}/{cmode}-{storage_mode}-{comm_mode}/{repetition}")
						
						if not (workdir/"success").is_file():
							print(f"{workdir} failed")
							continue
							#sys.exit(1)
						
						time = float(subprocess.run("rg \"The time elapsed to write the netCDF-4 File is: ([0-9]+\.[0-9]+)\" -or '$1' slurm-*", capture_output=True, cwd=workdir, check=True, shell=True).stdout)
						
						print(f"{nodes},{tasks_per_node},{storage_mode},{cmode},{comm_mode},{repetition},{time}")

						
