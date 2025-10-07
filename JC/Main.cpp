#include "JC/Common_Mem.h"
#include "JC/Event.h"
#include "JC/Log.h"
#include "JC/Sys.h"
#include "JC/Unit.h"
#include "JC/Window.h"

using namespace JC;

Res<> Run() {
	Mem::Mem* const frameMem = Mem::Create(16 * GB);

	Log::AddFn([](Log::Msg const* msg) {
		Sys::Print(Str(msg->line, msg->lineLen));
		if (Sys::DbgPresent()) {
			Sys::DbgPrint(msg->line);
		}
	});

	const Window::InitDesc windowInitDesc = {
		.tempMem    = frameMem,
		.title      = "JC Test",
		.style      = Window::Style::BorderedResizable,
		.width      = 1024,
		.height     = 768,
		.displayIdx = 1,
	};
	Try(Window::Init(&windowInitDesc));

	U64 frame = 0;
	bool exitRequested = false;
	while (!exitRequested) {
		frame++;

		Mem::Reset(frameMem);

		while (Event::Event const* const event = Event::Get()) {
			switch (event->type) {
				case Event::Type::ExitRequest:
					exitRequested = true;
					break;
			}
		}

		Err::Frame(frame);
		Window::Frame();
	}

	Window::Shutdown();
	Mem::Destroy(frameMem);

	return Ok();
}

int main(int argc, const char** argv) {
	if (argc == 2 && argv[1] == Str("test")) {
		Unit::Run();
		return 0;
	}

	if (Res<> r = Run(); !r) {
		r;
	}

	return 0;
}