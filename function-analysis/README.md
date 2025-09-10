# Overview

These are the most important datapoints regarding functions:

- **Pre-Requirement:** Rounding mode that this function requires
- **Incoming-Rounding-Mode:** List of observed incoming round modes
- **Output-Rounding-Mode:** Self-explanatory

## 1. First Pass (Static Analysis)
   The first pass performs static code analysis using Clang AST matchers to identify and categorize functions. 
   
### a. Primary Nodes (Functions with Double Math and/or Ibex Calls)
   These are functions that directly perform floating-point operations or call Ibex interval arithmetic functions. They form the foundation of the analysis since their rounding mode requirements are deterministic. 
   
**Output:** `PrimaryFunctions.json`

- Contains pre-requirement and output-rounding-mode for each primary function
- Reports any internal contradictions (e.g., ibex calls with incorrect rounding modes)
- Uses line-by-line analysis to detect conflicts within function bodies

**Detection Logic:**

* Ibex Functions: Require "Upper" rounding mode
* Double Math: Requires "Nearest" rounding mode
* Rounding Mode Setters: fesetround() calls that change the current mode
* Contradictions: When incompatible operations occur in sequence

### b. All Nodes (Complete Function Tracking)
Captures the complete function call graph for dependency analysis.

**Outputs:**

* `functionsFull.json:` Complete list of function calls with line numbers (duplicates included)
* `functionsParentList.json`: Unique list of parent functions for each function

Purpose: Provides the complete call graph needed for topological sorting and dependency resolution.
### 2. Graph-Solver (Dependency Analysis)

The graph solver propagates rounding mode requirements through the function call graph using topological sorting.

#### Internal Data Structures

**FuncGraph** (`std::map<std::string, FuncNode>`)
- Dictionary of all function nodes indexed by function name
- Each node contains complete dependency and requirement information

**FuncNode Properties**:
- `solved`: Boolean indicating if analysis is complete
- `parents`: Functions called by this function (dependencies)
- `children`: Functions that call this function
- `orderedCalls`: Chronologically ordered function calls with line numbers
- `error`: Detailed error message if contradictions found
- `preRequirement`: Required input rounding mode
- `incomingRoundingMode`: Actual rounding modes from parent functions
- `outputRoundingMode`: Final rounding mode after execution

**FuncQueue** (`std::queue<std::string>`)
- Queue of functions ready for analysis (all dependencies resolved)

#### Analysis Algorithm

**Initialization**:
1. Load primary functions from `PrimaryFunctions.json` as initially solved
2. Load complete call sequences from `functionsFull.json`
3. Build dependency graph from `functionsParentList.json`
4. Initialize queue with leaf functions and primary functions

**Sequential Analysis**:
1. Process functions in dependency order using topological sorting
2. For each function, simulate execution through its ordered call sequence
3. Track rounding mode changes step-by-step
4. Detect contradictions when function calls require incompatible modes

**Contradiction Detection**:
- **Call Sequence Analysis**: Simulates actual execution order
- **Mode Compatibility**: Ensures each function call receives correct rounding mode
- **Line-Level Precision**: Reports exact source locations of conflicts
- **Propagation Tracking**: Identifies how errors cascade through call chains

**Output**: `GraphSolverResults.json`
- Complete analysis results for all functions
- Detailed error reporting with line numbers
- Dependency relationships for debugging
- Final rounding mode requirements for each function

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
- Test new versions
- Deciding to add incomingRoundingMode tracking, since it's unnecessary for current logic
- Test that clangtidy actually catches inline
- Double/float math matcher still have to be verified
- Make a python file or bash script that runs everything in the right order
