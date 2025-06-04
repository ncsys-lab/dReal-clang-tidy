These commands are useful if it isnt compiling because you have multiple llvm versions. However I still had to end 
up deleting llvm-19 because cmake just hated me
```
export LLVM_HOME=/usr/lib/llvm-18
export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
export Clang_DIR=/usr/lib/llvm-18/lib/cmake/clang
```


```
rm -rf build
mkdir build
cd build
cmake ..
make VERBOSE=1
./first_pass /home/maxim/CLionProjects/dReal-clang-tidy/dReal-CMake/src/dreal/dreal_main.cc
```

## TODO: 

- Verify that current matchers actually catch (dont catch) all set rounds and ibex calls
- Verify that FirstPass actually iterates through all files in dReal 
- Then, add the logic so that it writes required pre and post conditions for every function
- Fix this error:
```
  /home/maxim/CLionProjects/dReal-clang-tidy/dReal-CMake/_deps/ibex-src/interval_lib_wrapper/gaol/ibex_IntervalLibWrapper.h:4:10: fatal error: 'gaol/gaol.h' file not found
  4 | #include "gaol/gaol.h"
```
- Make a main python file like run-clang-tidy to find the first-pass bin and then call that file from dReal-CMake/src/dreal/

