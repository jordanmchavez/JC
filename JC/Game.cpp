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
	Unit::Init(permMem, tempMem);
	Effect::Init(permMem);

	Try(Unit::LoadDefs("Assets/Units.json5"));
	Try(Battle::Load("Assets/Battle.json5"));
	Try(Battle::GenerateMap());

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> Update(App::UpdateData const* updateData) {
	if (updateData->exit) {
		return App::Err_Exit();
	}
	return Battle::Update(updateData);
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
	.Update       = Update,
	.Draw         = Draw,
	.ResizeWindow = ResizeWindow,
};

App::App* GetApp() { return &app; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Game