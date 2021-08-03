#include "gc-lib.h"

#define NOINLINE __attribute__((noinline))

NOINLINE int foo(int GC_PTR *A)  {
  force_stackmap();
  return *A;
}

NOINLINE int bar(int GC_PTR *A, int GC_PTR *B) {
  force_stackmap();
  return foo(A) + foo(B);
}

struct GCObj {
  // WARNING: This is misleading as we don't yet have heapmap support.
  GCObj GC_PTR * Next = nullptr;
  int GC_PTR *Ptr = nullptr;
  int Data;
};

NOINLINE int baz(GCObj GC_PTR *O)  {
  force_stackmap();
  return *O->Ptr + O->Data;
}

int main() {
  // Workaround: Using an AWS built clang on a local ubuntu machine gets
  // confused about the location of system headers.  As such, just use
  // unsafe casts for our demo.
  void GC_PTR* space = gc_malloc(sizeof(GCObj));
  GCObj GC_PTR* obj = (GCObj GC_PTR*)space;

  void GC_PTR* space2 = gc_malloc(sizeof(int));
  obj->Ptr = (int GC_PTR*) space2;
  
  baz(obj);
  bar(&obj->Data, &obj->Data);
  return 0;
}
