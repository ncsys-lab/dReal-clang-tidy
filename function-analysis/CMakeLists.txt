cmake_minimum_required(VERSION 3.10)
project(FirstPassTool)

# Require C++17 for Clang/LLVM
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate compile_commands.json for our tool (need to run the tool with -p compile_commands.json)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Include clang builtins they are added in compile_commands.json and thus included by our tool
include_directories(${LLVM_LIB}/clang/21/include)
include_directories(${LLVM_INCLUDE_DIRS})

# Your source file(s)
add_executable(first_pass FirstPass.cpp)

# Link against Clang and LLVM libraries
target_link_libraries(first_pass
        PRIVATE
        LLVMSupport
        clangTooling
        clangBasic
        clangASTMatchers
)

# Enable all LLVM/Clang warnings and RTTI if needed
target_compile_options(first_pass PRIVATE ${LLVM_COMPILE_FLAGS})
set_target_properties(first_pass PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
)


