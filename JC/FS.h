#pragma once

#include "JC/Core.h"

namespace JC::FS {

//--------------------------------------------------------------------------------------------------

struct File { U64 handle = 0; };

Res<File>     Open(Str path);
Res<U64>      Len(File file);
Res<>         Read(File file, void* out, U64 outLen);
Res<Span<U8>> ReadAll(Allocator* allocator, Str path);
void          Close(File file);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::FS