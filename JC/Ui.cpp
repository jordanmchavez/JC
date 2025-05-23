#include "JC/Ui.h"

#include "JC/Array.h"
#include "JC/Log.h"
#include "JC/Render.h"

namespace JC::Ui {

//--------------------------------------------------------------------------------------------------

struct Box {
	SizeType sizeType[2];
	Vec2     size;
	U16      next;

	Vec2    computedPos;
	Vec2    computedSize;
};

static Mem::Allocator*     allocator;
static Mem::TempAllocator* tempAllocator;
static Log::Logger*        logger;
static Array<Box>          boxes;
static U16                 freeBoxIdx;

//--------------------------------------------------------------------------------------------------

void Init(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn) {
	allocator     = allocatorIn;
	tempAllocator = tempAllocatorIn;
	logger        = loggerIn;
	boxes.Init(allocator);
	boxes.Add();	// reserve index zero
	freeBoxIdx = 0;
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
}

//--------------------------------------------------------------------------------------------------

void Clear() {
	boxes.Clear();
	freeBoxIdx = 0;
}

//--------------------------------------------------------------------------------------------------

Res<> Events(Span<Event::Event> events) {
	events; return Ok();
}

//--------------------------------------------------------------------------------------------------

static U16 AllocBox() {
	if (freeBoxIdx) {
		U16 i = freeBoxIdx;
		freeBoxIdx = boxes[freeBoxIdx].next;
		return i;
	} else {
		Box* box = boxes.Add();
		return box - boxes.data;
	}
}

static void FreeBox(U16 i) {
	boxes[i].next = freeBoxIdx;
	freeBoxIdx = i;
}

//--------------------------------------------------------------------------------------------------

void MakeBox(SizeType sizeTypeX, SizeType sizeTypeY, Vec2 size, F32 parentPct, Vec4 color) {
	U16 i = AllocBox();
	Box* box = &boxes[i];
}

//--------------------------------------------------------------------------------------------------

void Draw() {
}

//--------------------------------------------------------------------------------------------------

}	// JC::Ui