Using 18 because 19 caused a version mismatch
export LLVM_HOME=/usr/lib/llvm-18
export LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
export Clang_DIR=/usr/lib/llvm-18/lib/cmake/clang


rm -rf build
mkdir build
cd build
cmake ..
make VERBOSE=1
./first_pass
