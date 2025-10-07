#include "JC/Event.h"

namespace JC::Event {

//----------------------------------------------------------------------------------------------

static constexpr U32 MaxEvents = 1024;

static Event events[MaxEvents];
static U32   head;
static U32   tail;

//----------------------------------------------------------------------------------------------

void Add(Event e) {
	U32 const nextHead = (head + 1) & (MaxEvents - 1);
	if (nextHead == tail) {
		// TODO: report error
		tail = (tail + 1) & (MaxEvents - 1);
	}
	head = nextHead;
	events[head] = e;
}

//----------------------------------------------------------------------------------------------

Event const* Get() {
	if (head == tail) {
		return 0;
	}
	Event const* event = &events[tail];
	tail = (tail + 1) & (MaxEvents - 1);
	return event;
}

//----------------------------------------------------------------------------------------------

}	// namespace JC::Event