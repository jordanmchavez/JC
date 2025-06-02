#include "JC/App.h"
#include "JC/Event.h"
#include "JC/Gpu.h"
#include "JC/Log.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct App_3D : App {
	Mem::Allocator*     allocator     = 0;
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;

	//----------------------------------------------------------------------------------------------

	Res<> PreInit(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn) {
		allocator     = allocatorIn;
		tempAllocator = tempAllocatorIn;
		logger        = loggerIn;
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	Res<> Init(const Window::State* windowState) {
		windowState;
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	void Shutdown() {
	}

	//----------------------------------------------------------------------------------------------

	Res<> Events(Span<Event::Event> events) {
		for (U64 i = 0; i < events.len; i++) {
			const Event::Event* const ev = &events[i];
			switch (ev->type) {
				case Event::Type::Exit:
					Exit();
					break;
			}
		}
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	Res<> Update(double secs) {
		secs;
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	Res<> Draw(Gpu::Cmd cmd, Gpu::Image swapchainImage) {
		cmd;swapchainImage;
		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static App_3D app;
App* g_app = &app;

//--------------------------------------------------------------------------------------------------

}	// namespace JC