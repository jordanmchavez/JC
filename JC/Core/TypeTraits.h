#pragma once

#include "JC/Core.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

template <class T>            struct                TypeIdentity               { using Type = T; };
template <class T>            struct                RemovePtr                  { using Type = T; };
template <class T>            struct                RemovePtr<T*>              { using Type = T; };
template <class T>            struct                RemoveRef                  { using Type = T; };
template <class T>            struct                RemoveRef<T&>              { using Type = T; };
template <class T>            struct                RemoveRef<T&&>             { using Type = T; };
template <class T>            struct                RemoveConst                { using Type = T; };
template <class T>            struct                RemoveConst<const T>       { using Type = T; };
template <class T>            struct                RemoveVolatile             { using Type = T; };
template <class T>            struct                RemoveVolatile<volatile T> { using Type = T; };
template <class T1, class T2> struct                IsSameTypeT                { static constexpr Bool Val = false; };
template <class T>            struct                IsSameTypeT<T, T>          { static constexpr Bool Val = true;  };
template <class T1, class T2> inline constexpr Bool IsSameType                 = IsSameTypeT<T1, T2>::Val;
template <class T>            struct                IsPointerT                 { static constexpr Bool Val = false; };
template <class T>            struct                IsPointerT<T*>             { static constexpr Bool Val = true; };
template <class T>            inline constexpr Bool IsPointer                  = IsPointerT<T>::Val;
template <class T>            inline constexpr Bool IsEnum                     = BuiltinIsEnum(T);
template <class...>           inline constexpr Bool AlwaysFalse                = false; 

//--------------------------------------------------------------------------------------------------

}	// namespace JC