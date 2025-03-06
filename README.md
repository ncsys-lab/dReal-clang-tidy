## Running locally

Clone this repo into the folder containing dReal.  
Then we need to build the llvm binaries
```
cd llvm-project
cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" -DCLANG_TIDY_ENABLE_STATIC_ANALYZER=OFF
ninja -C build clang-tidy
```

And then compile the dReal-Cmake codebase again (from its root)  
``` make -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .```

Now we just need to add clang-tidy to PATH so that run-clang-tidy.py will use the correct binary.  
You might need to modify these commands a little since I'm using my absolute path but your path should
be relatively similar.  
```export PATH=$PATH:/home/maxim/CLionProjects/dReal-clang-tidy/llvm-project/build/bin```  
Reload the terminal and then double check that it worked:
```
source ~/.bashrc
clang-tidy --version
```

And then the final command (from `/dReal-Cmake/`)   
```python3 ~/CLionProjects/dReal-clang-tidy/llvm-project/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py -checks='-*,readability-prevent-using-ibex,readability-prevent-ibex-float-math-in-same-line'```

For the future I will make a .clang-tidy file so that the clang-tidy will just use the checks enabled by that file, so  
that the runcommand is one term instead of an endless line.

### For developing new checks

From llvm-project
``` 
cd clang-tools-extra/
clang-tidy/add_new_check.py readability prevent-using-ibex
```