#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res;

template <> struct [[nodiscard]] Res<void> {
	Err err = {};

	constexpr Res() = default;
	constexpr Res(Err e) { err = e; }	// implicit
	constexpr Res(const Res<>&) = default;
	constexpr operator bool() const { return !err.data; }
};

template <class T> struct [[nodiscard]] Res {
	union {
		T   val;
		Err err;
	};
	bool hasVal = false;

	constexpr Res() = default;
	constexpr Res(T v)   { val = v; hasVal = true;  }	// implicit
	constexpr Res(Err e) { err = e; hasVal = false; }	// implicit
	constexpr Res(const Res<T>&) = default;
	constexpr operator bool() const { return hasVal; }
	constexpr Res<> To(T& out) { if (hasVal) { out = val; return Res<>{}; } return Res<>(err); }
	constexpr T Or(T def) { return hasVal ? val : def; }
};

constexpr Res<> Ok() { return Res<>(); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC