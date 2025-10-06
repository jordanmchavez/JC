#pragma once

//--------------------------------------------------------------------------------------------------

#define BreakOnError

//--------------------------------------------------------------------------------------------------

#if defined Compiler_Msvc
#endif	// Compiler_Msvc

constexpr U32 U32Max = 0xffffffff;
constexpr U64 U64Max = (U64)0xffffffffffffffff;


constexpr U64 KB = 1024;
constexpr U64 MB = 1024 * KB;
constexpr U64 GB = 1024 * MB;
constexpr U64 TB = 1024 * GB;

struct Mem;

template <class T> struct Array;
template <class K, class V> struct Map;

template <class T> constexpr T Min(T x, T y) { return x < y ? x : y; }
template <class T> constexpr T Max(T x, T y) { return x > y ? x : y; }
template <class T> constexpr T Clamp(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

//--------------------------------------------------------------------------------------------------

struct [[nodiscard]] Err { U64 handle = 0; };

void Err_AddArg(Err err, Str name, Arg arg);

template <class A1, class... A> void Err_AddArgs(Err err, Str name1, A1 arg1, A... args) {
	Err_AddArg(err, name1, Arg_Make(arg1));
	if constexpr (sizeof...(A) > 0) {
		Err_AddArgs(err, args...);
	}
}

Err Err_Make(Err prev, SrcLoc sl, Str ns, Str code);
Err Err_Make(Err prev, SrcLoc sl, Str ns, U64 code);

template <class... A> Err Err_Make(Err prev, SrcLoc sl, Str ns, Str code, A... args) {
	Err err = Err_Make(prev, sl, ns, code);
	Err_AddArgs(err, args...);
	return err;
}

template <class... A> Err Err_Make(Err prev, SrcLoc sl, Str ns, U64 code, A... args) {
	Err err = Err_Make(prev, sl, ns, code);
	Err_AddArgs(err, args...);
	return err;
}

struct Err_Code {
	Str const ns;
	Str const code;

                                                                                              Err operator()(                                                                                                                        SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code); }
	template <class A1                                                                      > Err operator()(Str n1, A1 a1,                                                                                                          SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1); }
	template <class A1, class A2                                                            > Err operator()(Str n1, A1 a1, Str n2, A2 a2,                                                                                           SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2); }
	template <class A1, class A2, class A3                                                  > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3,                                                                            SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3); }
	template <class A1, class A2, class A3, class A4                                        > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4,                                                             SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4); }
	template <class A1, class A2, class A3, class A4, class A5                              > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5,                                              SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5); }
	template <class A1, class A2, class A3, class A4, class A5, class A6                    > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6,                               SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7          > Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7,                SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6, n7, a7); }
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8> Err operator()(Str n1, A1 a1, Str n2, A2 a2, Str n3, A3 a3, Str n4, A4 a4, Str n5, A5 a5, Str n6, A6 a6, Str n7, A7 a7, Str n8, A8 a8, SrcLoc sl = SrcLoc_Here()) const { return Err_Make(Err(), sl, ns, code, n1, a1, n2, a2, n3, a3, n4, a4, n5, a5, n6, a6, n7, a7, n8, a8); }
};

#define Err_Def(Ns_, Code_) constexpr Err_Code Err_##Code_ = { .ns = #Ns_, .code = #Code_ }

//--------------------------------------------------------------------------------------------------

template <class T = void> struct [[nodiscard]] Res;

template <> struct [[nodiscard]] Res<void> {
	Err err;

	constexpr Res() = default;
	constexpr Res(Err e) { err = e; }	// implicit
	constexpr Res(const Res<>&) = default;
	constexpr operator Bool() const { return !err.handle; }
};

static_assert(sizeof(Res<>) == 8);

template <class T> struct [[nodiscard]] Res {
	union {
		T   val;
		Err err;
	};
	Bool hasVal = false;

	constexpr Res() = default;
	constexpr Res(T v) { val = v; hasVal = true;  }	// implicit
	constexpr Res(Err e) { err = e; hasVal = false; }	// implicit
	constexpr Res(const Res<T>&) = default;
	constexpr operator Bool() const { return hasVal; }
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

struct IRect { I32 x, y; U32 width, height; };
struct Vec2 { F32 x, y; };
struct Vec3 { F32 x, y, z; };
struct Vec4 { F32 x, y, z, w; };
struct Mat2 { F32 m[2][2]; };
struct Mat3 { F32 m[3][3]; };
struct Mat4 { F32 m[4][4]; };