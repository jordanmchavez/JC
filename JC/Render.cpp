#pragma once

#include "JC/Render.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct RenderApiImpl : RenderApi {
	void Init() override {
	}
};

RenderApiImpl renderApiImpl;

RenderApi* RenderApi::Get() {
	return &renderApiImpl;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC