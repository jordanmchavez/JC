#include "JC/Time.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

static F64 time_ticksPerDay;
static F64 time_ticksPerHour;
static F64 time_ticksPerMin;
static F64 time_ticksPerSec;
static F64 time_ticksPerMil;

//--------------------------------------------------------------------------------------------------

void Time_Init() {
	LARGE_INTEGER freq = {};
	QueryPerformanceFrequency(&freq);

	time_ticksPerDay  = (F64)freq.QuadPart * 60.0 * 60.0 * 24.0;
	time_ticksPerHour = (F64)freq.QuadPart * 60.0 * 60.0;
	time_ticksPerMin  = (F64)freq.QuadPart * 60.0;
	time_ticksPerSec  = (F64)freq.QuadPart;
	time_ticksPerMil  = (F64)freq.QuadPart / 1000.0;
}

//--------------------------------------------------------------------------------------------------

U64 Time_Now() {
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return (U64)count.QuadPart;
}

//--------------------------------------------------------------------------------------------------

F64 Time_Days (U64 ticks) { return (F64)ticks / time_ticksPerDay;  }
F64 Time_Hours(U64 ticks) { return (F64)ticks / time_ticksPerHour; }
F64 Time_Mins (U64 ticks) { return (F64)ticks / time_ticksPerMin;  }
F64 Time_Secs (U64 ticks) { return (F64)ticks / time_ticksPerSec;  }
F64 Time_Mils (U64 ticks) { return (F64)ticks / time_ticksPerMil;  }

U64 Time_FromDays (F64 days)  { return (U64)(days  * time_ticksPerDay ); }
U64 Time_FromHours(F64 hours) { return (U64)(hours * time_ticksPerHour); }
U64 Time_FromMins (F64 mins)  { return (U64)(mins  * time_ticksPerMin ); }
U64 Time_FromSecs (F64 secs)  { return (U64)(secs  * time_ticksPerSec ); }
U64 Time_FromMils (F64 mils)  { return (U64)(mils  * time_ticksPerMil ); }

//--------------------------------------------------------------------------------------------------

/*
constexpr i64 JulianDays(i64 d, i32 m, i64 y)
{
	// Fliegel, H. F. & van Flandern, T. C. 1968, Communications of the ACM, 11, 657.
	return day
		- 32075
		+ 1461 * (y + 4800 + (m - 14) / 12) / 4
		+ 367 * (m - 2 - (m - 14) / 12 * 12) / 12
		- 3 * ((y + 4900 + (m - 14) / 12) / 100) / 4		;
}

Date()
{
	// https://aa.usno.navy.mil/faq/JD_formula
	// Fliegel, H. F. & van Flandern, T. C. 1968, Communications of the ACM, 11, 657.
	// This is the same algorithm that UE's FDateTime uses

	const i64 jd = JulianDays(1, 1, 1) + static_cast<i64>(Ticks / TicksPerDay);
	i64 l = jd + 68569;
	i64 n = 4 * l / 146097;
		l = l - (146097 * n + 3) / 4;
	i64 i = 4000 * (l + 1) / 1461001;
		l = l - 1461 * i / 4 + 31;
	i64 j = 80 * l / 2447;
	i64 k = l - 2447 * j / 80;
		l = j / 11;
		j = j + 2 - 12 * l;
		i = 100 * (n - 49) + i + l;

	Year  = i;
	Month = j;
	Day   = k;
	Hour  = (Ticks / TicksPerHour) %   24;
	Min   = (Ticks / TicksPerMin ) %   60;
	Sec   = (Ticks / TicksPerSec ) %   60;
	Milli = (Ticks / TicksPerMil ) % 1000;
	return Date;
}
*/