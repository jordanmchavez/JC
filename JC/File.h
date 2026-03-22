#pragma once

#include "JC/Common.h"

namespace JC::File {

//--------------------------------------------------------------------------------------------------

DefHandle(File);

Res<File>      Open(Str path);
void           Close(File file);
Res<U64>       Len(File file);
Res<>          Read(File file, void* out, U64 outLen);
Res<Span<U8>>  ReadAllBytes(Mem mem, Str path);
Res<Str>       ReadAllStr(Mem mem, Str path);
Res<Span<Str>> EnumFiles(Mem mem, Str dir, Str ext);
Str            RemoveExt(Str path);
bool           PathsEq(Str path1, Str path2);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::File