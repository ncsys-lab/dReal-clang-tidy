cmake_minimum_required(VERSION 3.10)
project(FirstPassTool)

# Require C++17 for Clang/LLVM
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(LLVM_HOME $ENV{LLVM_HOME} CACHE PATH "Root of LLVM installation")
set(LLVM_INC ${LLVM_HOME}/include)
set(LLVM_LIB ${LLVM_HOME}/lib)
set(LLVM_BIN ${LLVM_HOME}/bin)
set(LLVM_CMAKE ${LLVM_LIB}/cmake/llvm)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")


# Include clang builtins they are added in compile_commands.json and thus included by our tool
include_directories(${LLVM_LIB}/clang/21/include)
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

# Your source file(s)
add_executable(first_pass FirstPass.cpp)

llvm_map_components_to_libnames(LLVM_LIBS
        Support
        Core
        Option
        # Add other components as needed
)


# Link against Clang and LLVM libraries
target_link_libraries(first_pass
        ${LLVM_LIBS}
        clangTooling
        clangBasic
        clangAST
        clangASTMatchers
        clangFrontend
        clangFrontendTool
        clangSerialization
        clangToolingCore
)

message(STATUS "LLVM_LIBS: ${LLVM_LIBS}")






