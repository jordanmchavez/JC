#include "JC/App.h"
#include "JC/Event.h"
#include "JC/Math.h"
#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

struct OrthoCamera {
	Vec3 pos        = {};
	f32  halfWidth  = 0.0f;
	f32  halfHeight = 0.0f;

	void Set(f32 fov, f32 aspect, f32 z) {
		pos.z      = z;
		halfWidth  = z * tanf(fov / 2.0f);
		halfHeight = halfWidth / aspect;
	}

	Mat4 GetProjView() {
		return Mat4::Ortho(
			pos.x - halfWidth,
			pos.x + halfWidth,
			pos.y - halfHeight,
			pos.y + halfHeight,
			10.0f,
			-10.0f
		);
	};
};

//--------------------------------------------------------------------------------------------------

struct Game : App {
	Mem::Allocator*     allocator     = 0;
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;

	Res<> Init(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn, const Window::State* windowState) override {
		allocator     = allocatorIn;
		tempAllocator = tempAllocatorIn;
		logger        = loggerIn;
		windowState;

		//LoadTextureAtlas("Assets/Texture.png", "Assets/Texture.atlas");
		//GetTexture("
		//load atlas;
		//generate map iles

		return Ok();
	}

	void Shutdown() override {
	}

	Res<> Events(Span<Event::Event> events) override {
		for (u64 i = 0; i < events.len; i++) {
			switch (events[i].type) {
				case Event::Type::Exit: {
					Exit();
					break;
				}
			}
		}
		return Ok();
	}

	Res<> Update(double secs) override {
		secs;
		return Ok();
	}

	Res<> Draw() override {
		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static Game game;

App* GetApp() { return &game; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC