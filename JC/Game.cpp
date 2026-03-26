#include "JC/App.h"
#include "JC/Battle.h"
#include "JC/Cfg.h"
#include "JC/Draw.h"
#include "JC/File.h"
#include "JC/Gpu.h"
#include "JC/Hash.h"
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

static Str ToLower(Mem mem, Str str) {
	char* lower = Mem::AllocT<char>(mem, str.len);
	for (U32 i = 0; i < str.len; i++) {
		char c = str[i];
		if (c >= 'A' && c <= 'Z') {
			lower[i] = c - 'A';
		} else {
			lower[i] = c;
		}
	}
	return Str(lower, str.len);
}

using LoadFn = Res<> (Str path);

struct Loader {
	Str     ext;
	LoadFn* loadFn;
};

static constexpr Loader loaders[] = {
	{ "sprites.def", Draw::LoadSprites },
	{ "font.def", Draw::LoadFont },
	{ "map.def", Battle::LoadMap },
};

static Res<> Load() {
	Span<Str> paths; TryTo(File::EnumFiles("Assets", "def"), paths);
	Span<U64> extHashes = Mem::AllocSpan<U64>(tempMem, paths.len);
	for (U64 i = 0; i < paths.len; i++) {
		extHashes[i] = Hash(ToLower(tempMem, File::GetMaxExt(paths[i])));
	}

	for (U64 i = 0; i < LenOf(loaders); i++) {
		U64 extHash = Hash(loaders[i].ext);
		for (U64 i = 0; i < extHashes.len; i++) {
			if (extHashes[i] == extHash) {
				Try(loaders[i].loadFn(paths[i]));
			}
		}
	}
};

//--------------------------------------------------------------------------------------------------

Res<> Init(Window::State const* windowState) {
	Battle::Init(permMem, tempMem, windowState);
	//Effect::Init(permMem);

	Try(Load());
	Try(Gpu::ImmediateWait());

	Battle::GenerateRandomMap(16, 16);
	Battle::GenerateRandomArmies();

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