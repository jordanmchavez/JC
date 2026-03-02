#pragma once

#include "JC/Common.h"

namespace JC::File {

//--------------------------------------------------------------------------------------------------

DefHandle(File);

void            Init(Mem tempMem);
Res<File>       Open(Str path);
void            Close(File file);
Res<U64>        Len(File file);
Res<>           Read(File file, void* out, U64 outLen);
Res<Span<U8>>   ReadAll(Mem mem, Str path);
Res<Span<char>> ReadAllZ(Mem mem, Str path);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::File