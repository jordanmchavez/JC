#pragma warning(disable: 4505)
#pragma warning(disable: 4189)

#include "JC/App.h"
#include "JC/Battle.h"
#include "JC/Cfg.h"
#include "JC/Draw.h"
#include "JC/Effect.h"
#include "JC/File.h"
#include "JC/Gpu.h"
#include "JC/Input.h"
#include "JC/Key.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Rng.h"
#include "JC/Time.h"
#include "JC/Unit.h"
#include "JC/Window.h"

namespace JC::Game {

//--------------------------------------------------------------------------------------------------

static Mem permMem;
static Mem tempMem;

//--------------------------------------------------------------------------------------------------

Res<> PreInit(Mem permMemIn, Mem tempMemIn) {
	permMem = permMemIn;
	tempMem = tempMemIn;
	Cfg::SetStr(App::Cfg_Title, "4x Fantasy");
	Cfg::SetU32(App::Cfg_WindowWidth, 1920);
	Cfg::SetU32(App::Cfg_WindowHeight, 1080);
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Init(Window::State const* windowState) {
	Try(Battle::Init(permMem, tempMem, windowState));
	Effect::Init(permMem);

	Try(Battle::LoadBattleJson("Assets/Battle.battlejson"));
	Try(Battle::GenerateMap());

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Frame(App::FrameData const* frameData) {
	if (frameData->exit) {
		return App::Err_Exit();
	}
	return Battle::Frame(frameData);
}

//--------------------------------------------------------------------------------------------------

Res<> Draw() {
	Battle::Draw();
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> ResizeWindow(U32 windowWidth, U32 windowHeight) {
	windowWidth;
	windowHeight;
	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
}

//--------------------------------------------------------------------------------------------------

App::App app = {
	.PreInit      = PreInit,
	.Init         = Init,
	.Shutdown     = Shutdown,
	.Frame        = Frame,
	.Draw         = Draw,
	.ResizeWindow = ResizeWindow,
};

App::App* GetApp() { return &app; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Game