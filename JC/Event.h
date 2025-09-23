#pragma once

#include "JC/Common.h"

enum struct Ev_Type {
	Key,
	Mouse,
	WindowResize,
	ExitRequest,
};

struct Ev_Event {
	Ev_Type type;
};

enum struct Ev_Key {
};


static constexpr U32 Ev_MaxEvents = 1024;
static Ev_Event ev_events[Ev_MaxEvents];
static U32      ev_head;
static U32      ev_tail;

void Ev_Init() {
	
}

Span<const Ev_Event> Ev_Get() {
}

void Ev_Clear() {
	
}

void Ev_Add(const Event* e) {
	
}