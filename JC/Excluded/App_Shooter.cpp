#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Cfg.h"
#include "JC/Draw.h"
#include "JC/Event.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Particle.h"
#include "JC/Rng.h"
#include "JC/Time.h"
#include "JC/Window.h"

namespace JC::Shooter {

//--------------------------------------------------------------------------------------------------

static constexpr U32  MaxBullets          = 1024;
static constexpr U32  MaxEngineParticles  = 128;
static constexpr Vec2 CanvasSize          = { 300.0f, 200.0f };
static constexpr F32  CanvasScale         = 3.f;
static constexpr F32  ShipFireCooldown    = 0.1f;

struct Ship {
	Vec2          pos;
	Vec2          speed;
	Vec2          size;
	F32           fireCooldown = 0.0f;
	Draw::Sprite  spriteNotMoving;
	Draw::Sprite  spriteMovingLeft;
	Draw::Sprite  spriteMovingRight;
	Draw::Sprite* sprite = 0;
};

struct Bullet {
	Vec2 pos;
	Vec2 speed;
};

static Mem                    permMem; 
static Mem                    tempMem; 
static F32                    windowWidth;
static F32                    windowHeight;
static bool                   keyDown[(U32)Event::Key::Max];
static Draw::Canvas           canvas;
static Ship                   ship;
static Draw::Sprite           bulletSprite;
static Vec2                   bulletSize;
static Array<Bullet>          bullets;
static U32                    emitterIdx; 

//---------------------------------------------------------------------------------------------

static constexpr U32 WindowWidth  = 1920;
static constexpr U32 WindowHeight = 1080;

static Res<> PreInit(Mem permMemIn, Mem tempMemIn) {
	permMem = permMemIn;
	tempMem = tempMemIn;
	Cfg::SetStr(App::Cfg_Title,        "Shmup");
	Cfg::SetU32(App::Cfg_WindowStyle,  (U32)Window::Style::BorderedResizable);
	Cfg::SetU32(App::Cfg_WindowWidth,  WindowWidth);
	Cfg::SetU32(App::Cfg_WindowHeight, WindowHeight);
	return Ok();
}

//---------------------------------------------------------------------------------------------

static Res<> Init(const Window::State* windowState) {
	windowWidth  = (F32)windowState->width;
	windowHeight = (F32)windowState->height;

	Try(Draw::CreateCanvas((U32)CanvasSize.x, (U32)CanvasSize.y).To(canvas));

	Try(Draw::LoadSpriteAtlas("Assets/SpaceShooter.png", "Assets/SpaceShooter.atlas"));
	Try(Gpu::ImmediateWait());

	Try(Draw::GetSprite("Ship1").To(ship.spriteNotMoving));
	Try(Draw::GetSprite("Ship1Left").To(ship.spriteMovingLeft));
	Try(Draw::GetSprite("Ship1Right").To(ship.spriteMovingRight));
	Try(Draw::GetSprite("Bullet").To(bulletSprite));

	ship.size = Draw::GetSpriteSize(ship.spriteNotMoving);
	ship.pos   = { CanvasSize.x / 2.f, CanvasSize.y / 2.f };
	ship.speed = { 150.0f, 150.0f };
	ship.sprite = &ship.spriteNotMoving;
		
	bulletSize = Draw::GetSpriteSize(bulletSprite);

	bullets.Init(permMem, MaxBullets);

	particleEmitters.Init(permMem, MaxParticleEmitters);
	particleTypes.Init(permMem, MaxParticleTypes);

	ParticleType* const type = particleTypes.Add();
	if (Res<> r = Draw::GetSprite("Particle1").To(type->sprite); !r) { return r; }
	type->spriteSize        = Draw::GetSpriteSize(type->sprite);
	type->scaleMin          = 0.25f;
	type->scaleMax          = 1.0f;
	type->scaleInc          = 1.0f;
	//type->sizeWiggle        = ;
	type->speedMin          = 40.0f;
	type->speedMax          = 80.0f;
	type->speedInc          = -0.0f;
	//type->speedWiggle       = ;
	type->angleMin          = 90.0f - 30.0f;
	type->angleMax          = 90.0f + 30.0f;
	type->angleInc          = 0.0f;//100.0f;
	//type->angleWiggle       = ;
	//type->gravityAngle      = ;
	//type->gravityAmount     = ;
	type->rotationMin       = 0.0f;
	type->rotationMax       = 0.0f;
	type->rotationInc       = 0.0f;
	//type->rotationWiggle    = ;
	//type->rotationRelMotion = ;
	type->color1            = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	type->color2            = Vec4(1.0f, 1.0f, 1.0f, 0.5f);
	type->color3            = Vec4(1.0f, 1.0f, 1.0f, 0.0f);
	type->lifeMin           = 0.5f;
	type->lifeMax           = 1.0f;
	type->particles.Init(permMem, MaxEngineParticles);

	ParticleEmitter* const emitter = particleEmitters.Add();
	emitter->type      = type;
	emitter->pos       = { 0.0f, 0.0f };
	emitter->shape     = ParticleEmitterShape::Rectangle;
	emitter->minX      = -0.0f;
	emitter->maxX      =  0.0f;
	emitter->minY      = 0.0f;
	emitter->maxY      = 0.0f;
	emitter->dist      = ParticleEmitterDistribution::Linear;
	emitter->emitRate  = 20.0f;
	emitter->emitAccum = 0.0f;
	emitterIdx = (U32)(emitter - particleEmitters.data);

	return Ok();
}

//---------------------------------------------------------------------------------------------

static void Shutdown() {
	Draw::DestroyCanvas(canvas);
}

//---------------------------------------------------------------------------------------------

static Res<> Update(U64 ticks) {
	F32 const fsecs = (F32)Time::Secs(ticks);

	Event::Event event;
	while (Event::GetEvent(&event)) {
		switch (event.type) {
			case Event::Type::ExitRequest: {
				App::RequestExit();
				break;
			}

			case Event::Type::WindowResized: {
				windowWidth  = (F32)event.windowResized.width;
				windowHeight = (F32)event.windowResized.height;
				break;
			}

			case Event::Type::Key: {
				if (event.key.key == Event::Key::Escape) {
					App::RequestExit();
				}
				keyDown[(U32)event.key.key] = event.key.down;
				break;
			}
		}
	}

	Particle::Frame(ticks);

	for (U64 i = 0; i < bullets.len; ) {
		Bullet* b = &bullets[i];
		if (
			b->pos.x < 0.0f ||
			b->pos.x > CanvasSize.x ||
			b->pos.y < 0.0f ||
			b->pos.y > CanvasSize.y
		) {
			bullets.RemoveUnordered(i);
			continue;
		}
		b->pos = Math::Add(b->pos, Math::Mul(b->speed, fsecs));
		i++;
	}

	if (ship.fireCooldown > 0.0f) {
		ship.fireCooldown -= fsecs;
	}

	Vec2 dPos;
	if (keyDown[(U32)Event::Key::W]) { dPos.y -= fsecs * ship.speed.y; }
	if (keyDown[(U32)Event::Key::S]) { dPos.y += fsecs * ship.speed.y; }
	if (keyDown[(U32)Event::Key::A]) { dPos.x -= fsecs * ship.speed.x; }
	if (keyDown[(U32)Event::Key::D]) { dPos.x += fsecs * ship.speed.x; }
	ship.pos = Math::Add(ship.pos, dPos);
	ship.pos.x = Clamp(ship.pos.x, 0.0f, CanvasSize.x - ship.size.x);
	ship.pos.y = Clamp(ship.pos.y, 0.0f, CanvasSize.y - ship.size.y);


		 if (dPos.x == 0.0f) { ship.sprite = &ship.spriteNotMoving;   }
	else if (dPos.x < 0.0f)  { ship.sprite = &ship.spriteMovingLeft;  }
	else                     { ship.sprite = &ship.spriteMovingRight; }

	if (keyDown[(U32)Event::Key::Space]) {
		if (ship.fireCooldown <= 0.0f) {
				
			bullets.Add(Bullet {
				.pos   = { ship.pos.x + (ship.size.x / 2.f) - (bulletSize.x / 2.f), ship.pos.y },
				.speed = { 0.0f, -100.0f },
			});
			ship.fireCooldown = ShipFireCooldown;
		}
	}

	particleEmitters[emitterIdx].pos = Vec2(ship.pos.x + (ship.size.x / 2.f), ship.pos.y + ship.size.y);

	return Ok();
}

//---------------------------------------------------------------------------------------------

static Res<> Draw(Gpu::Frame* gpuFrame) {
	Draw::BeginFrame(gpuFrame);

	Draw::SetCanvas(canvas);

	Draw::DrawRect(Vec2(0.f, 0.f), CanvasSize, Vec4(0.05f, 0.1f, 0.15f, 1.0f), Vec4(1.f, 1.f, 1.f, 1.f), 0.f, 0.f);

	Draw::DrawSprite(*ship.sprite, ship.pos);

	for (U64 i = 0; i < bullets.len; i++) {
		Draw::DrawSprite(bulletSprite, bullets[i].pos);
	}

	Particle::Draw();

	Draw::SetCanvas();
	Draw::DrawCanvas(canvas, Vec2(80.f, 60.f), Vec2(CanvasScale, CanvasScale));

	Draw::EndFrame();
	return Ok();
}

//--------------------------------------------------------------------------------------------------

App::App shooterApp = {
	.PreInit  = PreInit,
	.Init     = Init,
	.Shutdown = Shutdown,
	.Update   = Update,
	.Draw     = Draw,
};

App::App* GetApp() { return &shooterApp; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Shooter