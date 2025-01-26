#pragma once

#include "JC/Common.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res;

template <> struct [[nodiscard]] Res<void> {
	Err err = {};

	constexpr Res() = default;
	constexpr Res(Err e) { err = e; }	// implicit
	template <class T> constexpr Res(const Res<T>& r) {
		if constexpr (IsSameType<T, void>) {
			err = r.err;
		} else {
			Assert(!r.hasVal);
			err = r.err;
		}
	}

	constexpr operator bool() const { return err.data; }

	constexpr void Ignore() const {}
};

template <class T> struct [[nodiscard]] Res {
	union {
		T   val;
		Err err;
	};
	bool hasVal = false;

	constexpr Res()      {          hasVal = false; }
	constexpr Res(T v)   { val = v; hasVal = true;  }	// implicit
	constexpr Res(Err e) { err = e; hasVal = false; }	// implicit
	template <class U> constexpr Res(const Res<U>& r) {
		if constexpr (IsSameType<U, T>) {
			hasVal = r.hasVal;
			if (r.hasVal) {
				val = r.val;
			} else {
				err = r.err;
			}
		} else if constexpr (IsSameType<U, void>) {
			Assert(r.err.handle);
			err = r.err;
			hasVal = false;
			
		} else {
			Assert(!r.hasVal);
			err = r.err;
			hasVal = false;
		}
	}

	constexpr operator bool() const { return hasVal; }

	constexpr Res<> To(T& out) { if (hasVal) { out = val; return Res<>{}; } return Res<>(err); }
	constexpr T     Or(T def) { return hasVal ? val : def; }

	constexpr void Ignore() const {}
};

constexpr Res<> Ok() { return Res<>(); }

//--------------------------------------------------------------------------------------------------

}	// namespace JC