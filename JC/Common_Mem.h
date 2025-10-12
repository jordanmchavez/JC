#pragma once

#include "JC/Common_SrcLoc.h"
#include "JC/Common_Std.h"

namespace JC::Mem {

//--------------------------------------------------------------------------------------------------

DefHandle(Mem);

void                       Init();
Mem                        Create(U64 reserveSize);
void                       Destroy(Mem mem);
void*                      Alloc(Mem mem, U64 size, SrcLoc sl = SrcLoc::Here());
bool                       Extend(Mem mem, void* ptr, U64 newSize, SrcLoc sl = SrcLoc::Here());
U64                        Mark(Mem mem);
void                       Reset(Mem mem, U64 mark);

template <class T> T*      AllocT(Mem mem, U64 n, SrcLoc sl = SrcLoc::Here()) { return (T*)Alloc(mem, n * sizeof(T), sl); }
template <class T> Span<T> AllocSpan(Mem mem, U64 n, SrcLoc sl = SrcLoc::Here()) { return Span<T>((T*)Alloc(mem, n * sizeof(T), sl), n); }
template <class T> bool    ExtendT(Mem mem, T* ptr, U64 newN, SrcLoc sl = SrcLoc::Here()) { return Extend(mem, ptr , newN * sizeof(T), sl); }

struct Scope {
	Mem mem;
	U64 mark;
	Scope(Mem mem_) { mem = mem_; mark = Mark(mem); }
	~Scope() { Reset(mem, mark); }
};
//--------------------------------------------------------------------------------------------------

}	// namespace JC::Mem