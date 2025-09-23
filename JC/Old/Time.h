#pragma once

#include "JC/Core.h"

namespace JC::Time {

//--------------------------------------------------------------------------------------------------

void Init();

U64  Now();

double Days (U64 ticks);
double Hours(U64 ticks);
double Mins (U64 ticks);
double Secs (U64 ticks);
double Mils (U64 ticks);

U64 FromDays (double days);
U64 FromHours(double hours);
U64 FromMins (double mins);
U64 FromSecs (double secs);
U64 FromMils (double mils);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Time