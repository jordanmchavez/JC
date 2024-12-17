#pragma once

#include "JC/Common.h"

namespace JC::File {

//--------------------------------------------------------------------------------------------------

struct File { u64 handle = 0; };

struct Api {
	virtual void          Init(Arena* temp) = 0;
	virtual Res<File>     Open(s8 path) = 0;
	virtual Res<u64>      Len(File file) = 0;
	virtual Res<>         Read(File file, void* out, u64 outLen) = 0;
	virtual Res<Span<u8>> ReadAll(s8 path);
	virtual void          Close(File file) = 0;
};

Api* GetApi();

//--------------------------------------------------------------------------------------------------

}	// namespace JC"::File