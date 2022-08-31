import subprocess
import pathlib
import os
import sys
import json

import config

print("nodes,tasks_per_node,storage_mode,cmode,comm_mode,num_element_wise_variables,repetition,actual_information_bytes,seconds,throughput_B/s")
for c in config.configurations():
	workdir = pathlib.Path(f"nodes{nodes}-tasks_per_node{tasks_per_node}/{c.cmode}-{c.storage_mode}-{c.comm_mode}-{c.num_element_wise_variables}-{c.bytes_hint}/{c.repetition}")
	
	if not (workdir/"success").is_file():
		print(f"{workdir} failed", file=sys.stderr)
		continue
	
	results = json.load(open(workdir/"results.json", "r"))
	print(f"{nodes},{tasks_per_node},{storage_mode},{cmode},{comm_mode},{repetition},{results["actual_information_byte/s"]},{results["second/s"]},{results["throughput_B/s"]}")

