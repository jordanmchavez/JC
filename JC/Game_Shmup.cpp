#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Random.h"
#include "JC/Render.h"
#include "JC/Window.h"
#include <math.h>

namespace JC {

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
	Render::Sprite  sprite;
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


struct Game : App {
	static constexpr F32 SpriteScale = 8.0f;
	static constexpr F32 ShipFireCooldown = 0.2f;
	static constexpr U32 EngineSpriteCount = 4;
	static constexpr F32 EngineSpriteCooldown = 0.1f;


	struct Ship {
		Vec2            pos                              = {};
		Vec2            speed                            = {};
		Vec2            size                             = {};
		F32             fireCooldown                     = 0.0f;
		Render::Sprite  spriteNotMoving                  = {};
		Render::Sprite  spriteMovingLeft                 = {};
		Render::Sprite  spriteMovingRight                = {};
		Render::Sprite* sprite                           = 0;
		Render::Sprite  engineSprites[EngineSpriteCount] = {};
		Vec2            engineSizes[EngineSpriteCount]   = {};
		U32             engineSpriteIdx      = 0;
		F32             engineSpriteCooldown = 0.0f;
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
	Render::Sprite         bulletSprite                  = {};
	Vec2                   bulletSize                    = {};
	Array<Bullet>          bullets                       = {};
	Array<ParticleEmitter> particleEmitters              = {};
	Array<ParticleType>    particleTypes                 = {};
	Array<Particle>        particles                     = {};
	U32                    emitterIdx                    = 0;

	//---------------------------------------------------------------------------------------------

	void UpdateParticleType(F32 secs, ParticleType* type) {
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

	void UpdateParticleEmitter(F32 secs, ParticleEmitter* emitter) {
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
				default: Panic("Unhandled ParticleEmitterShape", "shape", emitter->shape);
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

	void DrawParticleType(const ParticleType* type) {
		for (U64 i = 0; i < type->particles.len; i++) {
			const Particle* const p = &type->particles[i];
			Render::DrawSprite(type->sprite, p->pos, p->size * SpriteScale, p->rotation, p->color);
		}
	}

	Res<> PreInit(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn) override {
		allocator     = allocatorIn;
		tempAllocator = tempAllocatorIn;
		logger        = loggerIn;
		return Ok();
	}

	Res<> Init(const Window::State* windowState) override {
		windowWidth  = (F32)windowState->width;
		windowHeight = (F32)windowState->height;

		if (Res<> r = Render::LoadSpriteAtlas("Assets/SpaceShooter.png", "Assets/SpaceShooter.atlas"); !r) { return r; }
		if (Res<> r = Render::GetSprite("Ship1").To(ship.spriteNotMoving);   !r) { return r; }
		if (Res<> r = Render::GetSprite("Ship1Left").To(ship.spriteMovingLeft);  !r) { return r; }
		if (Res<> r = Render::GetSprite("Ship1Right").To(ship.spriteMovingRight); !r) { return r; }
		for (U32 i = 0; i < EngineSpriteCount; i++) {
			if (Res<> r = Render::GetSprite(Fmt::Printf(tempAllocator, "Engine{}", i + 1)).To(ship.engineSprites[i]); !r) { return r; }
			ship.engineSizes[i] = Render::GetSpriteSize(ship.engineSprites[i]);
		}
		if (Res<> r = Render::GetSprite("Bullet").To(bulletSprite); !r) { return r; }

		ship.size = Render::GetSpriteSize(ship.spriteNotMoving);
		ship.speed = { 80.0f, 80.0f };
		ship.sprite = &ship.spriteNotMoving;
		
		ship.engineSpriteIdx = 0;

		bulletSize = Render::GetSpriteSize(bulletSprite);

		bullets.Init(allocator);

		particleEmitters.Init(allocator);
		particleTypes.Init(allocator);
		particles.Init(allocator);

		ParticleType* const type = particleTypes.Add();
		if (Res<> r = Render::GetSprite("Particle1").To(type->sprite); !r) { return r; }
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

	void Shutdown() override {
	}

	//---------------------------------------------------------------------------------------------

	Res<> Events(Span<Event::Event> events) override {
		for (U64 i = 0; i < events.len; i++) {
			switch (events[i].type) {
				case Event::Type::Exit: {
					Exit();
					break;
				}

				case Event::Type::WindowResized: {
					windowWidth  = (F32)events[i].windowResized.width;
					windowHeight = (F32)events[i].windowResized.height;
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

	Res<> Update(double secs) override {
		const F32 fsecs = (F32)secs;

		for (U64 i = 0; i < particleTypes.len; i++) {
			UpdateParticleType(fsecs, &particleTypes[i]);
		}

		for (U64 i = 0; i < particleEmitters.len; i++) {
			UpdateParticleEmitter(fsecs, &particleEmitters[i]);
		}

		const F32 boundL = -windowWidth  / SpriteScale;
		const F32 boundR =  windowWidth  / SpriteScale;
		const F32 boundB = -windowHeight / SpriteScale;
		const F32 boundT =  windowHeight / SpriteScale;

		ship.pos.x = Clamp(ship.pos.x, boundL + ship.size.x / 2.0f, boundR - ship.size.x / 2.0f);
		ship.pos.y = Clamp(ship.pos.y, boundB + ship.size.x / 2.0f, boundT - ship.size.x / 2.0f);

		ship.engineSpriteCooldown -= fsecs;
		if (ship.engineSpriteCooldown < 0.0f) {
			ship.engineSpriteCooldown = EngineSpriteCooldown;
			ship.engineSpriteIdx = (ship.engineSpriteIdx + 1) % LenOf(ship.engineSprites);
		}

		for (U64 i = 0; i < bullets.len; ) {
			Bullet* b = &bullets[i];
			if (
				b->pos.x  < boundL + bulletSize.x / 2.0f ||
				b->pos.x >= boundR - bulletSize.x / 2.0f ||
				b->pos.y  < boundB + bulletSize.y / 2.0f ||
				b->pos.y >= boundT - bulletSize.y / 2.0f
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
		if (keyDown[(U32)Event::Key::W]) { dPos.y += fsecs * ship.speed.y; }
		if (keyDown[(U32)Event::Key::S]) { dPos.y -= fsecs * ship.speed.y; }
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
					.speed = { 0.0f, 120.0f },
				});
				ship.fireCooldown = ShipFireCooldown;
			}
		}

		particleEmitters[emitterIdx].pos = ship.pos;
		particleEmitters[emitterIdx].pos.y -= ship.size.y / 2.0f;

		return Ok();
	}

	//---------------------------------------------------------------------------------------------

	U32 frameIdx;
	Gpu::Image swapchainImage;

	Res<> Draw() override {
		frameIdx = Gpu::GetFrameIdx();

		swapchainImage = Gpu::GetSwapchainImage();
		Gpu::ImageBarrier(swapchainImage, Gpu::Stage::PresentOld, Gpu::Stage::ColorAttachment);

		Render::BeginFrame();

		Render::DrawSprite(*ship.sprite, ship.pos, SpriteScale, 0.0f, Vec4(1.0f, 1.0f, 1.0f, 1.0f));

		//for (U64 i = 0; i < bullets.len; i++) {
		//	Render::DrawSprite(bulletSprite, bullets[i].pos);
		//}

		for (U64 i = 0; i < particleTypes.len; i++) {
			DrawParticleType(&particleTypes[i]);
		}

		Vec4 fillColor    = { 0.0f, 25.0f / 255.0f, 35.0f / 255.0f, 1.0f };
		Vec4 borderColor  = { 0.0f, 47.0f / 255.0f, 63.0f / 255.0f, 1.0f };
		F32  border       = 0.5f;
		F32  cornerRadius = 6.0f;
		Render::DrawRect({0.0f, 0.0f}, {200.0f, 80.0f}, fillColor, borderColor, border, cornerRadius);

		Render::EndFrame();

		Gpu::ImageBarrier(swapchainImage, Gpu::Stage::ColorAttachment, Gpu::Stage::Present);

		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static Game game;

App* GetGame() { return &game; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC