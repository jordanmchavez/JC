#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/FS.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Random.h"
#include "JC/Render.h"
#include "JC/Unicode.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"
#include <stdio.h>
#include "stb/stb_image.h"
#include "JC/MinimalWindows.h"

using namespace JC;

struct TestApp : App {
	Arena*           temp            = 0;
	Arena*           perm            = 0;
	Log*             log             = 0;
	u32              windowWidth     = 0;
	u32              windowHeight    = 0;

	Res<> Init(Arena* tempIn, Arena* permIn, Log* logIn, const Window::State* windowState) override {
		temp = tempIn;
		perm = permIn;
		log = logIn;
		windowWidth  = windowState->width;
		windowHeight = windowState->height;
		return Ok();
	}

	void Shutdown() override {}

	Res<> Events(Span<Event::Event> events) override {
		for (const Event::Event* e = events.Begin(); e != events.End(); e++) {
			switch (e->type) {
				case Event::Type::Exit:
					Exit();
					break;
				case Event::Type::Key:
					if (e->key.key == Event::Key::Escape) {
						Exit();
					}
					break;
			}
		}
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

//--------------------------------------------------------------------------------------------------

int main(int argc, const char** argv) {
	TestApp testApp;
	RunApp(&testApp, argc, argv);
	return 0;
}