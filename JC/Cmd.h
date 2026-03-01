#pragma once

#include "JC/Common.h"

namespace JC::Cmd {

//--------------------------------------------------------------------------------------------------

using CmdFn = void (Span<Str> args);

void Init(Mem permMem);
void AddCmd(Str name, CmdFn* cmdFn);
Res<> Exec(Str buf);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Cmd