#!/bin/bash
# get source dir
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
echo $SCRIPT_DIR
find $SCRIPT_DIR -name "*.cpp" | xargs -n 1 -P 40 ccache mpicxx -Wall -std=c++17 -g -O3 -march=cascadelake -Ipath/to/esdmnc_install/include -Ipath/to/t8code_install/include -c && mpicxx *.o -Lpath/to/t8code_install/lib -Lpath/to/esdmnc_install/lib -Wl,-rpath-link,path/to/esdm_install/lib -lt8 -lp4est -lsc -lnetcdf -o t8cdfmark
