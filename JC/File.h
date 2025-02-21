#pragma once

#include "JC/Core.h"

namespace JC::File {

//--------------------------------------------------------------------------------------------------

struct File { u64 handle = 0; };

Res<File>     Open(Str path);
Res<u64>      Len(File file);
Res<>         Read(File file, void* out, u64 outLen);
Res<Span<u8>> ReadAll(Mem::Allocator* allocator, Str path);
void          Close(File file);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::File