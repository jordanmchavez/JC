#pragma once

#include "JC/Common.h"

namespace JC::Json { struct Traits; }

namespace JC::Def {

//--------------------------------------------------------------------------------------------------

DefHandle(Reg);

void       Init(Mem permMem, Mem tempMem);
Reg        Register(Str name, Json::Traits const* jsonTraits, U64 maxLen);
Res<>      Load(Str path);
Span<void> Get(Str name);

template<class T> void RegisterArray(Str name, U64 maxLen) { RegisterJsonTraits(name, GetJsonTraits(T(), maxLen); }
template<class T> void RegisterObject(Str name) { Register(reg, name, GetJsonTraits(T(), 1); }

template <class T> T const* GetObject(Str name) {
	Span<void> span = Get(name);
	Assert(span.len == 1);
	return (T const*) span.data;
}

template <class T> Span<T const> GetArray(Str name) {
	Span<void> span = Get(name);
	return Span<T>((T const*)span.data, span.len);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Def