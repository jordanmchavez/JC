#pragma once

#include "JC/Common.h"

struct Arena;

Arena* Arena_Create();
void*  Arena_Alloc(Arena* a, U64 size);
U64    Arena_Mark(Arena* a);
void   Arena_Reset(Arena* a);
void   Arena_ResetTo(Arena* a, U64 mark);
void   Arena_Destroy(Arena* a);