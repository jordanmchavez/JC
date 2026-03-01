#pragma once

#include "JC/Common.h"

namespace JC::Event  { struct Event; }
namespace JC::Gpu    { struct FrameData; }
namespace JC::Window { struct State; }

namespace JC::App {

//--------------------------------------------------------------------------------------------------

DefErr(App, Exit);

constexpr Str Cfg_Title            = "App.Title";
constexpr Str Cfg_WindowStyle      = "App.WindowStyle";
constexpr Str Cfg_WindowWidth      = "App.WindowWidth";
constexpr Str Cfg_WindowHeight     = "App.WindowHeight";
constexpr Str Cfg_WindowDisplayIdx = "App.WindowDisplayIdx";

//--------------------------------------------------------------------------------------------------

struct FrameData {
	U64             ticks = 0;
	Span<U64 const> actions;
	I32             mouseX = 0;
	I32             mouseY = 0;
	I64             mouseDeltaX = 0;
	I64             mouseDeltaY = 0;
	bool            exit = false;
};

struct App {
	Res<> (*PreInit)(Mem permMem, Mem tempMem);
	Res<> (*Init)(Window::State const* windowState);
	void  (*Shutdown)();
	Res<> (*Frame)(FrameData const* appFrameData);	// Err_Exit = exit program
	Res<> (*Draw)(Gpu::FrameData const* gpuFrameData);
	Res<> (*ResizeWindow)(U32 width, U32 height);
};

bool Run(App* app, int argc, char const* const* argv);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::App