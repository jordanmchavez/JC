#pragma once

#include "JC/Core.h"

namespace JC::Gpu { struct Frame; }
namespace JC::Window { struct Event; }

namespace JC::Battle {

//--------------------------------------------------------------------------------------------------

Res<> Init();
void Events(Span<const Window::Event> events);
void Update(U64 ticks);
void Draw(Gpu::Frame frame);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Battle