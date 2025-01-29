#pragma once

#include "JC/Core.h"

namespace JC::Time {

//--------------------------------------------------------------------------------------------------

void Init();

u64  Now();

double Days (u64 ticks);
double Hours(u64 ticks);
double Mins (u64 ticks);
double Secs (u64 ticks);
double Mils (u64 ticks);

u64 FromDays (double days);
u64 FromHours(double hours);
u64 FromMins (double mins);
u64 FromSecs (double secs);
u64 FromMils (double mils);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Time