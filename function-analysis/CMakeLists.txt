cmake_minimum_required(VERSION 3.10)
project(FirstPassTool)

# Require C++17 for Clang/LLVM
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

# Your source file(s)
add_executable(first_pass FirstPass.cpp)
add_executable(graph_solver GraphSolver.cpp)

target_include_directories(first_pass
        PRIVATE
        ${LLVM_INCLUDE_DIRS}
        ${CLANG_INCLUDE_DIRS}
)

# Set default visibility for symbols (recommended for LLVM/Clang tools)
if (LLVM_ENABLE_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti")
endif()

# Link against Clang and LLVM libraries
target_link_libraries(first_pass
        PRIVATE
        clangTooling
        clangBasic
        clangASTMatchers
)

message(STATUS "LLVM include dirs: ${LLVM_INCLUDE_DIRS}")
message(STATUS "Clang include dirs: ${CLANG_INCLUDE_DIRS}")





