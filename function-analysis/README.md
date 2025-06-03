Using 18 because 19 caused a version mismatch
```
export LLVM_HOME=/usr/lib/llvm-18
export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
export Clang_DIR=/usr/lib/llvm-18/lib/cmake/clang


rm -rf build
mkdir build
cd build
cmake ..
make VERBOSE=1
./first_pass /home/maxim/CLionProjects/dReal-clang-tidy/dReal-CMake/src/dreal/dreal_main.cc
```

## TODO: 

- Make the first_pass function actually write fully-known functions along with their pre- and post-rounding states
  - make matchers for double math, ibex calls, and fesetround calls
  - make function, and have it iterate through matches in order of lineLocation for the advanced logic
- Fix this error:
```
  /home/maxim/CLionProjects/dReal-clang-tidy/dReal-CMake/_deps/ibex-src/interval_lib_wrapper/gaol/ibex_IntervalLibWrapper.h:4:10: fatal error: 'gaol/gaol.h' file not found
  4 | #include "gaol/gaol.h"
```
- Make a main python file like run-clang-tidy to find the first-pass bin and then call that file from dReal-CMake/src/dreal/

