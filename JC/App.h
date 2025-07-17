#pragma once

#include "JC/Core.h"

namespace JC::Log { struct Logger; } 
namespace JC::Event { struct Event; }
namespace JC::Window { struct State; }
namespace JC::Gpu { struct Cmd; struct Image; }

namespace JC::App {

//--------------------------------------------------------------------------------------------------

static constexpr Str Cfg_Title        = "App.Title";
static constexpr Str Cfg_WindowStyle  = "App.WindowStyle";
static constexpr Str Cfg_WindowWidth  = "App.WindowWidth";
static constexpr Str Cfg_WindowHeight = "App.WindowHeight";
static constexpr Str Cfg_DisplayIdx   = "App.DisplayIdx";
	
void Exit();

struct Fns {
	Res<> (*PreInit)(Mem::Allocator* allocator, Mem::TempAllocator* tempAllocator, Log::Logger* logger);
	Res<> (*Init)(const Window::State* windowState);
	void  (*Shutdown)();
	Res<> (*Events)(Span<Event::Event> events);
	Res<> (*Update)(double secs);
	Res<> (*Draw)(Gpu::Cmd cmd, Gpu::Image swapchainImage);
};

void Run(Fns* fns, int argc, const char** argv);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::App