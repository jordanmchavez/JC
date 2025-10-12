#include "JC/Common_Mem.h"
#include "JC/Event.h"
#include "JC/FS.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Sys.h"
#include "JC/Unit.h"
#include "JC/Window.h"

using namespace JC;

static Mem::Mem permMem;
static Mem::Mem tempMem;

Res<> Run() {
	Mem::Init();
	permMem = Mem::Create(16 * GB);
	tempMem = Mem::Create(16 * GB);

	Log::Init(permMem, tempMem);
	Log::AddFn([](Log::Msg const* msg) {
		Sys::Print(Str(msg->line, msg->lineLen));
		if (Sys::DbgPresent()) {
			Sys::DbgPrint(msg->line);
		}
	});

	FS::Init(permMem, tempMem);

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

int main(int argc, const char** argv) {
	if (argc == 2 && argv[1] == Str("test")) {
		Unit::Run();
		return 0;
	}

	if (Res<> r = Run(); !r) {
		Errorf("%s:%s", r.err->ns, r.err->sCode);
	}

	Gpu::Shutdown();
	Window::Shutdown();
	Mem::Destroy(permMem);
	Mem::Destroy(tempMem);

	return 0;
}