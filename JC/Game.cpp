#include "JC/App.h"
#include "JC/Battle.h"
#include "JC/Battle_Map.h"
#include "JC/Cfg.h"
#include "JC/Draw.h"
#include "JC/File.h"
#include "JC/Gpu.h"
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
	Battle::Init(permMem, tempMem, windowState);
	Battle::Map::Init(permMem, tempMem);
	//Effect::Init(permMem);

	Span<Str> paths; TryTo(File::EnumFiles("Assets", ".def"), paths);
	for (U64 i = 0; i < paths.len;) {
		if (File::HasExt(paths[i], ".sprites.def")) {
			Try(Draw::LoadSprites(paths[i]));
			paths.data[i] = paths.data[--paths.len];
		} else if (File::HasExt(paths[i], ".font.def")) {
			Try(Draw::LoadFont(paths[i]));
			paths.data[i] = paths.data[--paths.len];
		} else {
			i++;
		}
	}

	for (U64 i = 0; i < paths.len;) {
		if (File::HasExt(paths[i], ".map.def")) {
			Try(Battle::Map::Load(paths[i]));
			paths.data[i] = paths.data[--paths.len];
		} else {
			i++;
		}
	}

	Try(Gpu::ImmediateWait());

	Battle::Map::GenerateRandomMap(16, 16);

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