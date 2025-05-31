#pragma once

#include "JC/Core.h"

namespace JC::Log { struct Logger; }
namespace JC::Window { struct PlatformDesc; }

namespace JC::Gpu {

//--------------------------------------------------------------------------------------------------

DefErr(Gpu, RecreateSwapchain);

//--------------------------------------------------------------------------------------------------

constexpr Str Cfg_EnableVkValidation = "Gpu::EnableVkValidation";
constexpr Str Cfg_EnableDebug        = "Gpu::EnableDebug";

//--------------------------------------------------------------------------------------------------

struct InitDesc {
	Mem::Allocator*             allocator          = 0;
	Mem::TempAllocator*         tempAllocator      = 0;
	Log::Logger*                logger             = 0;
	U32                         windowWidth        = 0;
	U32                         windowHeight       = 0;
	const Window::PlatformDesc* windowPlatformDesc = {};
};

struct Buffer   { U64 handle = 0; };
struct Sampler  { U64 handle = 0; };
struct Image    { U64 handle = 0; };
struct Shader   { U64 handle = 0; };
struct Pipeline { U64 handle = 0; };

namespace BufferUsageFlags {
	constexpr U32 Storage = 1 << 0;
	constexpr U32 Index   = 1 << 1;
	constexpr U32 Static  = 1 << 2;
	constexpr U32 Dynamic = 1 << 4;
}

namespace ImageUsageFlags {
	constexpr U32 Sampled         = 1 << 0;
	constexpr U32 ColorAttachment = 1 << 1;
	constexpr U32 DepthAttachment = 1 << 2;
};

enum struct ImageFormat {
	Undefined = 0,
	B8G8R8A8_UNorm,
	R8G8B8A8_UNorm,
	D32_Float,
};

struct Viewport {
	F32 x;
	F32 y;
	F32 w;
	F32 h;
};

struct Pass {
	Pipeline    pipeline;
	Span<Image> colorAttachments;
	Image       depthAttachment;
	Viewport    viewport;
	Rect        scissor;
	bool        clear;
};

Res<>         Init(const InitDesc* initDesc);
void          Shutdown();
void          WaitIdle();
void          DebugBarrier();

Res<>         RecreateSwapchain(U32 width, U32 height);
Image         GetSwapchainImage();

Res<Buffer>   CreateBuffer(U64 size, U32 usageFlags);
void          DestroyBuffer(Buffer buffer);
U64           GetBufferAddr(Buffer buffer);
void          WriteBuffer(Buffer buffer, U64 offset, const void* data, U64 size);

Res<Image>    CreateImage(U32 width, U32 height, ImageFormat format, U32 usageFlags);
void          DestroyImage(Image image);
U32           GetImageWidth(Image image);	// TODO; -> IVec2 or IExtent or something
U32           GetImageHeight(Image image);
ImageFormat   GetImageFormat(Image image);
U32           BindImage(Image image);
void          WriteImage(Image image, const void* data);

Res<Shader>   CreateShader(const void* data, U64 len);
void          DestroyShader(Shader shader);

Res<Pipeline> CreateGraphicsPipeline(Span<Shader> shaders, Span<ImageFormat> colorAttachmentFormats, ImageFormat depthAttachmentFormat);
void          DestroyPipeline(Pipeline pipeline);

Res<>         BeginFrame();
Res<>         EndFrame();

void          BeginPass(const Pass* pass);
void          EndPass();

void          BindIndexBuffer(Buffer buffer);
void          PushConstants(Pipeline pipeline, const void* data, U32 len);
void          Draw(U32 vertexCount, U32 instanceCount);
void          DrawIndexed(U32 indexCount);

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Gpu

/*
what is the interface for tings we're actually doing?

sprite
ui
particle system
maybe meshes?

immutable data: texture, mesh VB
uploaded once then never touched
never need to barrier it
staging buffer can come and go

point 1: do we need to expose the per-frame data?
we do for dynamic stuff


gpu should handle dynamic buffers as a primitive:
create dynamic buffer
no frame stuff
no staging stuff
no barriers

static vs dynamic
index
storage
static
dynamic
stage usage: vertex, fragment, compute

raw interface

draw texture

*/