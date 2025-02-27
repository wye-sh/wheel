#! /bin/bash

# Default to running tests but not benchmarks
benchmark_flag="~[benchmark]"

# Check for --benchmark flag
if [ "$1" == "--benchmark" ]; then
    benchmark_flag="[benchmark]"
fi

cmake -D CMAKE_C_COMPILER=clang     \
      -D CMAKE_CXX_COMPILER=clang++ \
      -G "Ninja"                    \
      -B build                      \
 && cd build                        \
 && ninja -k 1                      \
 && cd ..                           \
 && ./bin/test.exe $benchmark_flag
