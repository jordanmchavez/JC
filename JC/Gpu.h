#pragma once

#include "JC/Core.h"

namespace JC::Log { struct Logger; }
namespace JC::Window { struct PlatformDesc; }

namespace JC::Gpu {

//--------------------------------------------------------------------------------------------------

JC_DEF_ERR(Gpu, RecreateSwapchain);

//--------------------------------------------------------------------------------------------------

constexpr Str Cfg_EnableVkValidation = "Gpu::EnableVkValidation";
constexpr Str Cfg_EnableDebug        = "Gpu::EnableDebug";

//--------------------------------------------------------------------------------------------------

constexpr U32 MaxFrames = 3;

struct InitDesc {
	Allocator*                  allocator;
	TempAllocator*              tempAllocator;
	Log::Logger*                logger;
	U32                         windowWidth;
	U32                         windowHeight;
	const Window::PlatformDesc* windowPlatformDesc;
};

struct Buffer   { U64 handle = 0; };
struct Sampler  { U64 handle = 0; };
struct Image    { U64 handle = 0; };
struct Shader   { U64 handle = 0; };
struct Pipeline { U64 handle = 0; };

namespace BufferUsage {
	using Flags = U32;
	constexpr Flags Storage      = 1 << 0;
	constexpr Flags Index        = 1 << 1;
	constexpr Flags DrawIndirect = 1 << 2;
	constexpr Flags Copy         = 1 << 3;
	constexpr Flags Addr         = 1 << 4;
}

namespace ImageUsage {
	using Flags = U32;
	constexpr Flags Sampled = 1 << 0;
	constexpr Flags Color   = 1 << 1;
	constexpr Flags Depth   = 1 << 2;
	constexpr Flags Copy    = 1 << 3;
}

enum struct ImageFormat {
	Undefined = 0,
	B8G8R8A8_UNorm,
	R8G8B8A8_UNorm,
	D32_Float,
};

enum struct ImageLayout {
	Undefined = 0,
	Color,
	ShaderRead,
	CopyDst,
};

struct Viewport {
	F32 x;
	F32 y;
	F32 w;
	F32 h;
};

namespace BarrierStage {
	using Flags = U64;
	constexpr Flags None                            = 0;
	constexpr Flags DrawIndirect_Read               = 1 <<  1;
	constexpr Flags IndexInput_Read                 = 1 <<  2;
	constexpr Flags VertexShader_UniformRead        = 1 <<  3;
	constexpr Flags VertexShader_SamplerRead        = 1 <<  4;
	constexpr Flags VertexShader_StorageRead        = 1 <<  5;
	constexpr Flags VertexShader_StorageWrite       = 1 <<  6;
	constexpr Flags FragmentShader_UniformRead      = 1 <<  7;
	constexpr Flags FragmentShader_SamplerRead      = 1 <<  8;
	constexpr Flags FragmentShader_StorageRead      = 1 <<  9;
	constexpr Flags FragmentShader_StorageWrite     = 1 << 10;
	constexpr Flags FragmentShader_ColorRead        = 1 << 11;
	constexpr Flags FragmentShader_DepthStencilRead = 1 << 12;
	constexpr Flags ComputeShader_UniformRead       = 1 << 13;
	constexpr Flags ComputeShader_SamplerRead       = 1 << 14;
	constexpr Flags ComputeShader_StorageRead       = 1 << 15;
	constexpr Flags ComputeShader_StorageWrite      = 1 << 16;
	constexpr Flags DepthStencilTest_Read           = 1 << 17;
	constexpr Flags DepthStencilTest_Write          = 1 << 18;
	constexpr Flags ColorOutput_ColorRead           = 1 << 19;
	constexpr Flags ColorOutput_ColorWrite          = 1 << 20;
	constexpr Flags Copy_Read                       = 1 << 21;
	constexpr Flags Copy_Write                      = 1 << 22;
};

struct Pass {
	Pipeline    pipeline;
	Span<Image> colorAttachments;
	Image       depthAttachment;
	Viewport    viewport;
	Rect        scissor;
	bool        clear;
};

struct Frame {
	U64   frame;
	Image swapchainImage;
};

Res<>         Init(const InitDesc* initDesc);
void          Shutdown();

ImageFormat   GetSwapchainImageFormat();
Res<>         RecreateSwapchain(U32 width, U32 height);

Res<Buffer>   CreateBuffer(U64 size, BufferUsage::Flags bufferUsageFlags);
void          DestroyBuffer(Buffer buffer);
U64           GetBufferAddr(Buffer buffer);

Res<Image>    CreateImage(U32 width, U32 height, ImageFormat format, ImageUsage::Flags imageUsageFlags);
void          DestroyImage(Image image);
U32           GetImageWidth(Image image);	// TODO; -> IVec2 or IExtent or something
U32           GetImageHeight(Image image);
ImageFormat   GetImageFormat(Image image);
U32           GetImageBindIdx(Image image);

Res<Shader>   CreateShader(const void* data, U64 len);
void          DestroyShader(Shader shader);

Res<Pipeline> CreateGraphicsPipeline(Span<Shader> shaders, Span<ImageFormat> colorAttachmentFormats, ImageFormat depthAttachmentFormat);
void          DestroyPipeline(Pipeline pipeline);

Res<>         ImmediateCopyToBuffer(const void* data, U64 len, Buffer buffer, U64 offset);
Res<>         ImmediateCopyToImage(const void* data, Image image, BarrierStage::Flags finalBarrierStageFlags, ImageLayout finalImageLayout);
Res<>         ImmediateWait();

Res<Frame>    BeginFrame();
Res<>         EndFrame();
void          WaitIdle();

void*         AllocStaging(U64 len);
void          CopyStagingToBuffer(void* staging, U64 len, Buffer buffer, U64 offset);
void          CopyStagingToImage(void* staging, Image image);
void          BufferBarrier(Buffer buffer, U64 offset, U64 size, BarrierStage::Flags srcBarrierStageFlags, BarrierStage::Flags dstBarrierStageFlags);
void          ImageBarrier(Image image, BarrierStage::Flags srcBarrierStageFlags, ImageLayout srcImageLayout, BarrierStage::Flags dstBarrierStageFlags, ImageLayout dstImageLayout);
void          DebugBarrier();
void          BeginPass(const Pass* pass);
void          EndPass();
void          BindIndexBuffer(Buffer buffer);
void          PushConstants(Pipeline pipeline, const void* data, U32 len);
void          Draw(U32 vertexCount, U32 instanceCount);
void          DrawIndexed(U32 indexCount);
void          DrawIndexedIndirect(Buffer indirectBuffer, U32 drawCount);

void          Name(SrcLoc sl, Buffer   buffer,   const char* fmt, Span<const Arg> args);
void          Name(SrcLoc sl, Image    image,    const char* fmt, Span<const Arg> args);
void          Name(SrcLoc sl, Shader   shader,   const char* fmt, Span<const Arg> args);
void          Name(SrcLoc sl, Pipeline pipeline, const char* fmt, Span<const Arg> args);

template <class T, class... Args>
void NameF(SrcLoc sl, T obj, FmtStr<Args...> fmt, Args... args) {
	Name(sl, obj, fmt, { MakeArg(args)..., });
}

#define JC_GPU_NAME(obj)            Gpu::NameF(SrcLoc::Here(), obj, #obj)
#define JC_GPU_NAMEF(obj, fmt, ...) Gpu::NameF(SrcLoc::Here(), obj, fmt, ##__VA_ARGS__)

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Gpu