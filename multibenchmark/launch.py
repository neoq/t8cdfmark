import subprocess
import pathlib
import os

import config

job_id = None
for c in list(dict.fromkeys(config.configurations())):
	print(f"scheduling {c}")
	workdir = pathlib.Path(f"nodes{c.nodes}-tasks_per_node{c.tasks_per_node}/{c.cmode}-{c.storage_mode}-{c.comm_mode}-{c.num_element_wise_variables}-{c.bytes_hint}/{c.repetition}")
	os.makedirs(workdir, exist_ok=True)
	if (workdir/"success").is_file():
		print(f"skipping {c}")
		continue
	
	benchmark_command = f"module load netcdf-c openmpi; srun {config.benchmark_executable_path} --num_element_wise_variables={c.num_element_wise_variables} --pseudo_random:bytes={c.bytes_hint} --netcdf_version={c.cmode} --storage_mode={c.storage_mode} --mpi_access={c.comm_mode} && echo 1 >success; du -c --apparent-size --block-size=1 t8_netcdf_performance_test* >apparent_storage; du -c --block-size=1 t8_netcdf_performance_test* >storage"
	sbatch_command = [
		"sbatch",
		"--parsable",
		"--time=10:00",
		f"--mem-per-cpu={max(c.bytes_hint*2//(c.tasks_per_node * c.nodes * 1000),10000)}KB",
		"--constraint=scratch",
		f"--nodes={c.nodes}",
		f"--ntasks-per-node={c.tasks_per_node}",
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


	sbatch_cleanup_command = [
		"sbatch",
		"--parsable",
		"--time=5:00",
		"--constraint=scratch",
		"--wrap=find -name \"t8_netcdf_performance_test*\" -delete",
		f"--dependency=afterany:{job_id}"
	]

	try:
		sbatch_cleanup = subprocess.run(sbatch_cleanup_command, capture_output=True, cwd=workdir, check=True)
	except subprocess.CalledProcessError as e:
		print(e.stdout, e.stderr)
		raise
	job_id = int(sbatch_cleanup.stdout)

