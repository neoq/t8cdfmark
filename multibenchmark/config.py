import collections
Config = collections.namedtuple("Config", "repetition nodes tasks_per_node storage_mode cmode comm_mode num_element_wise_variables bytes_hint")

benchmark_executable_path = "path/to/t8cdfmark"
def configurations():
	# how do the different mpi access patterns scale - with nodes and with tpn
	for repetition in range(3):
		for comm_mode in ["NC_INDEPENDENT", "NC_COLLECTIVE", "file_per_process"]:
			for storage_mode in ["NC_CONTIGUOUS", "NC_CHUNKED"]:
				for cmode in ["netcdf4_hdf5", "cdf5"]:
					if cmode == "cdf5" and comm_mode != "file_per_process":
						continue
					
					for nodes in [1,4,16,64]:
						yield Config(repetition=repetition, nodes=nodes, tasks_per_node=10, storage_mode=storage_mode, cmode=cmode, comm_mode=comm_mode, num_element_wise_variables=10, bytes_hint=100000000000)
					for tasks_per_node in [1,10,20]:
						yield Config(repetition=repetition, nodes=16, tasks_per_node=tasks_per_node, storage_mode=storage_mode, cmode=cmode, comm_mode=comm_mode, num_element_wise_variables=10, bytes_hint=100000000000)
	
	# what are the best defaults for t8code?
	# write a terrabyte on 64*20 processes with various settings
	for repetition in range(3):
		for comm_mode in ["NC_INDEPENDENT", "NC_COLLECTIVE", "file_per_process"]:
			for storage_mode in ["NC_CONTIGUOUS", "NC_CHUNKED"]:
				for cmode in ["netcdf4_hdf5", "cdf5"]:
					if cmode == "cdf5" and comm_mode != "file_per_process":
						continue
					yield Config(repetition=repetition, nodes=64, tasks_per_node=20, storage_mode=storage_mode, cmode=cmode, comm_mode=comm_mode, num_element_wise_variables=300, bytes_hint=1000000000000)
		