#include "JC/App.h"

namespace JC {

struct Game : App {
	Mem::Allocator*     allocator     = 0;
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;
	Res<> Init(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn, const Window::State* windowState) override {
		allocator     = allocatorIn;
		tempAllocator = tempAllocatorIn;
		logger        = loggerIn;
		windowState;
		return Ok();
	}

	void Shutdown() override {
	}

	Res<> Events(Span<Event::Event> events) override {
		events;
		return Ok();
	}

	Res<> Update(double secs) override {
		secs;
		return Ok();
	}

	Res<> Draw() override {
		return Ok();
	}
};

static Game game;

App* GetApp() { return &game; }

}	// namespace JC