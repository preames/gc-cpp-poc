Stackmaps for a hypothetic garbage collected C++
------------------------------------------------

This is a proof of concept for how one might leverage the infrastructure LLVM provides for GC stackmap generation in an effort to implement a garbage collected version of C++.  This is *not* a complete implementation of garbage collected C++ - for one thing it implements neither heapmaps nor an actual collector!

I wrote this up in a couple hours after another round of a recorruing conversation which goes something like this: Q: "Can we use the LLVM statepoint infrastructure to do a GCed version of C++?"  A: "Yes, most of the pieces are there, but they need glued together and exposed cleanly."  This POC demonstrates the "glued together" part, not the "exposed cleanly" part!

Contents Overview
=================

Let's briefly overview the contents of this repo.

In the gc-cpp.cpp file, this an example of what garbage collected C++ code might look like.  It needs to be compiled with a patched clang (more details below), and imposes some non-trivial language restrictions:

* All pointers passed to functions or returned from functions must point to the base of the allocation.  Handling derived pointers at function boundaries is generally a hard problem.
* No GC pointers stored in global variables or the heap (either GC or non).  The example code violates this rule, and as noted, would be incorrect if a GC actually ran right now.  Implementing a basic form of heapmap for GC'd objects wouldn't be hard, but is left as an exercise for the reader.

The gc-lib.cpp file is a *very* minimal GC runtime which must be compiled with an unmodified clang.  Key pieces are:

* Demonstration of stackmap parsing, and stack traversal suitable for root scanning.  This uses libunwind and LLVM's StackMapParser.  Actual root visitation is left as an exercise for the reader, but should follow naturally from the tracing code.
* A very basic "gc allocator".  It simply wraps malloc and casts to the GC_PTR type.  The key detail here is that this cast can't be visible to the code in gc-cpp.cpp without comprimising correctness.

Build, Setup, and Output
========================

Needs libunwind installed.  Needs LLVM *headers* local, but currently does *not* need to link against LLVM.  You do need to have run enough of an llvm-build to have the generated config header though.

$ ./build.sh && a.out
#1  0x0000000000401836 sp=0x00007ffcfdf1a160
#2  0x000000000040125a sp=0x00007ffcfdf1a170
stackmap record found
faddr = 0x0000000000401250, recs=1
  {id=2882400000, off=10, locs=5, pc=0x000000000040125a}
#3  0x0000000000401295 sp=0x00007ffcfdf1a180
stackmap record found
faddr = 0x0000000000401270, recs=3
  {id=2882400000, off=37, locs=5, pc=0x0000000000401295}
#4  0x00007f41b32a90b3 sp=0x00007ffcfdf1a190
#5  0x000000000040114e sp=0x00007ffcfdf1a260
Trace/breakpoint trap (core dumped)

This is the stack (including stackmaps for the relevant frames) of the first call to force_stackmap().  For context, here's the backtrace with function names:

#1  0x000000000040183b sp=0x00007ffde2934ae0 _Z14force_stackmapv + 0xb
#2  0x000000000040125a sp=0x00007ffde2934af0 _Z3bazPU3AS15GCObj + 0xa
#3  0x0000000000401296 sp=0x00007ffde2934b00 main + 0x26
#4  0x00007f826d71f0b3 sp=0x00007ffde2934b10 __libc_start_main + 0xf3
#5  0x000000000040114e sp=0x00007ffde2934be0 _start + 0x2e

The two frames with stackmaps are bar and main from gc-cpp.cpp.

Implementation
==============

This requires a lightly patched clang/llvm build to compile gc-cpp.cpp.  Each change should be reasonable explained in the diff.  Note that these changes are workarounds, and not suitable for submission upstream.

One major limitation to be aware of.  All the GC enabled C++ code must currently be in a single IR file when codegened.  (The easiest way to achieve this is to put it all in a single C++ source file.)  This is because the llvm_stackmap section's linkage semantics have never been defined.  The current users are all JITs which parse the object file directly, so this problem space has never been explored.

Future Work
===========

If we were going to turn this into something real, we'd need to define a set of language extensions for C++ to selectively enable the changes in my modified clang.  There's a lot of room for design iteration here, but let me layout one possibility.

We could expose a clang::gc function attribute which would look something like:
void foo()  __attribute__((clang::gc("statepoint-example")));

This would map pretty directly to LLVM's setGC on the corresponding Function.  The string name would be a GC plugin; in this case, the builtin example GC for the statepoint functionality.  (For those not familiar, LLVM's notion of a GCStrategy controls *compiler* behavior and is mostly a set of config flags.  LLVM does not provide anything in the way of GC runtime support.)

The other missing piece needed is a way to mark an address space as non-integral.  My current best idea is to have clang mark an AS non-integral if any Function in the Module has a GC which a) uses the abstract machine model, and b) considers that address space a GC pointer.  (None of the plumbing to explose that cleanly exists today.)

So far, we've said nothing about heapmaps or static variable roots.  Quite honestly, I haven't given them much thought from the language level, so I'll avoid saying anything here.



Assorted Notes on Code Quality
==============================

I noticed while looking at the generated code that GC_PTR function arguments are being considered live in the caller over the call and thus included in the stackmap entry for that call.  Without thinking about it too hard, that seems unneeded.

LLVM still defaults to the stack based lowering for stackmaps (e.g. all values are spilled over calls).  There is recent support for register based lowering (e.g. values kept in callee saved registers), but that generally requires much more involved runtime support.  I had no interest in prototyping all of that.  :)
