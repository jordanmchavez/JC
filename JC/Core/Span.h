#pragma once

#include "JC/Core.h"

//--------------------------------------------------------------------------------------------------

namespace std {
	template <class T> struct initializer_list {
		const T* _begin = 0;
		const T* _end   = 0;

		constexpr initializer_list() = default;
		constexpr initializer_list(const T* b, const T* e) { _begin = b; _end = e; }
		constexpr const T* begin() const { return _begin; }
		constexpr const T* end() const { return _end; }
		constexpr size_t size() const { return _end - _begin; }
	};
} namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T> struct Span {
	T*  data = 0;
	U64 len  = 0;

	constexpr Span() = default;
	constexpr Span(T* d, U64 l) { data = d; len = l; }
	constexpr Span(std::initializer_list<T> il) { data = const_cast<T*>(il.begin()); len = il.size(); }
	constexpr Span(const Span&) = default;

	constexpr Span& operator=(const Span&) = default;
	
	constexpr       T& operator[](U64 i)       { return data[i]; }
	constexpr const T& operator[](U64 i) const { return data[i]; }

	constexpr       T* Begin()       { return data; }
	constexpr const T* Begin() const { return data; }
	constexpr       T* End()         { return data + len; }
	constexpr const T* End()   const { return data + len; }
};

//--------------------------------------------------------------------------------------------------

}	// namespace JC