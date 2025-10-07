#pragma once

#include "JC/Common_Err.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res;

template <> struct [[nodiscard]] Res<void> {
	Err::Err const* err;

	constexpr Res() = default;
	constexpr Res(Err::Err const* e) { err = e; }	// implicit
	constexpr Res(const Res<>&) = default;
	constexpr operator bool() const { return !err; }
};

static_assert(sizeof(Res<>) == 8);

template <class T> struct [[nodiscard]] Res {
	union {
		T               val;
		Err::Err const* err;
	};
	bool hasVal = false;

	constexpr Res() = default;
	constexpr Res(T v) { val = v; hasVal = true;  }	// implicit
	constexpr Res(Err::Err const* e) { err = e; hasVal = false; }	// implicit
	constexpr Res(const Res<T>&) = default;
	constexpr operator bool() const { return hasVal; }
	constexpr Res<> To(T& out) { if (hasVal) { out = val; return Res<>{}; } return Res<>(err); }
	constexpr T Or(T def) { return hasVal ? val : def; }
};

constexpr Res<> Ok() { return Res<>(); }

#define Try(Expr) \
	do { if (Res<> r = (Expr); !r) { \
		return r.err; \
	} } while (false)

#define TryTo(Expr, Out) \
	do { if (Res<> r = (Expr).To(Out); !r) { \
		return r.err; \
	} } while (false)

//--------------------------------------------------------------------------------------------------

}	// namespace JC