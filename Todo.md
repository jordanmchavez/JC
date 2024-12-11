https://vulkan-tutorial.com/
https://vkguide.dev/
mastering vulkan book
compare with love2d
https://jorenjoestar.github.io/post/modern_sprite_batch/

https://www.sctheblog.com/blog/vulkan-initialization/
https://www.sctheblog.com/blog/vulkan-synchronization/
https://www.sctheblog.com/blog/vulkan-resources/
https://www.sctheblog.com/blog/vulkan-frame/

Sync:
https://www.khronos.org/blog/understanding-vulkan-synchronization
https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/
https://cpp-rendering.io/barriers-vulkan-not-difficult/
https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html

Render Graphics:
https://themaister.net/blog/2017/08/

Implementation:
https://zeux.io/2020/02/27/writing-an-efficient-vulkan-renderer/
https://github.com/Themaister/Granite
https://themaister.net/blog/2019/04/
https://themaister.net/blog/2019/05/


memtrace
windowing based on sdl2
simple 2d renderer api based on, maybe, love.2d
input
console/cmd

vk allocation callbacks

console window for logging
log file
move gfx into its own header
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
Robin hood hashing
Check for 64 bits in platform
ResizeAlloc for array, or at least realloc
Move disabled warnings to config so it's in code, not project file
MiMalloc as custom allocator

Windows window console
Log file output so we have something durable in addition to the console window in case of fatal error
Temp allocator for containers
SPrintf require buf size + 1? Will it always write null?
restrict on allocator returns
	also malloc annotation
temp allocator: could track how many segments over multiple frames and only dealloc if unused after X frames
temp allocator asserts
temp alloc tracking and debugging
replace system allocator with mimalloc variant
proper vm page size
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
MakeArg -> Arg_Make?
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
move this lwoer and add assert	constexpr char operator[](u64 i) const { return ptr[i]; }
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