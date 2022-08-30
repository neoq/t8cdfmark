import collections
Config = collections.namedtuple("Config", "repetition nodes tasks_per_node storage_mode cmode comm_mode num_element_wise_variables")

benchmark_executable_path = "/scratch1/users/hpctraining60/master/t8build/example/netcdf/benchmark/t8_write_forest_netcdf"
def configurations():
	for repetition in range(3):
		for nodes in [1,2,4,8,16,24,32,40,48,56,64]:
			for comm_mode in ["NC_COLLECTIVE", "file_per_process"]:
				for num_element_wise_variables in [0,10,300]:
					yield Config(repetition=repetition, nodes=nodes, tasks_per_node=4, storage_mode="NC_CONTIGUOUS", cmode="netcdf4_hdf5", comm_mode=comm_mode, num_element_wise_variables=num_element_wise_variables)