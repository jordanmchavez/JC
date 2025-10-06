#pragma once

#include "JC/Common.h"

//--------------------------------------------------------------------------------------------------

void Time_Init();

U64 Time_Now();

F64 Time_Days (U64 ticks);
F64 Time_Hours(U64 ticks);
F64 Time_Mins (U64 ticks);
F64 Time_Secs (U64 ticks);
F64 Time_Mils (U64 ticks);

U64 Time_FromDays (F64 days);
U64 Time_FromHours(F64 hours);
U64 Time_FromMins (F64 mins);
U64 Time_FromSecs (F64 secs);
U64 Time_FromMils (F64 mils);