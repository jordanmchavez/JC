#pragma once

#include "JC/Common.h"

namespace JC::FS {

//--------------------------------------------------------------------------------------------------

struct File { u64 handle = 0; };

void          Init();
Res<File>     Open(Str path);
Res<u64>      Len(File file);
Res<>         Read(File file, void* out, u64 outLen);
Res<Span<u8>> ReadAll(Arena* arena, Str path);
void          Close(File file);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::FS