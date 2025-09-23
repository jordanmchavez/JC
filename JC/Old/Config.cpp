#pragma once

#include "JC/Config.h"
#include "JC/Hash.h"
#include "JC/Map.h"

namespace JC::Config {

//--------------------------------------------------------------------------------------------------

struct Val {
	U64 u;
	F64 f;
	Str s;
};

static Allocator*    allocator;
static Map<Str, Val> vals;

//--------------------------------------------------------------------------------------------------

void Init(Allocator* allocatorIn) {
	allocator = allocatorIn;
	vals.Init(allocatorIn);
}

//--------------------------------------------------------------------------------------------------

U32  GetU32 (Str name, U32  def) { Val* val = vals.FindOrNull(name); return val ? (U32)val->u : def; }
U64  GetU64 (Str name, U64  def) { Val* val = vals.FindOrNull(name); return val ? val->u : def; }
F32  GetF32 (Str name, F32  def) { Val* val = vals.FindOrNull(name); return val ? (F32)val->f : def; }
F64  GetF64 (Str name, F64  def) { Val* val = vals.FindOrNull(name); return val ? val->f : def; }
Bool GetBool(Str name, Bool def) { Val* val = vals.FindOrNull(name); return val ? (Bool)val->u : def; }
Str  GetStr (Str name, Str  def) { Val* val = vals.FindOrNull(name); return val ? val->s : def; }

void SetU32 (Str name, U32  u) { vals.Put(name, Val { .u = (U64)u }); }
void SetU64 (Str name, U64  u) { vals.Put(name, Val { .u = u }); }
void SetF32 (Str name, F32  f) { vals.Put(name, Val { .f = (F64)f }); }
void SetF64 (Str name, F64  f) { vals.Put(name, Val { .f = f }); }
void SetBool(Str name, Bool b) { vals.Put(name, Val { .u = (U64)b }); }
void SetStr (Str name, Str  s) {
	Val* val = vals.FindOrNull(name);
	if (val) {
		allocator->Free((void*)val->s.data, val->s.len);
		val->s = Copy(allocator, s);
	} else {
		vals.Put(name, Val { .s = Copy(allocator, s) });
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Config