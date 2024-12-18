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
	Undefined = 0,
	B8G8R8_N,
	B8G8R8_U,
	R8G8B8_N,
	R8G8B8_U,
	B8G8R8A8_N,
	B8G8R8A8_U,
	R8G8B8A8_N,
	R8G8B8A8_U,
	D32_F,
};

enum struct ImageLayout {
	Undefined = 0,
};

struct BufferUpdate {
	Buffer buffer = {};
	void*  ptr    = 0;
};

struct ImageUpdate {
	Image image  = {};
	u32   rowLen = 0;
	void* ptr    = 0;
};

struct Viewport {
	f32 x      = 0.0f;
	f32 y      = 0.0f;
	f32 width  = 0.0f;
	f32 height = 0.0f;
};

struct Pass {
	Pipeline      pipeline         = {};
	Span<Image>   colorAttachments = {};
	Image         depthAttachment  = {};
	Viewport      viewport         = {};
	Rect          scissor          = {};
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
Image             GetCurrentSwapchainImage();
ImageFormat       GetSwapchainImageFormat();

Res<Buffer>       CreateBuffer(u64 size, BufferUsage usage);
void              DestroyBuffer(Buffer buffer);
u64               GetBufferAddr(Buffer buffer);

Res<Image>        CreateImage(u32 width, u32 height, ImageFormat format, ImageUsage usage, Sampler sampler);
void              DestroyImage(Image image);
void              BindImage(Image image, u32 idx, ImageLayout layout);

Res<Shader>       CreateShader(const void* data, u64 len);
void              DestroyShader(Shader shader);

Res<Pipeline>     CreateGraphicsPipeline(Span<Shader> shaders, Span<ImageFormat> colorAttachmentFormats, ImageFormat depthFormat);
void              DestroyPipeline(Pipeline pipeline);

Res<>             BeginFrame();
Res<>             EndFrame();

Res<>             BeginCmds();
Res<>             EndCmds();
Res<>             ImmediateSubmitCmds();

BufferUpdate      CmdBeginBufferUpdate(Buffer buffer);
void              CmdEndBufferUpdate(BufferUpdate bufferUpdate);
void              CmdBufferBarrier(Buffer buffer, u64 srcStage, u64 srcAccess, u64 dstStage, u64 dstAccess);

ImageUpdate       CmdBeginImageUpdate(Image image, u32 rowLen);
void              CmdEndImageUpdate(ImageUpdate);
void              CmdImageBarrier(Image image, u64 srcStage, u64 srcAccess, u64 dstLayout, u64 dstStage, u64 dstAccess);

void              CmdBeginPass(const Pass* pass);
void              CmdEndPass();

void              CmdBindIndexBuffer(Buffer buffer);

void              CmdPushConstants(Pipeline pipeline, const void* ptr, u64 len);

void              CmdDrawIndexed(u32 indexCount);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Render