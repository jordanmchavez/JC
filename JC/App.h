#pragma once

#include "JC/Common.h"

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

struct UpdateData {
	F32             sec;
	Span<U64 const> actions;
	I32             mouseX;
	I32             mouseY;
	I64             mouseDeltaX;
	I64             mouseDeltaY;
	bool            exit;
};

struct App {
	Res<> (*PreInit)(Mem permMem, Mem tempMem);
	Res<> (*Init)(Window::State const* windowState);
	void  (*Shutdown)();
	Res<> (*Update)(UpdateData const* appUpdateData);	// Err_Exit = exit program
	Res<> (*Draw)();
	Res<> (*ResizeWindow)(U32 width, U32 height);
};

bool Run(App* app, int argc, char const* const* argv);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::App