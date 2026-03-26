update claude.md/guidebook.md
Desc -> Def

console / cvar system
view layout:


memtrace
vk allocation callbacks
console window for logging
log file
log pattern
do something with timestamp/file/line/system/thread
memory scopes
    [0] VK_EXT_debug_report, v9
    [1] VK_EXT_debug_utils, v1
    [2] VK_EXT_validation_features, v2
[0] VK_KHR_device_group_creation, v1
[1] VK_KHR_display, v23
[2] VK_KHR_external_fence_capabilities, v1
[3] VK_KHR_external_memory_capabilities, v1
[4] VK_KHR_external_semaphore_capabilities, v1
[5] VK_KHR_get_display_properties2, v1
[6] VK_KHR_get_physical_device_properties2, v2
[7] VK_KHR_get_surface_capabilities2, v1
[8] VK_KHR_surface, v25
[9] VK_KHR_surface_protected_capabilities, v1
[10] VK_KHR_win32_surface, v6
[11] VK_EXT_debug_report, v10
[12] VK_EXT_debug_utils, v2
[13] VK_EXT_direct_mode_display, v1
[14] VK_EXT_swapchain_colorspace, v4
[15] VK_NV_external_memory_capabilities, v1

# TODO
console / cvar system
file logging
allocator statistics
vulkan statistics

allocation telemtry: file/line/system, counts and maxes over frames
	use a separate heap for this
better solution for large temp allocs than static cap
	really? isn't having a fixed-size array a reasonable sanity check? perhaps exponentially increasing sizes?
	problem is it happening in a long-running game
log format string
log colors? def in window console, maybe not in system console
hashset and replace log map with set
app name J_AppName
reasoning for MaxAlign = 1mb
aligned realloc sucks
Str hash collisions
Research and consider VkValidationFeaturesEXT
Research and consider VK_EXT_debug_report
min/max for containers and allocators
queue labels in vulkan debug logging
vulkan label colors
error on zero vk devices
multiple physical vk devices: cull non-gpu, pick one
verify device extension VK_KHR_SWAPCHAIN_EXTENSION_NAME
VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT and vulkan protected memory in general
Array::Grow to template-less non-inline
if (surfCapabilities.currentExtent.width == 0xFFFFFFFF)
consistently use %u for unsigned printf
warning 4189: local var initialized but not referenced
understand VkCompositeAlphaFlagBitsKHR
why swapchain VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT and not stencil
vk identity swizzle vs rgba
log levels as bitmask, we don't really want vulkan info
use VK_QUEUE_FAMILY_IGNORED
vulkan message severity flags at debug messenger creation
truly minimal windows with only the shit we use
clang format
easy to bug: alloc sizeof() doesn't match final ptr...use a macro
engine/app names in gfx
explicit initialization
dedicate vk xfer queue?
optional stencil
flag this: .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
investigate dedicated transfer queues
offsetof custom
on viewport change, should we recreate the entire pipeline? or is it okay to use dynamic states?
swapChainImageCount -> cmdbufcnt
deal with VK_SUBOPTIMAL_KHR
vull vk list
support os swap mouse buttons: GetSystemMetrics(SM_SWAPBUTTON)
support WM_INPUT absolute mouse: http://www.petergiuntoli.com/parsing-wm_input-over-remote-desktop
input: make source copying more efficient
Consider whether expand is even worth it...it's two compares and a memory bump. Might be simpler and faster to just allocate a larger base chunk.
Log format flags: https://github.com/gabime/spdlog/wiki/3.-Custom-formatting








mem: return vmem to system by measuring high water marks for a few seconds
complete utf tests
fmt tests
	even rounding
	go through all code branch points
	sci vs fixed boundary cases
fix subset execution
memory assets should not be asserts: they should be hard checks
How to track memory?
Option 1: File/Line
All allocator funcs accept file/line args. Code needs to propagate file/line args up the call stack fromi the point where they'd be most "useful", which is usually the root initiator of the memory-allocating operation.
This is tricky because everything that can alloc must accept file/line: arrays, hash tables, strings, formatting. Esp crusty with variadic format funcs.
Option 2: Scoped Allocators
Lots of small "named" allocators that don't track file/line, but rather attach the scope name to the allocation.
Less detail than file/line, but simpler implementation
windows dark mode support: sdl WIN_UpdateDarkModeForHWND
testBuf can overflow
test reporters, to file
all uses of dbgPrint should probably not need to check dbg() first: just always do it
similarly, disable dbgBreak in shipping builds
extract the non-parameterized map code into .cpp
prefix/static fmt helper fns
MakeVArg -> Arg_Make?
mem_scratch should be in std.h and delete all references to include jc/mem.h
panic includes all err ctx info in log print
panic includes stack trace
replace win fiber with custom coro
our own atomic library
ensure all static data is initialized
panic/assert should debug break in code...maybe?
thread debug names: SetThreadDescription
task system for long-running background threads
profiler
UTF tests should have actual char/wchar strings converting
panic/assert fmt arg is an s8, not an FmtStr
if we're serious about multithreading, then we nee dto make *everything& thread safe: memory tracing, etc
memory report
focus click pending: user clicks out then back in, shouldn't send lmb event
stuff to consider when gaining/losing focus:
	last mouse pos (send mouse move event for new pos)
	don't send click event that activated us
	mouse movement / events while downclick hasn't been released
array tests, insert
window messages WM_ check which we should return 0 and which we should defer to defwindowproc
WM_MOUSEHWHEEL
address sanitizer
lint
clang format
other static analysis tools
move this lwoer and add assert	constexpr char operator[](U64 i) const { return ptr[i]; }
cursor clipping/locking to screen as an option but only while focused
wm_paint? is black brush sufficient?
wm_syscommand? ignore SC_KEYMENU messages?
fix dropped events on first frame
log scopes should have the same length to make log more readable
vulkan enumerate layer extensions, at least to see what we have
add vulkan memory allocator
better vulkan debug printing
add null checks to vk loader
move error decls to bottom of file
graphicsQueueFamily -> graphicsQueueFamilyIndex
test disable macro
temp mem stats
temp mem doubling chunk size up to some limit...also consider just reserving and committing pages
test tracing
see if we can get hash table sizing to not double: but may require power of two
track "want to extend last block but failed" frequency. it may make sense to specialize this path in the allocator
add temp memory high water mark tracking and freeing up
map perf test to ensure our impl doesn't suck
initial reserve sizes for arrays/maps...some use cases would like this such as mem traces
xIn -> x_
Consider VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT and friends
GPU query pools
Investigate whether we need to support separate graphics/present queues: currently require same queue for both
const/constexpr all the VK structures in init
Look into VkValidationFeatureEnableEXT
Look into VkValidationFlagsEXT

Vulkan heap notes:
	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	Static GPU resources like render targets, textures, and geometry buffers

	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	AMD only: the 256 of video memory the CPU can write to directly
	Good for dynamic stuff the CPU writes to every frame eg uniform buffers/dyn geom buffers

	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	CPU memory that the GPU reads over PCIE
	Second best for uniform/dy geom buffers
	Best for staging buffers

	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT
	???

	VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	Integrated GPUs can store static resources here as well

	Dynamic: any non-DEVICE_LOCAL works and write directly
	Highly random access: DEVICE_LOCAL and stage from HOST_VISIBLE


	if DEVICE_LOCAL alloc fails, fall back to HOST_VISIBLE
	Alloc large DEVICE_LOCAL objects first, like render targets

	HOST_CACHED: good for reading back via pointer
	not HOST_CACHED: good for memcpying to

	VK_MEMORY_PROPERTY_
	VK_MEMORY_PROPERTY_
	VK_MEMORY_PROPERTY_
	VK_MEMORY_PROPERTY_
	VK_MEMORY_PROPERTY_
switch all UINT32_MAX and friends to local defs
Interested in calculating avg wasted temp mem when we jump to a new block and leave the wasted end bit
ability to begin multiple buffer transfers and wait on all of their fences
window rect is signed
de-line Map.h
go through win, fs, event, render and replace errors with real error codes
! crashes if window is bigger than desktop
! start maximized
! profiling
! vulkan memory allocation
! configuration
memstats / tracing
window::stateflgs::hidden never being used
concerned about wasted memory with maps...would be nice if 
map rehash testing
camera class
check: shaderUniformBufferArrayDynamicIndexing, shaderSampledImageArrayDynamicIndexing, shaderStorageBufferArrayDynamicIndexing and shaderStorageImageArrayDynamicIndexing are caps since Vulkan 1.0. Having those false means that the arrays of the relevant resources can only be accessed using a constant index (or even better: any constant expression). Pretty much everyone has those set to true so let�s move on. Vulkan 1.2 added shaderInputAttachmentArrayDynamicIndexing, shaderUniformTexelBufferArrayDynamicIndexing and shaderStorageTexelBufferArrayDynamicIndexing
	re: https://anki3d.org/resource-uniformity-bindless-access-in-vulkan/
check: XXXArrayNonUniformIndexing 
research timeline semaphores and their uses
static-ify render
FmtStr shouldn't be parameterized on all args, only the number of args, since we're currently not checking the individual arg flags
VArg -> Val?
test for all the common shit: common, math
hash table stats
maximizing window right when we're starting up causes a command buffer weirdness
	we should probably end the command buffer and restart it, see what love2d does here
our sprite structure has padding...can we get rid of it?
consider replacing all ' = 0' with ' = {}'
if performance is a concern and you have profiling data to back it up then you need to take into account: VkPhysicalDeviceLimits.optimalBufferCopyOffsetAlignment
replace staging buffer with host/device visible memory if it exists
array tests, especially around zero-ing memory
map zero memory and tests
use REBAR for sprite xfer if available
VK_EXT_host_image_copy
Report vulkan linear block internal fragmentation (amt we lose at the end of the current block if its not big enough). May want to scan all linear for remaining size, sort by remaining size, etc
Test vk mem alloc paths: reducing chunk size when out of memory, dedicated/arena boundary conditions, etc

Redo Fmt_WriteF64 to make it more efficient, compare to stb_printf
Move all static bounds to config
cvar/config


- clang / lint to enforce no dropped Res<>
- frameId in ErrData, need Err::Frame()
- Check frameId on access
- Hash ns/code at compiletime
- cap arg strings: set err flag on truncation
- Err pool should be a ring buffer, so we don't overwrite frameId if possible
- Implicit stack trace even if propagating?
- Error on zero-sized window: vk can't create a zero sized swapchain
- Consider whether permmem allocation for handlepools and such is worth it. Could easily require static buffers
window events: minimized, hidden, etc

WM_ENTERSIZEMOVE freezes processing and fills up the buffer. options:
	stop sending windows-related input events until WM_EXITSIZEMOVE
	just overwrite the event buffer events; don't panic on full
Verify json parsing fails if negative value is present when expecint U32
namespace FS -> Fs
make all perm arrays use the perm allocator
- Consider accumulating/combining input events in Window_Win.cpp instead of in Innput.cpp. Have the keyDown state trackers and such live in Window_Win, and have it only emit a key event if the state actually changed.
- Unit.cpp: verify that Log::Init(tempMem) does not persist any state into tempMem, since Mem::Reset(tempMem, MemMark()) is called each test iteration and would invalidate it.

# Gpu review

- ! swapchainImages leaks permMem on every RecreateSwapchain: InitSwapchain calls Mem::AllocSpan<Image>(permMem) each time, stranding the old allocation
- FreeFns() never called in Shutdown() — vulkan-1.dll not unloaded
- Per-resource vkAllocateMemory (no suballocator) — can hit maxMemoryAllocationCount limit at scale
- CreateGraphicsPipeline: blend state hardcoded to alpha-blend on all color attachments, no way to create non-blending or additive pipelines
- CreateGraphicsPipeline: depth test LESS + depth write always ON, breaks 2D/UI passes that want depth disabled
- BeginPass: depth attachment always uses VK_ATTACHMENT_LOAD_OP_CLEAR even when pass->clear is false — pass->clear only controls color attachments
- BarrierStage flags start at 1<<1, skipping bit 0 (None=0 is correct but first real flag could be 1<<0)
- DrawIndexedIndirect: indirect buffer offset hardcoded to 0, can't draw a subset of the buffer
- Dead code: empty `struct VPrinter : Printer {}` inside InitInstance() (Gpu_Vk.cpp:302)
- Validation warnings silently dropped in DebugCallback (line 179 commented out)
- enableDebug hardcoded to true — expose via cfg
- Gpu_Vk_Util.cpp has #pragma once at line 1 but is compiled as a separate TU, so it has no effect
- CLAUDE.md documents pool sizes as 4096/4096/1024/128 but code has all four at 128
- CreateBufferImpl sets queueFamilyIndexCount=1 with VK_SHARING_MODE_EXCLUSIVE (field is ignored by spec but misleading)
- No pipeline cache passed to vkCreateGraphicsPipelines — consider for faster subsequent startups

# Draw review

- ! DrawFont: signed char indexing into glyphs[256] — str[i] is signed on MSVC x64, chars >= 128 produce negative indices; cast to U8
- ! origin silently ignored for DrawSprite, DrawCanvas, DrawRect — only DrawFont applies it; either implement or remove origin from those DrawDef structs
- Shutdown destroys fontObjs[0].image which is never initialized (fontObjsLen starts at 1); safe only if Mem::AllocT zero-inits and DestroyImage is no-op for null handles
- Canvas layout ordering is an invisible invariant — DrawCanvas will sample in Undefined layout if the canvas hasn't been rendered to yet this frame; add assert or doc
- SetDefaultCanvas is redundant with SetCanvas({}) since SetCanvas already falls back to swapchainCanvas when handle == 0
- No z-sorting of draw commands — transparency requires painter's order; add comment or assert that callers are responsible
- static frameIdx declared mid-file, not grouped with other module globals
- Recreated depth image in ResizeWindow is not named (no Gpu_Name after recreate)
- MaxSprites = 1MB (~48MB permanent allocation); add comment justifying size or reduce to realistic cap
- DrawCanvas ignores origin
- outlineWidth is treated as bool in shader
- ImmediateWait is caller responsibility. LoadImage calls ImmediateCopyToImage but doesn't wait. App_Battle.cpp has a comment acknowledging this has caused multiple debugging sessions. The risk is someone calling LoadSprites/LoadFont without a subsequent ImmediateWait and getting GPU hazards. The cleanup in LoadImage releases CPU memory before the copy is guaranteed complete, which relies on the fact that stbi_image_free in the Defer block is fine because ImmediateCopyToImage presumably starts the copy synchronously (or it would be a double bug). Worth documenting the contract explicitly.
10. Scene::projViews[MaxCanvases + 1] comment is stale (Draw.cpp:141)
  The +1 for the no-canvas pass comment doesn't match the current design where the swapchain canvas occupies a real pool slot. The +1 may just be a safety margin now.

  11. StrWidth uses two different metrics depending on position (Draw.cpp:766)
  Interior chars use xAdv, last char uses off.x + size.x. This gives true visual width for the last glyph but is inconsistent with cursor-advance semantics. The commented-out xAdv line
  suggests this was a deliberate choice — worth a comment explaining why.

  12. SetCanvas(nullHandle) vs SetDefaultCanvas redundancy (Draw.cpp:638)
  SetCanvas with a null handle silently routes to the swapchain canvas. SetDefaultCanvas does the same explicitly. Consider removing the null-handle fallback from SetCanvas and requiring
  callers to use SetDefaultCanvas.

- Use u32 for all Max sizes instead of U16. if we ever change max size to exceed a u16 boundary, we have to change teh corresponding *Len field as well, error prone. Same problem for loop indexes.
- explicitly initialize static local vars to zero?
- rename forestwalk and friends
- reorder vars in major .cpp files like Draw
- Json errors are not useful because they don't include the path
	- Either use Json::FileToObj and Json::FileToArray so we have the path, or stack the errors
- Json missing close quote gives the END OF FILE as position. store opening quote in parse ctx
- Log error on Err::MakeV break
	- but be careful of recursion
- Pathfinder should prefer shorts # hexes all things being equal
- Eador-like pathfinding:
	- hover over enemy: shortest path
	- hover over enemy border: shortest path going through that border
- Clear keys on refocus window
- Err/Res conversion should push the current fiel/line on the err stack
- All static sizes should be data driven
- Error push scope/context in json..path at least