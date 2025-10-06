#pragma once

#include "JC/Common_SrcLoc.h"
#include "JC/Common_Std.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

struct Mem;

Mem*  Create(U64 reserve);
void  Destroy(Mem* mem);
void* Alloc(Mem* mem, U64 size, SrcLoc sl = SrcLoc::Here());
bool  Extend(Mem* mem, void* p, U64 size, SrcLoc sl = SrcLoc::Here());
void  Reset(Mem* mem);

template <class T> T* AllocT(Mem* mem, U64 n, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(mem, n * sizeof(T), sl); }
template <class T> bool ExtendT(Mem* mem, T* p, U64 n, SrcLoc sl = SrcLoc::Here()) { return Extend(mem, p, n * sizeof(T), sl); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem