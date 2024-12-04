#include "JC/Event.h"

#include "JC/Fmt.h"
#include "JC/Log.h"
#include "JC/Mem.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct EventApiObj : EventApi {
	static constexpr u32 MaxEvents = 64 * 1024;

	Log*     log               = 0;
	TempMem* tempMem           = 0;
	Event    events[MaxEvents] = {};
	u32      eventsLen         = 0;

	void Init(Log* log_, TempMem* tempMem_) override{
		log       = log_;
		tempMem   = tempMem_;
		eventsLen = 0;
	}

	s8 EventStr(Event e) {
		switch (e.type) {
			case EventType::Exit:  return Fmt(tempMem, "ExitEvent");
			case EventType::Focus: return Fmt(tempMem, "FocusEvent(focused={})", e.focus.focused);
			default:               Panic("Unhandled EventType {}", e.type);
		}
	}

	void AddEvent(Event e) override{
		if (eventsLen >= MaxEvents) {
			LogErrorf("Dropped event: {}", EventStr(e));
			return;
		}
		events[eventsLen] = e;
		eventsLen++;
	}

	Span<Event> GetEvents() override {
		return Span<Event>(events, eventsLen);
	}

	void ClearEvents() override {
		eventsLen = 0;
	}
};

static EventApiObj eventApi;

EventApi* GetEventApi() {
	return &eventApi;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC