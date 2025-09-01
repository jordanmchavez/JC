#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Config.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Random.h"
#include "JC/Render2D.h"
#include "JC/Window.h"
#include <math.h>

namespace JC::AppShmup {

//--------------------------------------------------------------------------------------------------

struct Particle {
	Vec2 pos;
	F32  size;
	F32  angle;
	F32  speed;
	F32  rotation;
	Vec4 color;
	F32  life;
	F32  lifeEnd;
};

struct ParticleType {
	// animated
	// stretch animation over particle lifetime, or repeat, or runonce
	// use random subimage
	Render2D::Sprite sprite;
	F32             scaleMin;
	F32             scaleMax;
	F32             scaleInc;
	F32             sizeWiggle;
	F32             speedMin;
	F32             speedMax;
	F32             speedInc;
	F32             speedWiggle;
	F32             angleMin;
	F32             angleMax;
	F32             angleInc;
	F32             angleWiggle;
	F32             gravityAngle;
	F32             gravityAmount;
	F32             rotationMin;
	F32             rotationMax;
	F32             rotationInc;
	F32             rotationWiggle;
	Bool            rotationRelMotion;
	Vec4            color1;
	Vec4            color2;
	Vec4            color3;
	F32             lifeMin;
	F32             lifeMax;
	Array<Particle> particles;
};

enum struct ParticleEmitterShape {
	Rectangle,
	Ellipse,
	Diamond,
	Line,
};

enum struct ParticleEmitterDistribution {
	Linear,
	Gauss,
	InvGauss,
};

struct ParticleEmitter {
	ParticleType*               type;
	Vec2                        pos;
	ParticleEmitterShape        shape;
	F32                         minX;
	F32                         maxX;
	F32                         minY;
	F32                         maxY;
	ParticleEmitterDistribution dist;
	F32                         emitRate;
	F32                         emitAccum;
};

static constexpr Vec2 PlayAreaVirtualSize = { 800.0f, 900.0f };
static constexpr F32 SpriteScale = 8.0f;
static constexpr F32 ShipFireCooldown = 0.2f;

struct Ship {
	Vec2              pos                              = {};
	Vec2              speed                            = {};
	Vec2              size                             = {};
	F32               fireCooldown                     = 0.0f;
	Render2D::Sprite  spriteNotMoving                  = {};
	Render2D::Sprite  spriteMovingLeft                 = {};
	Render2D::Sprite  spriteMovingRight                = {};
	Render2D::Sprite* sprite                           = 0;
};

struct Bullet {
	Vec2 pos;
	Vec2 speed;
};


Mem::Allocator*        allocator                     = 0;
Mem::TempAllocator*    tempAllocator                 = 0;
Log::Logger*           logger                        = 0;
F32                    windowWidth                   = 0.0f;
F32                    windowHeight                  = 0.0f;
Bool                   keyDown[(U32)Event::Key::Max] = {};
Ship                   ship                          = {};
Render2D::Sprite       bulletSprite                  = {};
Vec2                   bulletSize                    = {};
Array<Bullet>          bullets                       = {};
Array<ParticleEmitter> particleEmitters              = {};
Array<ParticleType>    particleTypes                 = {};
Array<Particle>        particles                     = {};
U32                    emitterIdx                    = 0;

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
				p->pos.x = emitter->pos.x + Math::Lerp(emitter->minX, emitter->maxX, Random::NextF32());
				p->pos.y = emitter->pos.y + Math::Lerp(emitter->minY, emitter->maxY, Random::NextF32());
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
			default: JC_PANIC("Unhandled ParticleEmitterShape {}", emitter->shape);
		}
		p->size     = Math::Lerp(t->scaleMin, t->scaleMax, Random::NextF32());
		p->angle    = Math::Lerp(t->angleMin, t->angleMax, Random::NextF32());
		p->speed    = Math::Lerp(t->speedMin, t->speedMax, Random::NextF32());
		p->rotation = Math::Lerp(t->rotationMin, t->rotationMax, Random::NextF32());
		p->color    = t->color1;
		p->life     = 0.0f;
		p->lifeEnd  = Math::Lerp(t->lifeMin, t->lifeMax, Random::NextF32());
		emitter->emitAccum -= 1.0f;
	}
}

//---------------------------------------------------------------------------------------------

static void DrawParticleType(const ParticleType* type) {
	for (U64 i = 0; i < type->particles.len; i++) {
		const Particle* const p = &type->particles[i];
		Render2D::DrawSprite(type->sprite, p->pos, p->size * SpriteScale, p->rotation, p->color);
	}
}

//---------------------------------------------------------------------------------------------

static constexpr U32 WindowWidth  = 1920;
static constexpr U32 WindowHeight = 1080;

static Res<> PreInit(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn) {
	allocator     = allocatorIn;
	tempAllocator = tempAllocatorIn;
	logger        = loggerIn;
	Config::SetStr(App::Cfg_Title,        "Shmup");
	Config::SetU32(App::Cfg_WindowStyle,  (U32)Window::Style::BorderedResizable);
	Config::SetU32(App::Cfg_WindowWidth,  WindowWidth);
	Config::SetU32(App::Cfg_WindowHeight, WindowHeight);
	return Ok();
}

//---------------------------------------------------------------------------------------------

static Res<> Init(const Window::State* windowState) {
	windowWidth  = (F32)windowState->width;
	windowHeight = (F32)windowState->height;

	Render2D::InitDesc initDesc = {
		.allocator     = allocator,
		.tempAllocator = tempAllocator,
		.logger        = logger,
		.windowWidth   = WindowWidth,
		.windowHeight  = WindowHeight,

	};
	JC_CHECK_RES(Render2D::Init(&initDesc));

	JC_CHECK_RES(Render2D::LoadSpriteAtlas("Assets/SpaceShooter.png", "Assets/SpaceShooter.atlas"));
JC_CHECK_RES(Gpu::ImmediateWait());
	JC_CHECK_RES(Render2D::GetSprite("Ship1").To(ship.spriteNotMoving));
	JC_CHECK_RES(Render2D::GetSprite("Ship1Left").To(ship.spriteMovingLeft));
	JC_CHECK_RES(Render2D::GetSprite("Ship1Right").To(ship.spriteMovingRight));
	JC_CHECK_RES(Render2D::GetSprite("Bullet").To(bulletSprite));

	ship.size = Render2D::GetSpriteSize(ship.spriteNotMoving);
	ship.speed = { 300.0f, 300.0f };
	ship.sprite = &ship.spriteNotMoving;
		
	bulletSize = Render2D::GetSpriteSize(bulletSprite);

	bullets.Init(allocator);

	particleEmitters.Init(allocator);
	particleTypes.Init(allocator);
	particles.Init(allocator);

	ParticleType* const type = particleTypes.Add();
	if (Res<> r = Render2D::GetSprite("Particle1").To(type->sprite); !r) { return r; }
	type->scaleMin          = 0.25f;
	type->scaleMax          = 1.0f;
	type->scaleInc          = 1.0f;
	//type->sizeWiggle        = ;
	type->speedMin          = 20.0f;
	type->speedMax          = 40.0f;
	type->speedInc          = -0.0f;
	//type->speedWiggle       = ;
	type->angleMin          = 270.0f - 30.0f;
	type->angleMax          = 270.0f + 30.0f;
	type->angleInc          = 100.0f;
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
	type->lifeMin           = 2.0f;
	type->lifeMax           = 4.0f;
	type->particles.Init(allocator);

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
	Render2D::Shutdown();
}

//---------------------------------------------------------------------------------------------

static Res<> Events(Span<Event::Event> events) {
	for (U64 i = 0; i < events.len; i++) {
		switch (events[i].type) {
			case Event::Type::Exit: {
				App::Exit();
				break;
			}

			case Event::Type::WindowResized: {
				windowWidth  = (F32)events[i].windowResized.width;
				windowHeight = (F32)events[i].windowResized.height;
				break;
			}

			case Event::Type::Key: {
				if (events[i].key.key == Event::Key::Escape) {
					App::Exit();
				}
				keyDown[(U32)events[i].key.key] = events[i].key.down;
				break;
			}
		}
	}
	return Ok();
}

//---------------------------------------------------------------------------------------------

static Res<> Update(double secs) {
	const F32 fsecs = (F32)secs;

	for (U64 i = 0; i < particleTypes.len; i++) {
		UpdateParticleType(fsecs, &particleTypes[i]);
	}

	for (U64 i = 0; i < particleEmitters.len; i++) {
		UpdateParticleEmitter(fsecs, &particleEmitters[i]);
	}

	ship.pos.x = Clamp(ship.pos.x, 0.0f, PlayAreaVirtualSize.x);
	ship.pos.y = Clamp(ship.pos.y, 0.0f, PlayAreaVirtualSize.y);

	for (U64 i = 0; i < bullets.len; ) {
		Bullet* b = &bullets[i];
		if (
			b->pos.x < 0.0f ||
			b->pos.x > PlayAreaVirtualSize.x ||
			b->pos.y < 0.0f ||
			b->pos.y > PlayAreaVirtualSize.y
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
				.pos   = { ship.pos.x, ship.pos.y + ((ship.size.y + bulletSize.y) / 2.0f) },
				.speed = { 0.0f, -120.0f },
			});
			ship.fireCooldown = ShipFireCooldown;
		}
	}

	particleEmitters[emitterIdx].pos = ship.pos;
	//particleEmitters[emitterIdx].pos.y -= ship.size.y / 2.0f;

	return Ok();
}

//---------------------------------------------------------------------------------------------

static Res<> Draw(Gpu::Frame frame) {
	Render2D::BeginFrame(frame);

	const Vec2 origin = { 100.0f, 50.0f };
	Vec2 pos = origin;

	const Vec2 PlayAreaSize = { windowWidth / 2.0f, windowHeight - 100.0f };
	//Render2D::DrawRect(origin, PlayAreaSize, { 0.1f, 0.2f, 0.05f, 1.0f }, Vec4(), 0.0f, 0.0f);

	Vec2 finalPos = {
		(ship.pos.x * PlayAreaSize.x / PlayAreaVirtualSize.x) + origin.x,
		(ship.pos.y * PlayAreaSize.y / PlayAreaVirtualSize.y) + origin.y,
	};
	Render2D::DrawSprite(*ship.sprite, finalPos, SpriteScale, 0.0f, Vec4(1.0f, 1.0f, 1.0f, 1.0f));

	for (U64 i = 0; i < bullets.len; i++) {
		Render2D::DrawSprite(bulletSprite, bullets[i].pos);
	}

	for (U64 i = 0; i < particleTypes.len; i++) {
		DrawParticleType(&particleTypes[i]);
	}

	Render2D::EndFrame();
	return Ok();
}

//--------------------------------------------------------------------------------------------------

App::Fns appFns = {
	.PreInit  = PreInit,
	.Init     = Init,
	.Shutdown = Shutdown,
	.Events   = Events,
	.Update   = Update,
	.Draw     = Draw,
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC::AppShmup