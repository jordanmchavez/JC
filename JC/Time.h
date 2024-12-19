#pragma once

#include "JC/Common.h"

namespace JC::Tm {

//--------------------------------------------------------------------------------------------------

// 1 Tick == 100 nanoseconds, matching the window API *EXCEPT*:
// Time stores the number of ticks since 1/1/0001 UTC
// 1 Tick = 100 * 10^-9 = 10^-7 Sec

static constexpr u64 TicksPerMil  = 10000;	// 10^-3 / 10^-7 = 10^4
static constexpr u64 TicksPerSec  = TicksPerMil  * 1000;
static constexpr u64 TicksPerMin  = TicksPerSec  *   60;
static constexpr u64 TicksPerHour = TicksPerMin  *   60;
static constexpr u64 TicksPerDay  = TicksPerHour *   24;

constexpr bool Eq       (Time t1, Time t2) { return t1.ticks == t2.ticks; }
constexpr bool NotEq    (Time t1, Time t2) { return t1.ticks != t2.ticks; }
constexpr bool Less     (Time t1, Time t2) { return t1.ticks  < t2.ticks; }
constexpr bool LessEq   (Time t1, Time t2) { return t1.ticks <= t2.ticks; }
constexpr bool Greater  (Time t1, Time t2) { return t1.ticks  > t2.ticks; }
constexpr bool GreaterEq(Time t1, Time t2) { return t1.ticks >= t2.ticks; }

constexpr bool Eq       (Dur d1, Dur d2) { return d1.ticks == d2.ticks; }
constexpr bool NotEq    (Dur d1, Dur d2) { return d1.ticks != d2.ticks; }
constexpr bool Less     (Dur d1, Dur d2) { return d1.ticks  < d2.ticks; }
constexpr bool LessEq   (Dur d1, Dur d2) { return d1.ticks <= d2.ticks; }
constexpr bool Greater  (Dur d1, Dur d2) { return d1.ticks  > d2.ticks; }
constexpr bool GreaterEq(Dur d1, Dur d2) { return d1.ticks >= d2.ticks; }

constexpr Time Add(Time t,  Dur  d)  { return Time { .ticks =       t.ticks  + d.ticks  }; }
constexpr Time Add(Dur  d,  Time t)  { return Time { .ticks =       d.ticks  + t.ticks  }; }
constexpr Dur  Add(Dur  d1, Dur  d2) { return Dur  { .ticks =       d1.ticks + d2.ticks }; }

constexpr Dur  Sub(Dur  d1, Dur  d2) { return Dur  { .ticks =       d1.ticks - d2.ticks }; }
constexpr Time Sub(Time t,  Dur  d)  { return Time { .ticks =       t.ticks  - d.ticks  }; }
constexpr Dur  Sub(Time t1, Time t2) { return Dur  { .ticks =       t1.ticks - t2.ticks }; }

constexpr Dur  Mul(Dur  d,  u32  s)  { return Dur  { .ticks =       d.ticks  * s        }; }
constexpr Dur  Mul(Dur  d,  u64  s)  { return Dur  { .ticks =       d.ticks  * s        }; }
constexpr Dur  Mul(Dur  d,  f32  s)  { return Dur  { .ticks = (u64)(d.ticks  * s)       }; }
constexpr Dur  Mul(Dur  d,  f64  s)  { return Dur  { .ticks = (u64)(d.ticks  * s)       }; }
constexpr Dur  Mul(u32  s,  Dur  d)  { return Dur  { .ticks =       s        * d.ticks  }; }
constexpr Dur  Mul(u64  s,  Dur  d)  { return Dur  { .ticks =       s        * d.ticks  }; }
constexpr Dur  Mul(f32  s,  Dur  d)  { return Dur  { .ticks = (u64)(s        * d.ticks) }; }
constexpr Dur  Mul(f64  s,  Dur  d)  { return Dur  { .ticks = (u64)(s        * d.ticks) }; }

constexpr u64 Days (Time t) { return t.ticks / TicksPerDay; }
constexpr u64 Hours(Time t) { return t.ticks / TicksPerHour; }
constexpr u64 Mins (Time t) { return t.ticks / TicksPerMin; }
constexpr u64 Secs (Time t) { return t.ticks / TicksPerSec; }
constexpr u64 Mils (Time t) { return t.ticks / TicksPerMil; }

constexpr Dur FromDays (u64 days ) { return Dur { .ticks = days  * TicksPerDay  }; }
constexpr Dur FromHours(u64 hours) { return Dur { .ticks = hours * TicksPerHour }; }
constexpr Dur FromMins (u64 mins ) { return Dur { .ticks = mins  * TicksPerMin  }; }
constexpr Dur FromSecs (u64 secs ) { return Dur { .ticks = secs  * TicksPerSec  }; }
constexpr Dur FromMils (u64 mils ) { return Dur { .ticks = mils  * TicksPerMil  }; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Tm