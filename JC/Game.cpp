#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Math.h"
#include "JC/Render.h"
#include "JC/Window.h"
#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

struct Game : App {
	Mem::Allocator*     allocator                                   = 0;
	Mem::TempAllocator* tempAllocator                               = 0;
	Log::Logger*        logger                                      = 0;
	U32                 windowWidth                                 = 0;
	U32                 windowHeight                                = 0;
	Render::Sprite      shipSprite                                  = {};

	//---------------------------------------------------------------------------------------------

	Res<> Init(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn, const Window::State* windowState) override {
		allocator     = allocatorIn;
		tempAllocator = tempAllocatorIn;
		logger        = loggerIn;
		windowWidth   = windowState->width;
		windowHeight  = windowState->height;

		if (Res<> r = Render::LoadSpriteAtlas("Assets/SpaceShooter.png", "Assets/SpaceShooter.atlas"); !r) {
			return r;
		}

		if (Res<> r = Render::GetSprite("Ship1").To(shipSprite); !r) {
			return r;
		}

		return Ok();
	}

	//---------------------------------------------------------------------------------------------

	void Shutdown() override {
	}

	//---------------------------------------------------------------------------------------------

	Bool keyDown[(U32)Event::Key::Max];

	Res<> Events(Span<Event::Event> events) override {
		for (U64 i = 0; i < events.len; i++) {
			switch (events[i].type) {
				case Event::Type::Exit: {
					Exit();
					break;
				}

				case Event::Type::WindowResized: {
					windowWidth  = events[i].windowResized.width;
					windowHeight = events[i].windowResized.height;
					break;
				}

				case Event::Type::Key: {
					if (events[i].key.key == Event::Key::Escape) {
						Exit();
					}
					keyDown[(U32)events[i].key.key] = events[i].key.down;
					break;
				}
			}
		}
		return Ok();
	}

	//---------------------------------------------------------------------------------------------

	Vec3 shipPos;
	F32 shipSpeed = 150.0f;

	Res<> Update(double secs) override {
		if (keyDown[(U32)Event::Key::W]) { shipPos.y += (F32)(secs * shipSpeed); }
		if (keyDown[(U32)Event::Key::S]) { shipPos.y -= (F32)(secs * shipSpeed); }
		if (keyDown[(U32)Event::Key::A]) { shipPos.x -= (F32)(secs * shipSpeed); }
		if (keyDown[(U32)Event::Key::D]) { shipPos.x += (F32)(secs * shipSpeed); }
		return Ok();
	}

	//---------------------------------------------------------------------------------------------

	Res<> Draw() override {
		//Mat4 projView = Math::Ortho(0.0f, (F32)windowWidth, 0.0f, (F32)windowHeight, -10.0f, 10.0f);
		Mat4 projView = Math::Ortho(0.0f, (F32)windowWidth/4, 0.0f, (F32)windowHeight/4, -10.0f, 10.0f);
		//Mat4 projView = Math::Ortho(-(F32)windowWidth, (F32)windowWidth, -(F32)windowHeight, (F32)windowHeight, -10.0f, 10.0f);
		Render::SetProjView(&projView);

		Array<Render::DrawSprite> sprites(tempAllocator);
		sprites.Add(Render::DrawSprite {
			.sprite = shipSprite,
			.xyz    = shipPos,
		});
		Render::DrawSprites(sprites);

		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static Game game;

App* GetGame() { return &game; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC