#include "JC/Event.h"

namespace JC::Event {

//----------------------------------------------------------------------------------------------

static constexpr U32 MaxEvents = 1024;

static Event events[MaxEvents];
static U32   head;
static U32   tail;

//----------------------------------------------------------------------------------------------

void AddEvent(Event event) {
	U32 const nextHead = (head + 1) & (MaxEvents - 1);
	if (nextHead == tail) {
		// TODO: report error
		tail = (tail + 1) & (MaxEvents - 1);
	}
	head = nextHead;
	events[head] = event;
}

//----------------------------------------------------------------------------------------------

// TODO: consider just returning Span<Event const>
bool GetEvent(Event* eventOut) {
	if (head == tail) {
		return false;
	}
	*eventOut = events[tail];
	tail = (tail + 1) & (MaxEvents - 1);
	return true;
}

//----------------------------------------------------------------------------------------------

}	// namespace JC::Event