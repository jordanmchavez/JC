#include "JC/Sys.h"
#include "JC/MinimalWindows.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct SysApiImpl : SysApi {
	void Abort() override {
		TerminateProcess(GetCurrentProcess(), 3);
	}
};

SysApiImpl sysApiImpl;

SysApi* SysApi::Get() {
	return &sysApiImpl;
}

//--------------------------------------------------------------------------------------------------

struct DbgApiImpl : DbgApi {
	bool IsPresent() override {
		return IsDebuggerPresent();
	}

	void Print(s8 msg) override {
		OutputDebugStringA(msg);
	}

	void Break() override {
	}
};

static DbgApiImpl dbgApiImpl;

DbgApi* DbgApi::Get() {
	return &dbgApiImpl;
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC