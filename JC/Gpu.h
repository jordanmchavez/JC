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

constexpr U32 MaxFrames = 3;

struct InitDesc {
	Mem::Allocator*             allocator;
	Mem::TempAllocator*         tempAllocator;
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
	constexpr Flags Upload       = 1 << 3;
	constexpr Flags Addr         = 1 << 4;
}

namespace ImageUsage {
	using Flags = U32;
	constexpr Flags Sampled = 1 << 0;
	constexpr Flags Color   = 1 << 1;
	constexpr Flags Depth   = 1 << 2;
	constexpr Flags Upload  = 1 << 3;
}

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

enum struct ImageLayout {
	Undefined = 0,
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
U32           GetFrameIdx();
ImageFormat   GetSwapchainImageFormat();
Res<>         RecreateSwapchain(U32 width, U32 height);

Res<Buffer>   CreateBuffer(U64 size, BufferUsage::Flags bufferUsageFlags);
void          DestroyBuffer(Buffer buffer);
U64           GetBufferAddr(Buffer buffer);
Res<>         UploadToBuffer(Buffer buffer, U64 offset, const void* data, U64 len, BarrierStage::Flags srcBarrierStageFlags, BarrierStage::Flags dstBarrierStageFlags);
void          BufferBarrier(Buffer buffer, U64 offset, U64 size, BarrierStage::Flags srcBarrierStageFlags, BarrierStage::Flags dstBarrierStageFlags);

Res<Image>    CreateImage(U32 width, U32 height, ImageFormat format, ImageUsage::Flags imageUsageFlags);
void          DestroyImage(Image image);
U32           GetImageWidth(Image image);	// TODO; -> IVec2 or IExtent or something
U32           GetImageHeight(Image image);
ImageFormat   GetImageFormat(Image image);
U32           GetImageBindIdx(Image image);
Res<>         UploadToImage(Image image, const void* data, BarrierStage::Flags srcBarrierStageFlags, ImageLayout srcImageLayout, BarrierStage::Flags dstBarrierStageFlags, ImageLayout dstImageLayout);
void          ImageBarrier(Image image, BarrierStage::Flags srcBarrierStageFlags, ImageLayout srcImageLayout, BarrierStage::Flags dstBarrierStageFlags, ImageLayout dstImageLayout);

Res<>         WaitForUploads();

Res<Shader>   CreateShader(const void* data, U64 len);
void          DestroyShader(Shader shader);

Res<Pipeline> CreateGraphicsPipeline(Span<Shader> shaders, Span<ImageFormat> colorAttachmentFormats, ImageFormat depthAttachmentFormat);
void          DestroyPipeline(Pipeline pipeline);

Res<Image>    BeginFrame();
Res<>         EndFrame();

void          BeginPass(const Pass* pass);
void          EndPass();

void          BindIndexBuffer(Buffer buffer);
void          PushConstants(Pipeline pipeline, const void* data, U32 len);

void          Draw(U32 vertexCount, U32 instanceCount);
void          DrawIndexed(U32 indexCount);
void          DrawIndexedIndirect(Buffer indirectBuffer, U32 drawCount);

void          WaitIdle();

void          DebugBarrier();

void          DbgName(Buffer   buffer,   SrcLoc sl, const char* fmt, Span<const Arg> args);
void          DbgName(Image    image,    SrcLoc sl, const char* fmt, Span<const Arg> args);
void          DbgName(Shader   shader,   SrcLoc sl, const char* fmt, Span<const Arg> args);
void          DbgName(Pipeline pipeline, SrcLoc sl, const char* fmt, Span<const Arg> args);

template <class T, class... Args>
void DbgNameF(T obj, SrcLoc sl, FmtStr<Args...> fmt, Args... args) {
	DbgName(obj, sl, fmt, { MakeArg(args)..., });
}

//#define Gpu_DbgName(obj)            Gpu::DbgNameF(obj, SrcLoc::Here(), #obj)
//#define Gpu_DbgNameF(obj, fmt, ...) Gpu::DbgNameF(obj, SrcLoc::Here(), fmt, ##__VA_ARGS__)

#define Gpu_DbgName(obj)            
#define Gpu_DbgNameF(obj, fmt, ...) 

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Gpu