#pragma once

#include "JC/Common.h"

namespace JC::FS {

//--------------------------------------------------------------------------------------------------

DefHandle(File);

void          Init(Mem permMem, Mem tempMem);
Res<File>     Open(Str path);
Res<U64>      Len(File file);
Res<>         Read(File file, void* out, U64 outLen);
Res<Span<U8>> ReadAll(Mem mem, Str path);
void          Close(File file);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::FS