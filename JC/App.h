#pragma once

#include "JC/Common.h"

namespace JC::Event  { struct Event; }
namespace JC::Gpu    { struct Frame; }
namespace JC::Window { struct State; }

namespace JC::App {

//--------------------------------------------------------------------------------------------------

constexpr Str Cfg_Title            = "App.Title";
constexpr Str Cfg_WindowStyle      = "App.WindowStyle";
constexpr Str Cfg_WindowWidth      = "App.WindowWidth";
constexpr Str Cfg_WindowHeight     = "App.WindowHeight";
constexpr Str Cfg_WindowDisplayIdx = "App.WindowDisplayIdx";

//--------------------------------------------------------------------------------------------------

struct App {
	Res<> (*PreInit)(Mem permMem, Mem tempMem);
	Res<> (*Init)(Window::State const* windowState);
	void  (*Shutdown)();
	Res<> (*Update)(U64 ticks);
	Res<> (*Draw)(Gpu::Frame* frame);
};

void RequestExit();

bool Run(App* app, int argc, char const* const* argv);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::App