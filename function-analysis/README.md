# Overview

These are the most important datapoints regarding functions:

- **Pre-Requirement:** Rounding mode that this function requires
- **Incoming-Rounding-Mode:** List of observed incoming round modes
- **Output-Rounding-Mode:** Self-explanatory

## 1. First Pass

This functions has 2 different targets:

### a. Primary Nodes _(Functions with Double Math and/or Ibex Calls)_


These functions/nodes will then impact the pre-requirement and output-rounding-modes for all functions that call this one.

**Logic:**

First pass will log these into a file called _PrimaryFunctions.json_ which stores the pre-requirement
and output-rounding-modes of these functions
and reports if there are any contradictions. Logic on how it calculates pre and output data is in the file itself.

### b. All nodes


Create _functionsFull.json_ and _functionsParentList.json_ 

**Logic:**

First pass will log these into a file called _functions.json_ which stores each function along with an itemized list
of all functions calls _(duplicates included)_ per function, and logs the unordered unique list of functions calls per
function into _functionsParentList.json_.




## 2. Graph-Solver  

**Parents in this context**: functions that are called inside the given function. They are parents
because they have to be solved in order for the child node/function to be evaluated.

--- 
Internal Datastructure: **FuncGraph**

Each node represents a function.
- Node Properties:
  - **Solved**(bool): All parents solved and then this node was solved. Pre and post conditions are final
  - **Parents** ([strings]): list of parent function names
  - **Children** ([strings]): list of children function names
  - **Error**(str): A contradiction was found if str is not "No"
  - **Pre-Requirement:** Rounding mode that this node requires
  - **Incoming-Rounding-Mode:** List of observed incoming round modes
  - **Output-Rounding-Mode:** Self-explanatory  


Internal Datastructure: **FuncDictionary**

Dictionary of all nodes in FuncGraph by function name

Internal Datastructure: **FuncQueue**

A queue of functions that have all their parents solved

---
Helper function: **Graph-Creator**  

**Logic:**  

Traverses _PrimaryFunctions.json_ first to make a set _initialSolvedFunctions_.  
Then, traverses _functionsParentList.json_ and:
- **Creates a node** for each function and add it to the graph
- **Adds node to parent's child list** via FuncDictionary
- **Add node pointer to FuncQueue** if node doesn't have parents(no parents in _functionsParentList_) or is in _initialSolvedFunctions_


---

**Main function:**

Iterate through FuncQueue and apply logic

**Logic:**

TBD.  
Change node to Solved when done and add its children to FuncQueue if the child's other parents are solved

---
## 3. Running Locally
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

If you encounter this error just ask Kunal:
```
  /home/maxim/CLionProjects/dReal-clang-tidy/dReal-CMake/_deps/ibex-src/interval_lib_wrapper/gaol/ibex_IntervalLibWrapper.h:4:10: fatal error: 'gaol/gaol.h' file not found
  4 | #include "gaol/gaol.h"
```

## 4. TODO: 
- Finish updating all logic
  - figure how and when to log the incoming-rounding-mode of functions
  - finish Graph-Solver main logic
  - Update First Pass based on new plan
- Implement logic
- Double/float math matcher still have to be verified
- Make a python file or bash script that runs everything in the right order
