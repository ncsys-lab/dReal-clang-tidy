# Information

This repository is meant to:

- Catch problematic lines in Dreal that perform distinct operations (ibex calls and float math) requiring two different rounding modes 
 via custom clang-tidy checks

- Analyze all functions within dReal-CMake/src to make sure that the proper rounding mode is set before and after 
function calls, and return a breakdown of those functions in `GraphSolverResults.json` and all functions in
`GraphSolverResultsDump.json`


## 1. Running locally

Clone [dReal-CMake](https://github.com/ncsys-lab/dReal-CMake) into the root of this project  

Run this script from the project root to execute the custom checks on dReal-CMake
```bash
# Build the llvm binaries
cd llvm-project
cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" -DCLANG_TIDY_ENABLE_STATIC_ANALYZER=OFF
ninja -C build clang-tidy
# And then compile the dReal-Cmake codebase again (from its root)  
cd ..
cd dReal-CMake
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build ./ -j16 
cd ..
ln -s build/compile_commands.json compile_commands.json
cd ..
# Now add the binary to path so that clang-tidy is attached as well
export PATH=$PATH:./llvm-project/build/bin
source ~/.bashrc
clang-tidy --version
# Now run the clang tidy checks (using a .clang-tidy folder in the root dir)
cd dReal-CMake
python3 ../llvm-project/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py -header-filter='.*' -config-file=../.clang-tidy
cd ..
```
Once you have looked at that feedback and cleaned up the hard-to-parse lines, you can then run the static analysis tool 
on the freshly-cleaned codebase with this script (from the project root).

```bash
PROJECT_DIR=$(pwd)  
cd function-analysis 
rm -rf build
mkdir build
cd build
cmake ..
make VERBOSE=1
./first_pass $PROJECT_DIR/dReal-CMake/src/dreal/dreal_main.cc
./graph_solver
```
## 2. Developing new checks

From llvm-project
``` 
cd clang-tools-extra/
clang-tidy/add_new_check.py readability prevent-using-ibex
```
First term is the module it will be part of (easy if it's an existing one)  
Second term is the name of the new check

## 3. Debugging

If you encounter this error just ask Kunal:
```
  /home/maxim/CLionProjects/dReal-clang-tidy/dReal-CMake/_deps/ibex-src/interval_lib_wrapper/gaol/ibex_IntervalLibWrapper.h:4:10: fatal error: 'gaol/gaol.h' file not found
  4 | #include "gaol/gaol.h"
```

Any other questions can be directed towards me, but they're just debuggable problems in which you're using the wrong
version of something or it can't find the right path.

```bash
export LLVM_HOME=/usr/lib/llvm-18
export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
export Clang_DIR=/usr/lib/llvm-18/lib/cmake/clang
```

These commands are useful if it isn't compiling because you have multiple llvm versions. However,
I just deleted llvm-19 because it was causing too many problems

## 4. More Information

More information on how my static analysis tool works and the logic behind it can be found in
`./function-analysis`