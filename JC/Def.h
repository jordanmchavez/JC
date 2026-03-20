#pragma once

#include "JC/Common.h"

namespace JC::Json { struct Traits; }

namespace JC::Def {

//--------------------------------------------------------------------------------------------------

DefHandle(Reg);

void        Init(Mem permMem, Mem tempMem);
Res<>       Load(Str path);
void        Register(Reg reg, Str name, Json::Traits const* jsonTraits, U32 maxLen);
void const* GetData(Reg reg);
U32         GetLen(Reg reg);

template <class T>
struct RegId {
	static inline char Val;
};

template<class T> Reg RegisterArray(Str name, U32 maxLen) {
	Reg reg = { .handle = &RegId<T>::Val };
	RegisterJsonTraits(reg, name, GetJsonTraits(T(), maxLen);
	return reg;
}

template<class T> Reg RegisterObject(Str name) {
	Reg reg = { .handle = &RegId<T>::Val };
	Register(reg, name, GetJsonTraits(T(), 1);
	return reg;
}

template <class T> T const* GetObject(Reg reg) {
	return return (T const*)GetData(reg);
}

template <class T> Span<T const> GetArray(Reg reg) {
	return return Span<T const>((T const*)GetData(reg), GetLen(reg));
}


//--------------------------------------------------------------------------------------------------

}	// namespace JC::Def