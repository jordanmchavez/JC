#include "JC/App.h"

namespace JC {

struct Game : App {
	Res<> Init(Mem::Allocator* allocator, Mem::TempAllocator* tempAllocator, Log::Logger* logger, const Window::State* windowState) override {
		allocator;tempAllocator;logger;windowState;
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