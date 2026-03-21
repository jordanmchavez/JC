#pragma once

#include "JC/Common.h"

namespace JC::Def {

//--------------------------------------------------------------------------------------------------

using JsonFn = Res<> (Str json);

struct RegDesc {
	Str     jsonMemberName;
	JsonFn* jsonFn;
};


void  RegisterModule(Str name, Span<RegDesc> regDescs);
Res<> Load(Str path);
void  Apply(Str module);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Def