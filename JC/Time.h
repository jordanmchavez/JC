#pragma once

#include "JC/Common_Std.h"

namespace JC::Time {

//--------------------------------------------------------------------------------------------------

void Init();

U64 Now();

F64 Days (U64 ticks);
F64 Hours(U64 ticks);
F64 Mins (U64 ticks);
F64 Secs (U64 ticks);
F64 Mils (U64 ticks);

U64 FromDays (F64 days);
U64 FromHours(F64 hours);
U64 FromMins (F64 mins);
U64 FromSecs (F64 secs);
U64 FromMils (F64 mils);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Time