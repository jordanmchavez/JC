#pragma once

#include "JC/Common.h"

namespace JC::Json { struct Traits; }

namespace JC::Def {

//--------------------------------------------------------------------------------------------------

template<class T> void RegisterObject(Str name) { RegisterJsonTraits(name, GetJsonTraits(T(), 1); }
template<class T> void RegisterArray(Str name, U32 maxLen) { RegisterJsonTraits(name, GetJsonTraits(T(), m1xLen); }

void RegisterJsonTraits(Str name, Json::Traits const* jsonTraits, U32 maxLen);

template <class T> T const* GetObject(Str name);
template <class T> Span<T> GetArray(Str name);

Res<> Load(Str path);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Def