#pragma once

#include "JC/Common.h"

namespace JC {
	struct Log;
	struct Mat4;
	namespace Window { struct PlatformInfo; }
}

namespace JC::Render {

//--------------------------------------------------------------------------------------------------

struct InitInfo {
	Arena*                perm               = 0;
	Arena*                temp               = 0;
	Log*                  log                = 0;
	u32                   width              = 0;
	u32                   height             = 0;
	Window::PlatformInfo* windowPlatformInfo = {};
};

enum struct BufferUsage {
	Vertex,
	Index,
	Uniform,
	Storage,
	Indirect,
	Readback,
};

enum struct ImageUsage {
	Sampled,
	ColorAttachment,
	DepthStencilAttachment,
};

enum struct ImageFormat {
};

enum struct ImageLayout {
	Undefined,
	General,
	ColorAttachmentOptimal,
	DepthStencilAttachmentOptimal,
};

struct Buffer   { u64 handle = 0; };
struct Sampler  { u64 handle = 0; };
struct Image    { u64 handle = 0; };
struct Shader   { u64 handle = 0; };
struct Pipeline { u64 handle = 0; };

struct BufferUpdate {
	u64    handle = 0;
	Buffer buffer = {};
	void*  ptr    = 0;
};

struct Viewport {
};

struct Scissor {
};

enum struct LoadOp {
};

struct Clear {};

struct Attachment {
	Image image = {};
	LoadOp  loadOp  = {};
	Clear clear = {};
};

struct Binding {
	Buffer buffer = {};
	Image  image  = {};
};

struct PushConstants {
	void* data = 0;
	u64   len  = 0;
};

struct Pass {
	Pipeline         pipeline               = {};
	Viewport         viewport               = {};
	Scissor          scissor                = {};
	Span<Attachment> colorAttachments       = {};
	Attachment       depthStencilAttachment = {};
	Span<Binding>    bindings               = {};
	PushConstants    pushConstants          = {};
};

static constexpr ErrCode Err_Version  = { .ns = "render", .code = 1 };
static constexpr ErrCode Err_NoLayer  = { .ns = "render", .code = 2 };
static constexpr ErrCode Err_NoDevice = { .ns = "render", .code = 3 };
static constexpr ErrCode Err_NoMem    = { .ns = "render", .code = 4 };
static constexpr ErrCode Err_Resize   = { .ns = "render", .code = 5 };

Res<>         Init(const InitInfo* initInfo);
void          Shutdown();

Res<>         ResizeSwapchain(u32 width, u32 height);

Res<Buffer>   CreateBuffer(u64 size, BufferUsage usage);
void          DestroyBuffer(Buffer buffer);
u64           GetBufferAddr(Buffer buffer);
Res<BufferUpdate> BeginBufferUpdate(Buffer buffer);
void              EndBufferUpdate(BufferUpdate bufferUpdate);
void              BufferBarrier(Buffer buffer, ...);	// src/dst stage/access

Res<Image>    CreateImage(u32 width, u32 height, ImageFormat format, ImageUsage usage, Sampler sampler, void* data);
void          DestroyImage(Image image);
void          ImageBarrier(Image image, ...);	// src/dst layout/stage/access

Res<Shader>   CreateShader(const void* data, u64 len, s8 entry);
void          DestroyShader(Shader shader);

Res<Pipeline> CreateGraphicsPipeline(...);
Res<Pipeline> CreateComputePipeline(...);
void          DestroyPipeline(Pipeline pipeline);

Res<>         BeginFrame();
Res<>         EndFrame();

Res<>         BeginPass();
void          EndPass();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Render