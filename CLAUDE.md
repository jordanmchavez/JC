## Project
A C++ game engine and game (working title "JC"). Currently building toward a 4X/trading/mercenary roguelike with hex-grid combat (see `Notes/Game Design.md`). The engine targets Windows + Vulkan.

## Build
- **IDE:** Visual Studio 2022, project file `JC.vcxproj`
- **Standard:** C++20, no exceptions, no RTTI
- **Run tests:** `jc.exe test` (passes `"test"` arg to `Main.cpp`, which routes to unit tests)
- **Output:** `Build\x64-Debug\` or `Build\x64-Release\`
- **Key dependencies (in `3rd/`):** Vulkan SDK, spirv-reflect, stb_image, dragonbox

## Architecture
**Startup flow** (`App::Run` in `App.cpp`):
1. Reserve virtual memory (16 GB perm + 16 GB temp)
2. Init subsystems in order: `Time` → `StrDb` → `Cfg` → `Log` → `App::PreInit` → `Window` → `Gpu` → `Draw` → `App::Init`
3. Main loop: reset temp memory → input → `App::Update` → `App::Draw` → GPU present

**Subsystem layers:**
- **Infrastructure:** `Common.h` (types, errors, strings), `Common_Mem/Err/Fmt.cpp`, `Log`, `Sys`, `Time`
- **Platform:** `Window_Win.cpp`, `Gpu_Vk.cpp`, `Gpu_Vk_Util.cpp`
- **Engine:** `Draw.cpp` (2D sprite/canvas), `Event.cpp` (input queue), `Cfg.h` (config/cvars)
- **Game:** `App.h/.cpp` (framework), `App_Shooter.cpp` (sample), `Main.cpp` (entry point)
- **Data structures:** `Array.h`, `HandlePool.h`, `Map.h`, `Hash.cpp`, `Json.h/.cpp`, `FS.h/.cpp`

**Graphics:** Vulkan with bindless descriptors. Resource pools: 4096 buffers, 4096 images, 1024 shaders, 128 pipelines. All GPU resources are handle-based with generation counts (see `HandlePool.h`).

**2D Drawing:** `Draw.cpp` provides a canvas abstraction, sprite atlasing with name lookup, and deferred `DrawCmd` submission.

## Coding Conventions
- Headers are not allowed to include other headers, with the exception of "JC/Common.h", which is included by every header.
**Naming:**
- Namespaces: `JC::SubSystem` (e.g., `JC::Window`, `JC::Gpu`)
- Types, functions: `PascalCase`
- Variables: `camelCase`
- Macros: `SCREAMING_SNAKE_CASE`
- Primitive type aliases: `U8`, `U32`, `I64`, `F32`, `F64`, etc.
- Error codes: `Err_Name`
- Config keys: `Cfg_DomainItem`

**Memory:**
- Two allocators: `permMem` (permanent) and `tempMem` (reset each frame)
- Linear allocators only — no `new`/`delete`, no `malloc`/`free`
- Use `MemScope` / `Mem::Mark` + `Mem::Reset` for scoped temp allocations
- Last allocation can be extended via `Mem::Extend()`

**Error handling:**
- `Res<T>` result type (monadic). Functions that can fail return `Res<T>` or `Res<void>`.
- `Try(expr)` macro propagates errors up the call stack
- `TryTo(expr, var)` extracts value from `Res<T>` or propagates
- Define error codes with `DefErr(Namespace, Code)`
- No exceptions anywhere

**Strings:**
- `Str` = pointer + length (not null-terminated, not `std::string`)
- `SPrintf()` family with compile-time format string validation
- Custom specifiers: `%t` = bool, `%b` = binary, `%a` = auto

**Patterns:**
- `Defer { ... }` for scoped cleanup (like Go's defer)
- Subsystems receive their allocators explicitly at init — no globals
- `Log::AddFn()` for callback-based log sinks (max 32)

## Philosophy (from `Notes/Guidebook.md`)
- **Easiness to change** is the top priority — minimize hidden behavior, make complexity visible
- **Less is more** — code is a liability; delete unused code; don't abstract until the value is obvious
- **Explicit > implicit** — slow/expensive operations should look slow/expensive at the call site
- **Fast iteration** — minimize headers, avoid code-in-headers, keep build times short
- **Abstraction value = surface area / volume** — the interface should be small relative to the implementation it hides
