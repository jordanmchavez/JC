#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Cfg.h"
#include "JC/Draw.h"
#include "JC/Event.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Rng.h"
#include "JC/Time.h"
#include "JC/Window.h"

namespace JC::Shooter {

//--------------------------------------------------------------------------------------------------

static constexpr U32  MaxBullets          = 1024;
static constexpr U32  MaxParticleEmitters = 1024;
static constexpr U32  MaxParticleTypes    = 64;
static constexpr U32  MaxEngineParticles  = 128;
static constexpr Vec2 CanvasSize          = { 300.0f, 200.0f };
static constexpr F32  CanvasScale         = 5.f;
static constexpr F32  ShipFireCooldown    = 0.1f;

struct Particle {
	Vec2 pos;
	F32  size = 0.0f;
	F32  angle = 0.0f;
	F32  speed = 0.0f;
	F32  rotation = 0.0f;
	Vec4 color;
	F32  life = 0.0f;
	F32  lifeEnd = 0.0f;
};

struct ParticleType {
	// animated
	// stretch animation over particle lifetime, or repeat, or runonce
	// use random subimage
	Draw::Sprite    sprite;
	Vec2            spriteSize;
	F32             scaleMin = 0.0f;
	F32             scaleMax = 0.0f;
	F32             scaleInc = 0.0f;
	F32             sizeWiggle = 0.0f;
	F32             speedMin = 0.0f;
	F32             speedMax = 0.0f;
	F32             speedInc = 0.0f;
	F32             speedWiggle = 0.0f;
	F32             angleMin = 0.0f;
	F32             angleMax = 0.0f;
	F32             angleInc = 0.0f;
	F32             angleWiggle = 0.0f;
	F32             gravityAngle = 0.0f;
	F32             gravityAmount = 0.0f;
	F32             rotationMin = 0.0f;
	F32             rotationMax = 0.0f;
	F32             rotationInc = 0.0f;
	F32             rotationWiggle = 0.0f;
	bool            rotationRelMotion = false;
	Vec4            color1;
	Vec4            color2;
	Vec4            color3;
	F32             lifeMin = 0.0f;
	F32             lifeMax = 0.0f;
	Array<Particle> particles;
};

enum struct ParticleEmitterShape {
	Invalid = 0,
	Rectangle,
	Ellipse,
	Diamond,
	Line,
};

enum struct ParticleEmitterDistribution {
	Invalid = 0,
	Linear,
	Gauss,
	InvGauss,
};

struct ParticleEmitter {
	ParticleType*               type = 0;
	Vec2                        pos;
	ParticleEmitterShape        shape = ParticleEmitterShape::Invalid;
	F32                         minX = 0.0f;
	F32                         maxX = 0.0f;
	F32                         minY = 0.0f;
	F32                         maxY = 0.0f;
	ParticleEmitterDistribution dist = ParticleEmitterDistribution::Invalid;
	F32                         emitRate = 0.0f;
	F32                         emitAccum = 0.0f;
};

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
static Array<ParticleEmitter> particleEmitters;
static Array<ParticleType>    particleTypes;
static U32                    emitterIdx; 

//---------------------------------------------------------------------------------------------

static void UpdateParticleType(F32 secs, ParticleType* type) {
	for (U64 i = 0; i < type->particles.len;) {
		Particle* const p = &type->particles[i];
		if (p->life += secs; p->life >= p->lifeEnd) {
			type->particles.RemoveUnordered(i);
			continue;
		}
		p->size     += secs * type->scaleInc;
		p->speed    += secs * type->speedInc;
		p->angle    += secs * type->angleInc;
		p->rotation += secs * type->rotationInc ;
		p->pos.x    += secs * p->speed * cosf(Math::DegToRad(p->angle));
		p->pos.y    += secs * p->speed * sinf(Math::DegToRad(p->angle));
		F32 t = p->life / p->lifeEnd;
		if (t < 0.5f) {
			p->color = Math::Lerp(type->color1, type->color2, t * 2.0f);
		} else {
			p->color = Math::Lerp(type->color2, type->color3, (t - 0.5f) * 2.0f);
		}
		i++;
	}
}

//---------------------------------------------------------------------------------------------

static void UpdateParticleEmitter(F32 secs, ParticleEmitter* emitter) {
	ParticleType* const t = emitter->type;
	emitter->emitAccum += emitter->emitRate * secs;
	while (emitter->emitAccum >= 1.0f) {
		Particle* const p = t->particles.Add();
		switch (emitter->shape) {
			case ParticleEmitterShape::Rectangle: {
				p->pos.x = emitter->pos.x + Math::Lerp(emitter->minX, emitter->maxX, Rng::NextF32()) - (t->spriteSize.x / 2) + 0.5f;
				p->pos.y = emitter->pos.y + Math::Lerp(emitter->minY, emitter->maxY, Rng::NextF32()) - (t->spriteSize.x / 2) + 0.5f;

				break;
			}
			case ParticleEmitterShape::Ellipse: {
				break;
			}
			case ParticleEmitterShape::Diamond: {
				break;
			}
			case ParticleEmitterShape::Line: {
				break;
			}
			default: Panic("Unhandled ParticleEmitterShape %u", (U32)emitter->shape);
		}
		p->size     = Math::Lerp(t->scaleMin, t->scaleMax, Rng::NextF32());
		p->angle    = Math::Lerp(t->angleMin, t->angleMax, Rng::NextF32());
		p->speed    = Math::Lerp(t->speedMin, t->speedMax, Rng::NextF32());
		p->rotation = Math::Lerp(t->rotationMin, t->rotationMax, Rng::NextF32());
		p->color    = t->color1;
		p->life     = 0.0f;
		p->lifeEnd  = Math::Lerp(t->lifeMin, t->lifeMax, Rng::NextF32());
		emitter->emitAccum -= 1.0f;
	}
}

//---------------------------------------------------------------------------------------------

static void DrawParticleType(const ParticleType* type) {
	for (U64 i = 0; i < type->particles.len; i++) {
		const Particle* const p = &type->particles[i];
		Draw::DrawSprite(type->sprite, p->pos, Vec2(p->size, p->size), p->rotation, p->color);
	}
}

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

	for (U64 i = 0; i < particleTypes.len; i++) {
		UpdateParticleType(fsecs, &particleTypes[i]);
	}

	for (U64 i = 0; i < particleEmitters.len; i++) {
		UpdateParticleEmitter(fsecs, &particleEmitters[i]);
	}

	ship.pos.x = Clamp(ship.pos.x, 0.0f, CanvasSize.x - ship.size.x);
	ship.pos.y = Clamp(ship.pos.y, 0.0f, CanvasSize.y - ship.size.y);

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
	ship.pos= Math::Add(ship.pos, dPos);

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

	for (U64 i = 0; i < particleTypes.len; i++) {
		DrawParticleType(&particleTypes[i]);
	}

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