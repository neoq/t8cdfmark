#!/bin/bash
# get source dir
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
echo $SCRIPT_DIR
find $SCRIPT_DIR -name "*.cpp" | xargs -n 1 -P 40 ccache mpicxx -Wall -std=c++17 -g -O3 -Ipath/to/t8code_install/include -c && mpicxx *.o -Lpath/to/t8code_install/lib -lt8 -lp4est -lsc -o t8cdfmark
