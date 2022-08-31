#!/bin/bash
# get source dir
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
echo $SCRIPT_DIR
find $SCRIPT_DIR -name "*.cpp" | xargs -n 1 -P 40 ccache mpicxx -std=c++17 -I/scratch/users/hpctraining60/master/t8code_install/include -c && mpicxx *.o -L/scratch/users/hpctraining60/master/t8code_install/lib -lt8 -lp4est -lsc -o t8cdfmark