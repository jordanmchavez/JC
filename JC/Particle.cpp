#include "JC/Particle.h"

#include "JC/Array.h"
#include "JC/Draw.h"
#include "JC/Math.h"
#include "JC/Rng.h"
#include "JC/Time.h"

namespace JC::Particle {

//--------------------------------------------------------------------------------------------------

static constexpr U32  MaxEmitters = 1024;
static constexpr U32  MaxTypes    = 64;

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

struct Type {
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

enum struct Shape {
	Invalid = 0,
	Rectangle,
	Ellipse,
	Diamond,
	Line,
};

enum struct Distribution {
	Invalid = 0,
	Linear,
	Gauss,
	InvGauss,
};

struct Emitter {
	Type*        type = 0;
	Vec2         pos;
	Shape        shape = Shape::Invalid;
	F32          minX = 0.0f;
	F32          maxX = 0.0f;
	F32          minY = 0.0f;
	F32          maxY = 0.0f;
	Distribution dist = Distribution::Invalid;
	F32          emitRate = 0.0f;
	F32          emitAccum = 0.0f;
};

static Array<Type>    types;
static Array<Emitter> emitters;

//--------------------------------------------------------------------------------------------------

static void UpdateParticleType(F32 sec, Type* type) {
	for (U64 i = 0; i < type->particles.len;) {
		Particle* const p = &type->particles[i];
		if (p->life += sec; p->life >= p->lifeEnd) {
			type->particles.RemoveUnordered(i);
			continue;
		}
		p->size     += sec * type->scaleInc;
		p->speed    += sec * type->speedInc;
		p->angle    += sec * type->angleInc;
		p->rotation += sec * type->rotationInc ;
		p->pos.x    += sec * p->speed * Math::Cos(Math::DegToRad(p->angle));
		p->pos.y    += sec * p->speed * Math::Sin(Math::DegToRad(p->angle));
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

static void UpdateParticleEmitter(F32 sec, Emitter* emitter) {
	Type* const t = emitter->type;
	emitter->emitAccum += emitter->emitRate * sec;
	while (emitter->emitAccum >= 1.0f) {
		Particle* const p = t->particles.Add();
		switch (emitter->shape) {
			case Shape::Rectangle: {
				p->pos.x = emitter->pos.x + Math::Lerp(emitter->minX, emitter->maxX, Rng::NextF32()) - (t->spriteSize.x / 2) + 0.5f;
				p->pos.y = emitter->pos.y + Math::Lerp(emitter->minY, emitter->maxY, Rng::NextF32()) - (t->spriteSize.y / 2) + 0.5f;

				break;
			}
			case Shape::Ellipse: {
				break;
			}
			case Shape::Diamond: {
				break;
			}
			case Shape::Line: {
				break;
			}
			default: Panic("Unhandled Shape %u", (U32)emitter->shape);
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

void Update(F32 sec) {
	for (U64 i = 0; i < types.len; i++) {
		UpdateParticleType(sec, &types[i]);
	}

	for (U64 i = 0; i < emitters.len; i++) {
		UpdateParticleEmitter(sec, &emitters[i]);
	}
}

//---------------------------------------------------------------------------------------------

void Draw() {
	for (U64 i = 0; i < types.len; i++) {
		Type const* const type = &types[i];
		for (U64 j = 0; j < type->particles.len; i++) {
			Particle const* const p = &type->particles[j];
			Draw::DrawSprite({
				.sprite   = type->sprite,
				.pos      = p->pos,
				.scale    = Vec2(p->size, p->size),
				.color    = p->color,
			});
		}
	}
}

//---------------------------------------------------------------------------------------------

}	// namespace JC::Particle