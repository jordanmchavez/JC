#pragma once

#include "JC/Common.h"

namespace JC {
	struct Log;
	struct Mat4;
	namespace Window { struct PlatformInfo; }
}

namespace JC::Render {

//--------------------------------------------------------------------------------------------------

static constexpr ErrCode Err_Version  = { .ns = "render", .code = 1 };
static constexpr ErrCode Err_NoLayer  = { .ns = "render", .code = 2 };
static constexpr ErrCode Err_NoDevice = { .ns = "render", .code = 3 };
static constexpr ErrCode Err_NoMem    = { .ns = "render", .code = 4 };
static constexpr ErrCode Err_Resize   = { .ns = "render", .code = 5 };
static constexpr ErrCode Err_ShaderTooManyPushConstantBlocks = { .ns = "render", .code = 6 };

//--------------------------------------------------------------------------------------------------

struct Buffer   { u64 handle = 0; };
struct Sampler  { u64 handle = 0; };
struct Image    { u64 handle = 0; };
struct Shader   { u64 handle = 0; };
struct Pipeline { u64 handle = 0; };

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
	Undefined = 0
};

enum struct ImageLayout {
	Undefined = 0,
};

struct BufferUpdate {
	u64    handle = 0;
	Buffer buffer = {};
	void*  ptr    = 0;
};

struct ImageUpdate {
	u64   handle = 0;
	Image image  = {};
	void* ptr    = 0;
	u32   rowLen = 0;
};

struct Viewport {
	float x      = 0.0f;
	float y      = 0.0f;
	float width  = 0.0f;
	float height = 0.0f;
};

struct PushConstants {
	void* data = 0;
	u64   len  = 0;
};

struct Pass {
	Pipeline      pipeline         = {};
	Span<Image>   colorAttachments = {};
	Image         depthAttachment  = {};
	Viewport      viewport         = {};
	Rect          scissor          = {};
	PushConstants pushConstants    = {};
};

struct InitInfo {
	Arena*                perm               = 0;
	Arena*                temp               = 0;
	Log*                  log                = 0;
	u32                   width              = 0;
	u32                   height             = 0;
	Window::PlatformInfo* windowPlatformInfo = {};
};

Res<>             Init(const InitInfo* initInfo);
void              Shutdown();
Res<>             ResizeSwapchain(u32 width, u32 height);

Res<Buffer>       CreateBuffer(u64 size, BufferUsage usage);
void              DestroyBuffer(Buffer buffer);
u64               GetBufferAddr(Buffer buffer);
Res<BufferUpdate> BeginBufferUpdate(Buffer buffer);
void              EndBufferUpdate(BufferUpdate bufferUpdate);
void              BufferBarrier(Buffer buffer, u64 srcStage, u64 srcAccess, u64 dstStage, u64 dstAccess);

Res<Image>        CreateImage(u32 width, u32 height, ImageFormat format, ImageUsage usage, Sampler sampler);
void              DestroyImage(Image image);
void              BindImage(Image image, u32 idx, ImageLayout layout);
Res<ImageUpdate>  BeginImageUpdate(Image image, u32 rowLen);
void              EndImageUpdate(ImageUpdate imageUpdate);
void              ImageBarrier(Image image, u64 srcStage, u64 srcAccess, u64 dstLayout, u64 dstStage, u64 dstAccess);

Res<Shader>       CreateShader(const void* data, u64 len, s8 entry);
void              DestroyShader(Shader shader);

Res<Pipeline>     CreateGraphicsPipeline(Span<Shader> shaders);
void              DestroyPipeline(Pipeline pipeline);

Res<>             BeginFrame();
Res<>             EndFrame();

void              BeginPass();
void              EndPass();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Render