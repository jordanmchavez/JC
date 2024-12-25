#pragma once

#include "JC/Common.h"

namespace JC {

struct Log;
namespace Window { struct PlatformDesc; }

namespace Render {

//--------------------------------------------------------------------------------------------------

static constexpr ErrCode Err_Version                         = { .ns = "render", .code = 1 };
static constexpr ErrCode Err_NoLayer                         = { .ns = "render", .code = 2 };
static constexpr ErrCode Err_NoDevice                        = { .ns = "render", .code = 3 };
static constexpr ErrCode Err_NoMem                           = { .ns = "render", .code = 4 };
static constexpr ErrCode Err_RecreateSwapchain               = { .ns = "render", .code = 5 };
static constexpr ErrCode Err_ShaderTooManyPushConstantBlocks = { .ns = "render", .code = 6 };

//--------------------------------------------------------------------------------------------------

struct Buffer   { u64 handle = 0; };
struct Sampler  { u64 handle = 0; };
struct Image    { u64 handle = 0; };
struct Shader   { u64 handle = 0; };
struct Pipeline { u64 handle = 0; };

enum struct Stage {
	None,
	TransferSrc,
	TransferDst,
	ShaderSampled,
	ColorAttachment,
	Present,
};

enum struct BufferUsage {
	Undefined = 0,
	Storage,
	Index,
};

enum struct ImageUsage {
	Undefined = 0,
	Sampled,
	ColorAttachment,
	DepthAttachment,
};

enum struct ImageFormat {
	Undefined = 0,
	B8G8R8A8_UNorm,
	R8G8B8A8_UNorm,
	D32_Float,
};

struct Viewport {
	f32 x = 0.0f;
	f32 y = 0.0f;
	f32 w = 0.0f;
	f32 h = 0.0f;
};

struct Pass {
	Pipeline    pipeline         = {};
	Span<Image> colorAttachments = {};
	Image       depthAttachment  = {};
	Viewport    viewport         = {};
	Rect        scissor          = {};
};

struct InitDesc {
	Arena*                      perm               = 0;
	Arena*                      temp               = 0;
	Log*                        log                = 0;
	u32                         width              = 0;
	u32                         height             = 0;
	const Window::PlatformDesc* windowPlatformDesc = {};
};


Res<>         Init(const InitDesc* initDesc);
void          Shutdown();

void          WaitIdle();

Res<>         RecreateSwapchain(u32 width, u32 height);
Image         GetSwapchainImage();
ImageFormat   GetSwapchainImageFormat();

Res<Buffer>   CreateBuffer(u64 size, BufferUsage usage);
void          DestroyBuffer(Buffer buffer);
u64           GetBufferAddr(Buffer buffer);

Res<Image>    CreateImage(u32 width, u32 height, ImageFormat format, ImageUsage usage);
void          DestroyImage(Image image);
u32           BindImage(Image image);

Res<Shader>   CreateShader(const void* data, u64 len);
void          DestroyShader(Shader shader);

Res<Pipeline> CreateGraphicsPipeline(Span<Shader> shaders, Span<ImageFormat> colorAttachmentFormats, ImageFormat depthAttachmentFormat);
void          DestroyPipeline(Pipeline pipeline);

Res<>         BeginFrame();
Res<>         EndFrame();

void*         BeginBufferUpdate(Buffer buffer);
void          EndBufferUpdate(Buffer buffer);

void*         BeginImageUpdate(Image image);
void          EndImageUpdate(Image image);

void          BufferBarrier(Buffer buffer, u64 srcStage, u64 srcAccess, u64 dstStage, u64 dstAccess);
void          ImageBarrier(Image image, Stage src, Stage dst);

void          BeginPass(const Pass* pass);
void          EndPass();

void          BindIndexBuffer(Buffer buffer);

void          PushConstants(Pipeline pipeline, const void* data, u32 len);

void          DrawIndexed(u32 indexCount);

//--------------------------------------------------------------------------------------------------

}	// namespace Render
}	// namespace JC