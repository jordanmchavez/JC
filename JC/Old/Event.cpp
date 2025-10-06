#include "JC/Event.h"

//----------------------------------------------------------------------------------------------

static constexpr U32 Ev_MaxEvents = 1024;

static Ev_Event ev_events[Ev_MaxEvents];
static U32      ev_head;
static U32      ev_tail;

//----------------------------------------------------------------------------------------------

void Ev_Add(Ev_Event e) {
	const U32 nextHead = (ev_head + 1) & (Ev_MaxEvents - 1);
	if (nextHead == ev_tail) {
		ev_tail = (ev_tail + 1) & (Ev_MaxEvents - 1);
	}
	ev_head = nextHead;
	ev_events[ev_head] = e;
}

//----------------------------------------------------------------------------------------------

Ev_Event const* Ev_Get() {
	if (ev_head == ev_tail) {
		return 0;
	}
	Ev_Event const* event = &ev_events[ev_tail];
	ev_tail = (ev_tail + 1) & (Ev_MaxEvents - 1);
	return event;
}