#include "JC/Draw.h"
#include "JC/Event.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/StrDb.h"
#include "JC/Sys.h"
#include "JC/Time.h"
#include "JC/Unit.h"
#include "JC/Window.h"

using namespace JC;

static Mem permMem;
static Mem tempMem;

Res<> Run() {
	permMem = Mem::Create(16 * GB);
	tempMem = Mem::Create(16 * GB);

	Time::Init();

	StrDb::Init();

	Log::Init(permMem, tempMem);
	Log::AddFn([](Log::Msg const* msg) {
		Sys::Print(Str(msg->line, msg->lineLen));
		if (Sys::DbgPresent()) {
			Sys::DbgPrint(msg->line);
		}
	});

	Window::InitDesc const windowInitDesc = {
		.tempMem    = tempMem,
		.title      = "JC Test",
		.style      = Window::Style::BorderedResizable,
		.width      = 1024,
		.height     = 768,
		.displayIdx = 1,
	};
	Try(Window::Init(&windowInitDesc));


	Window::PlatformDesc const windowPlatformDesc = Window::GetPlatformDesc();
	const Gpu::InitDesc gpuInitDesc = {
		.permMem            = permMem,
		.tempMem            = tempMem,
		.windowWidth        = 1024,
		.windowHeight       = 768,
		.windowPlatformDesc = &windowPlatformDesc,
	};
	Try(Gpu::Init(&gpuInitDesc));

	Draw::InitDesc const drawInitDesc = {
		.permMem      = permMem,
		.tempMem      = tempMem,
		.windowWidth  = 1024,
		.windowHeight = 768,
	};
	Try(Draw::Init(&drawInitDesc));

	U64 frame = 0;
	bool exitRequested = false;
	while (!exitRequested) {
		frame++;

		while (Event::Event const* const event = Event::Get()) {
			switch (event->type) {
				case Event::Type::ExitRequest:
					exitRequested = true;
					break;
			}
		}

		Err::Frame(frame);
		Window::Frame();
		Mem::Reset(tempMem, 0);
	}

	return Ok();
}

int main(int argc, char const** argv) {
	if (argc == 2 && argv[1] == Str("test")) { Unit::Run(); return 0; }

	Res<> r = Run();

	Draw::Shutdown();
	Gpu::Shutdown();
	Window::Shutdown();

	if (!r) {
		LogErr(r);
	}
	return r ? 0 : 1;
}