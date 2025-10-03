	#define JC_BUILTIN_IS_ENUM(T) __is_enum(T)


constexpr U64 KB = 1024;
constexpr U64 MB = 1024 * KB;
constexpr U64 GB = 1024 * MB;
constexpr U64 TB = 1024 * GB;

//--------------------------------------------------------------------------------------------------


Allocator*     InitAllocator(U64 commitSize, U64 reserveSize);
TempAllocator* InitTempAllocator(U64 reserveSize);

//--------------------------------------------------------------------------------------------------

struct Rect { I32 x, y; U32 w, h; };
struct Vec2 { F32 x, y; };
struct Vec3 { F32 x, y, z; };
struct Vec4 { F32 x, y, z, w; };
struct Mat2 { F32 m[2][2]; };
struct Mat3 { F32 m[3][3]; };
struct Mat4 { F32 m[4][4]; };

//--------------------------------------------------------------------------------------------------



//--------------------------------------------------------------------------------------------------


#define JC_DEFER \
	auto JC_MACRO_UNIQUE_NAME(Defer_) = DeferHelper() + [&]()

template <class F> struct DeferInvoker {
	F fn;
	DeferInvoker(F&& fn_) : fn(fn_) {}
	~DeferInvoker() { fn(); }
};

enum struct DeferHelper {};
template <class F> DeferInvoker<F> operator+(DeferHelper, F&& fn) { return DeferInvoker<F>((F&&)fn); }

//--------------------------------------------------------------------------------------------------

template <class T> constexpr T Min(T x, T y) { return x < y ? x : y; }
template <class T> constexpr T Max(T x, T y) { return x > y ? x : y; }
template <class T> constexpr T Clamp(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }

//--------------------------------------------------------------------------------------------------

template <class T> struct Array;
template <class K, class V> struct Map;

//--------------------------------------------------------------------------------------------------

void* Copy(Allocator* allocator, const void* p, U64 len);

inline Str Copy(Allocator* allocator, Str s) { return Str((char*)Copy(allocator, s.data, s.len), s.len); }

template <class T> Span<T> Copy(Allocator* allocator, Span<const T> s) {
	return Span<T>(Copy(s.data, s.len * sizeof(T)), s.len);
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC