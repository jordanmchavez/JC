#include "JC/App.h"

namespace JC {

struct Game : App {
	Res<> Init(Arena* perm, Arena* temp, Log* log, const Window::State* windowState) override {
		perm;temp;log;windowState;
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