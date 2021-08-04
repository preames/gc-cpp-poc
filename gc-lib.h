/// A very, very minimal library for mocking garbage collected memory
/// from a programs perspective w/o actually doing any GC.
#ifndef _GC_LIB_H_
#define  _GC_LIB_H_

/// Mark a pointer as garbage collected.  This assumes the builtin
/// "statepoint-example" GC from LLVM which uses address space 1 to
/// represent GC references.  Note that this address space must also
/// be marked non-integral and functions which use them need to have
/// the GC set (handled in the clang source patch.)
#define GC_PTR __attribute__((address_space(1)))

/// Return a block of memory in the garbage collected heap.
void GC_PTR * gc_malloc(int N);

/// An external call to force the existance of a stackmap at some
/// point in garbage collected code.  In GC terminology, this would
/// be a possible safepoint.
void force_stackmap();

#endif // _GC_LIB_H_
