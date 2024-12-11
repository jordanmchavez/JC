#pragma once

#include "JC/Common.h"

namespace JC {

struct Mem;
struct TempMem;

//--------------------------------------------------------------------------------------------------

struct File { u64 handle = 0; };

struct FileApi {
	virtual void      Init(TempMem* tempMem) = 0;
	virtual Res<File> Open(s8 path) = 0;
	virtual Res<u64>  Len(File file) = 0;
	virtual Res<>     Read(File file, void* out, u64 outLen) = 0;
	virtual void      Close(File file) = 0;

	Res<Span<u8>>     ReadAll(Mem* mem, s8 path);
};

FileApi* GetFileApi();

//--------------------------------------------------------------------------------------------------

}	// namespace JC