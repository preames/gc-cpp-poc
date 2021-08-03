
#include "gc-lib.h"

#include <cstdlib>

#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include <cxxabi.h>

#include <stdio.h>
#include <stdlib.h>

// Directly copied from https://github.com/cslarsen/libunwind-examples/blob/master/backtrace.cpp, license is public domain
void backtrace()
{
  unw_cursor_t cursor;
  unw_context_t context;

  unw_getcontext(&context);
  unw_init_local(&cursor, &context);

  int n=0;
  while ( unw_step(&cursor) ) {
    unw_word_t ip, sp, off;

    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);

    char symbol[256] = {"<unknown>"};
    char *name = symbol;

    if ( !unw_get_proc_name(&cursor, symbol, sizeof(symbol), &off) ) {
      int status;
#if 0
      if ( (name = abi::__cxa_demangle(symbol, NULL, NULL, &status)) == 0 )
        name = symbol;
#endif
    }

    printf("#%-2d 0x%016" PRIxPTR " sp=0x%016" PRIxPTR " %s + 0x%" PRIxPTR "\n",
        ++n,
        static_cast<uintptr_t>(ip),
        static_cast<uintptr_t>(sp),
        name,
        static_cast<uintptr_t>(off));

    if ( name != symbol )
      free(name);
  }
}

// Need hacked clang to ensure this symbol is public.
extern "C" const char __LLVM_StackMaps;

#include "llvm/Object/StackMapParser.h"

// Avoid need to link against libLLVM (e.g. pretend previous include
// is a header-only library.)
namespace llvm {
  int EnableABIBreakingChecks = 0;
}

// Print out the whole stackmap
void print_stackmap() {
  using namespace llvm;
  ArrayRef<uint8_t> A((uint8_t*)&__LLVM_StackMaps, 4096);
  StackMapParser<support::native> smp(A);
  auto RecI = smp.records_begin();
  for (auto &F : smp.functions()) {
    printf("faddr = 0x%016" PRIxPTR ", recs=%lu\n",
           F.getFunctionAddress(), F.getRecordCount());
    for (int i = 0; i < F.getRecordCount(); i++) {
      auto &R = *RecI;
      printf("  {id=%lu, off=%u, locs=%u, pc=0x%016" PRIxPTR "}\n", R.getID(),
             R.getInstructionOffset(), R.getNumLocations(),
             (uint64_t)(F.getFunctionAddress() + R.getInstructionOffset()));
      RecI++;
    }
  }
  assert(RecI == smp.records_end());
}


void scan_stack_roots()
{
  unw_cursor_t cursor;
  unw_context_t context;

  unw_getcontext(&context);
  unw_init_local(&cursor, &context);

  int n=0;
  while ( unw_step(&cursor) ) {
    unw_word_t ip, sp;

    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);

    printf("#%-2d 0x%016" PRIxPTR " sp=0x%016" PRIxPTR "\n",
        ++n,
        static_cast<uintptr_t>(ip),
        static_cast<uintptr_t>(sp));

    using namespace llvm;
    ArrayRef<uint8_t> A((uint8_t*)&__LLVM_StackMaps, 4096);
    StackMapParser<support::native> smp(A);
    auto RecI = smp.records_begin();
    for (auto &F : smp.functions()) {
      for (int i = 0; i < F.getRecordCount(); i++) {
        auto &R = *RecI;
        const auto pc =
          (uint64_t)(F.getFunctionAddress() + R.getInstructionOffset());
        if (pc == ip) {
          printf("stackmap record found\n");
          printf("faddr = 0x%016" PRIxPTR ", recs=%lu\n",
                 F.getFunctionAddress(), F.getRecordCount());
          printf("  {id=%lu, off=%u, locs=%u, pc=0x%016" PRIxPTR "}\n",
                 R.getID(),
                 R.getInstructionOffset(), R.getNumLocations(),
                 pc);
          // This demonstrates how to find our roots, a real collector
          // would need to do something with each one.  (e.g. add to
          // mark queue, relocate, etc..)
        }
        RecI++;
      }
    }
    assert(RecI == smp.records_end());

  }
}


void force_stackmap() {
  scan_stack_roots();
  // Useful debugging aids for more detail...
  //backtrace();
  //print_stackmap();
  
  // crash on first call since we aren't really interested in the result
  asm("int3");
}

void GC_PTR* gc_malloc(int N) {
  // This is an addrspacecast and needs to *not* be compiled
  // with our modified clang which unconditionally marks functions
  // as gc enabled.  RS4GC fails on addrspacecasts
  return (void GC_PTR*)malloc(N);
}
