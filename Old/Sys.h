#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Mutex { u64 handle; };

struct ThreadApi {
	static ThreadApi* Init();

	virtual Mutex CreateMutex() = 0;
	virtual void  LockMutex(Mutex m) = 0;
	virtual void  UnlockMutex(Mutex m) = 0;
	virtual void  DestroyMutex(Mutex m) = 0;
};

//--------------------------------------------------------------------------------------------------

struct Date {
	u32 year;
	u32 month;
	u32 day;
	u32 hour;
	u32 minute;
	u32 second;
	u32 millisecond;
};

struct TimeApi {
	static TimeApi* Init();

	virtual Time   Now() = 0;
	virtual Time   Add(Time t, double seconds) = 0;
	virtual double Since(Time end, Time start) = 0;
	virtual Date   TimeToDate(Time t) = 0;
};

//--------------------------------------------------------------------------------------------------

//struct File { u64 opaque = 0; };
//
//struct File_Attrs {
//	u64  len;
//	bool dir;
//	Time created;
//	Time modified;
//};
//
//Res<File>       File_Open(s8 path);
//Res<File_Attrs> File_GetAttrs(File f);
//Res<u64>        File_Read(File f, void* buf, u64 len);
//void            File_Close(File f);

//--------------------------------------------------------------------------------------------------

}	// namespace JC