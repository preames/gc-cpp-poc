set -e
set -x

PATCHED=~/llvm-dev/llvm-project/llvm/clang

$PATCHED gc-cpp.cpp -O3 -c
clang++ gc-lib.cpp -O3 -c -fPIC -I ~/llvm-dev/llvm-project/llvm/include/ -I ~/llvm-dev/build/include/
# Use clang to link to avoid needing to know library locations
clang++ -o a.out gc-cpp.o gc-lib.o -lunwind -v -lLLVM
