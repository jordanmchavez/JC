#include "JC/Time.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

namespace JC::Time {

//--------------------------------------------------------------------------------------------------

static double ticksPerDay;
static double ticksPerHour;
static double ticksPerMin;
static double ticksPerSec;
static double ticksPerMil;

//--------------------------------------------------------------------------------------------------

void Init() {
	LARGE_INTEGER freq = {};
	QueryPerformanceFrequency(&freq);

	ticksPerDay  = (double)freq.QuadPart * 60.0 * 60.0 * 24.0;
	ticksPerHour = (double)freq.QuadPart * 60.0 * 60.0;
	ticksPerMin  = (double)freq.QuadPart * 60.0;
	ticksPerSec  = (double)freq.QuadPart;
	ticksPerMil  = (double)freq.QuadPart / 1000.0;
}

//--------------------------------------------------------------------------------------------------

U64 Now() {
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return (U64)count.QuadPart;
}

//--------------------------------------------------------------------------------------------------

double Days (U64 ticks) { return (double)ticks / ticksPerDay;  }
double Hours(U64 ticks) { return (double)ticks / ticksPerHour; }
double Mins (U64 ticks) { return (double)ticks / ticksPerMin;  }
double Secs (U64 ticks) { return (double)ticks / ticksPerSec;  }
double Mils (U64 ticks) { return (double)ticks / ticksPerMil;  }

U64 FromDays (double days)  { return (U64)(days  * ticksPerDay ); }
U64 FromHours(double hours) { return (U64)(hours * ticksPerHour); }
U64 FromMins (double mins)  { return (U64)(mins  * ticksPerMin ); }
U64 FromSecs (double secs)  { return (U64)(secs  * ticksPerSec ); }
U64 FromMils (double mils)  { return (U64)(mils  * ticksPerMil ); }

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

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Time