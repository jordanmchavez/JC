#pragma once

#include "JC/Common_Mem.h"
#include "JC/Common_Res.h"
#include "JC/Common_Span.h"
#include "JC/Common_Std.h"

namespace JC::FS {

//--------------------------------------------------------------------------------------------------

DefHandle(File);

void          Init(Mem::Mem permMem, Mem::Mem tempMem);
Res<File>     Open(Str path);
Res<U64>      Len(File file);
Res<>         Read(File file, void* out, U64 outLen);
Res<Span<U8>> ReadAll(Mem::Mem mem, Str path);
void          Close(File file);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::FS