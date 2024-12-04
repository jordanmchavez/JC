#pragma once

#include "JC/Common.h"

namespace JC {

struct Log;
struct TempMem;

//--------------------------------------------------------------------------------------------------

enum struct EventType {
	Exit,
	Focus,
};

struct FocusEvent {
	bool focused;
};

struct Event {
	EventType type;
	union {
		FocusEvent focus;
	};
};

struct EventApi {
	virtual void        Init(Log* log, TempMem* tempMem) = 0;
	virtual void        AddEvent(Event e) = 0;
	virtual Span<Event> GetEvents() = 0;
	virtual void        ClearEvents();
};

EventApi* GetEventApi();

//--------------------------------------------------------------------------------------------------

}	// namespace JC