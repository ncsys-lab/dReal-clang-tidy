These commands are useful if it isnt compiling because you have multiple llvm versions. However I still had to end 
up deleting llvm-19 because cmake just hated me
```
export LLVM_HOME=/usr/lib/llvm-18
export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
export Clang_DIR=/usr/lib/llvm-18/lib/cmake/clang
```

Actual build script, which you run from this folder

```
rm -rf build
mkdir build
cd build
cmake ..
make VERBOSE=1
./first_pass /home/maxim/CLionProjects/dReal-clang-tidy/dReal-CMake/src/dreal/dreal_main.cc
```

## TODO: 
- Write the block analyzer logic on paper next 

- The setround matchers don't seem to match with the round_upward funcs, and those seem to be the only call to setround. It's
probably due to the gaol errors.
- Double/float math matcher still have to be verified
- Fix this error:
```
  /home/maxim/CLionProjects/dReal-clang-tidy/dReal-CMake/_deps/ibex-src/interval_lib_wrapper/gaol/ibex_IntervalLibWrapper.h:4:10: fatal error: 'gaol/gaol.h' file not found
  4 | #include "gaol/gaol.h"
```
- Make a main python file like run-clang-tidy to find the first-pass bin and then call that file from dReal-CMake/src/dreal/

