# Project
4x turn-based strategy game with roguelite meta-progression. Three gameplay layers: Universe (shard selection, meta), Shard (hex 4x map), Battle (army vs army). Currently implementing the Battle prototype.

- **Language/Platform:** C++20, Windows desktop, Vulkan, MSVC 2022 (`JC.vcxproj`)
- **Run tests:** `jc.exe test`
- **Design docs:** `Docs/4xGameDesign.md`, `Docs/Battle.md`
- **Coding principles:** `Docs/Guidebook.md` — read this first

# Coding Principles (from Guidebook.md)
- Simple, minimal, transparent, explicit — minimize cognitive load to read end-to-end
- Code is a liability: less is more. Solve only known use-cases.
- No destructors, copy constructors, or `operator=` — everything trivially copyable/POD (except a few C++ utilities like `Array`, `DeferInvoker`, `MemScope`)
- Arrays and hash tables are the only general data structures
- No standard C/C++ headers — everything is implemented in-house or forwarded manually
- **Headers may only `#include "JC/Common.h"`** — forward declare everything else
- Minimize code in headers (especially templates)
- Use `tempMem` arena for all allocations 1 frame or less (reset each frame), use permMem for everything permanent.
- Errors must be unignorable: use `Res<T>` / `Err` / `ErrCode` system
- `Assert(expr)`/`Panic(fmt, ...)` for invariants/sanity checks, NOT for error handling

# Key Types (Common.h)
- **Primitives:** `I8/I16/I32/I64`, `U8/U16/U32/U64`, `F32/F64`
- **`Str`** — non-owning string view `{ char const* data; U32 len; }`
- **`Span<T>`** — non-owning array view `{ T* data; U64 len; }`
- **`Array<T>`** — owning growable array backed by a `Mem` arena
- **`Mem`** — arena allocator handle. `Mem::AllocT<T>()`, `Mem::Mark()`/`Mem::Reset()` for scoped rollback
- **`MemScope`** — RAII arena rollback (the one RAII construct allowed)
- **`Handle<Tag>`** / `DefHandle(Type)` — typed opaque 64-bit handles (generation + index)
- **`HandlePool<T, H>`** — generational pool for handle-based object management
- **`Res<T>`** / **`Res<>`** — result type wrapping value or `Err const*`; `[[nodiscard]]`
- **`Err`** / **`ErrCode`** — structured error with source location and named args. Define with `DefErr(Ns, Code)`
- **`Try(expr)`** — propagates error up the call stack (like `?` in Rust)
- **`TryTo(expr, out)`** — like `Try` but extracts value into `out`
- **`HexPos { I32 c, r; }`** — axial hex coordinate
- **`SPrintf(mem, fmt, ...)`** — type-checked printf-style formatting into arena memory
- **`Logf(fmt, ...)` / `Errorf(...)` / `LogErr(err)`** — logging macros (source loc automatic)
- **`Defer { ... }`** — deferred execution lambda at scope exit

# Input System
- Create a `BindingSet`, bind `Key::Key` values to `U64` action IDs via `Input::Bind()`
- `frameData->actions` is a `Span<U64 const>` of triggered actions each frame
- Switch on action IDs in `Frame()`

# Claude
- For design and architecture questions, give detailed responses: named options with pros/cons, concrete code examples, and an explicit recommendation. Don't default to brevity on open-ended design questions.