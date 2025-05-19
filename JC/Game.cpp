#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Math.h"
#include "JC/Sprite.h"
#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Player {
	Vec2 pos;
};

struct Game : App {
	Mem::Allocator*     allocator     = 0;
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;

	Sprite::Sprite shipSprite;

	Res<> Init(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn, const Window::State* windowState) override {
		allocator     = allocatorIn;
		tempAllocator = tempAllocatorIn;
		logger        = loggerIn;
		windowState;

		Sprite::LoadAtlas("Assets/SpaceShooter.png", "Assets/SpaceShooter.atlas");
		shipSprite = Sprite::Get("Ship1");

		return Ok();
	}

	void Shutdown() override {
	}

	Res<> Events(Span<Event::Event> events) override {
		for (U64 i = 0; i < events.len; i++) {
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
		Array<Sprite::Sprite> sprites(tempAllocator);
		sprites.Add(shipSprite);
		Sprite::Draw(sprites);

		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static Game game;

App* GetApp() { return &game; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC