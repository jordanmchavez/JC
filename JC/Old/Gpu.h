#pragma once

#include "JC/Common.h"

struct Wnd_PlatformDesc;

//--------------------------------------------------------------------------------------------------

Err_Def(Gpu, RecreateSwapchain);

constexpr Str Cfg_EnableVkValidation = "Gpu::EnableVkValidation";
constexpr Str Cfg_EnableDebug        = "Gpu::EnableDebug";

//--------------------------------------------------------------------------------------------------

constexpr U32 Gpu_MaxFrames = 3;

struct Gpu_InitDesc {
	Mem*                    permMem;
	Mem*                    tempMem;
	U32                     windowWidth;
	U32                     windowHeight;
	Wnd_PlatformDesc const* wndPlatformDesc;
};

struct Gpu_Buffer   { U64 handle = 0; };
struct Gpu_Sampler  { U64 handle = 0; };
struct Gpu_Image    { U64 handle = 0; };
struct Gpu_Shader   { U64 handle = 0; };
struct Gpu_Pipeline { U64 handle = 0; };

namespace Gpu_BufferUsage {
	using Flags = U32;
	constexpr Flags Storage      = 1 << 0;
	constexpr Flags Index        = 1 << 1;
	constexpr Flags DrawIndirect = 1 << 2;
	constexpr Flags Copy         = 1 << 3;
	constexpr Flags Addr         = 1 << 4;
}

namespace Gpu_ImageUsage {
	using Flags = U32;
	constexpr Flags Sampled = 1 << 0;
	constexpr Flags Color   = 1 << 1;
	constexpr Flags Depth   = 1 << 2;
	constexpr Flags Copy    = 1 << 3;
}

enum struct Gpu_ImageFormat {
	Undefined = 0,
	B8G8R8A8_UNorm,
	R8G8B8A8_UNorm,
	D32_Float,
};

enum struct Gpu_ImageLayout {
	Undefined = 0,
	Color,
	ShaderRead,
	CopyDst,
};

struct Gpu_Viewport {
	F32 x;
	F32 y;
	F32 w;
	F32 h;
};

namespace Gpu_BarrierStage {
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

struct Gpu_Pass {
	Gpu_Pipeline    pipeline;
	Span<Gpu_Image> colorAttachments;
	Gpu_Image       depthAttachment;
	Gpu_Viewport    viewport;
	IRect           scissor;
	bool            clear;
};

struct Gpu_Frame {
	U64       frame;
	Gpu_Image swapchainImage;
};

Res<>             Gpu_Init(Gpu_InitDesc const* initDesc);
void              Gpu_Shutdown();
Gpu_ImageFormat   Gpu_GetSwapchainImageFormat();
Res<>             Gpu_RecreateSwapchain(U32 width, U32 height);
Res<Gpu_Buffer>   Gpu_CreateBuffer(U64 size, Gpu_BufferUsage::Flags bufferUsageFlags);
void              Gpu_DestroyBuffer(Gpu_Buffer buffer);
U64               Gpu_GetBufferAddr(Gpu_Buffer buffer);
Res<Gpu_Image>    Gpu_CreateImage(U32 width, U32 height, Gpu_ImageFormat format, Gpu_ImageUsage::Flags imageUsageFlags);
void              Gpu_DestroyImage(Gpu_Image image);
U32               Gpu_GetImageWidth(Gpu_Image image);	// TODO; -> IVec2 or IExtent or something
U32               Gpu_GetImageHeight(Gpu_Image image);
Gpu_ImageFormat   Gpu_GetImageFormat(Gpu_Image image);
U32               Gpu_GetImageBindIdx(Gpu_Image image);
Res<Gpu_Shader>   Gpu_CreateShader(void const* data, U64 len);
void              Gpu_DestroyShader(Gpu_Shader shader);
Res<Gpu_Pipeline> Gpu_CreateGraphicsPipeline(Span<Gpu_Shader> shaders, Span<Gpu_ImageFormat> colorAttachmentFormats, Gpu_ImageFormat depthAttachmentFormat);
void              Gpu_DestroyPipeline(Gpu_Pipeline pipeline);
Res<>             Gpu_ImmediateCopyToBuffer(void const* data, U64 len, Gpu_Buffer buffer, U64 offset);
Res<>             Gpu_ImmediateCopyToImage(void const* data, Gpu_Image image, Gpu_BarrierStage::Flags finalBarrierStageFlags, Gpu_ImageLayout finalImageLayout);
Res<>             Gpu_ImmediateWait();
Res<Gpu_Frame>    Gpu_BeginFrame();
Res<>             Gpu_EndFrame();
void              Gpu_WaitIdle();
void*             Gpu_AllocStaging(U64 len);
void              Gpu_CopyStagingToBuffer(void* staging, U64 len, Gpu_Buffer buffer, U64 offset);
void              Gpu_CopyStagingToImage(void* staging, Gpu_Image image);
void              Gpu_BufferBarrier(Gpu_Buffer buffer, U64 offset, U64 size, Gpu_BarrierStage::Flags srcBarrierStageFlags, Gpu_BarrierStage::Flags dstBarrierStageFlags);
void              Gpu_ImageBarrier(Gpu_Image image, Gpu_BarrierStage::Flags srcBarrierStageFlags, Gpu_ImageLayout srcImageLayout, Gpu_BarrierStage::Flags dstBarrierStageFlags, Gpu_ImageLayout dstImageLayout);
void              Gpu_DebugBarrier();
void              Gpu_BeginPass(Gpu_Pass const* pass);
void              Gpu_EndPass();
void              Gpu_BindIndexBuffer(Gpu_Buffer buffer);
void              Gpu_PushConstants(Gpu_Pipeline pipeline, void const* data, U32 len);
void              Gpu_Draw(U32 vertexCount, U32 instanceCount);
void              Gpu_DrawIndexed(U32 indexCount);
void              Gpu_DrawIndexedIndirect(Gpu_Buffer indirectBuffer, U32 drawCount);
void              Gpu_Namev(SrcLoc sl, Gpu_Buffer   buffer,   char const* fmt, Span<Arg const> args);
void              Gpu_Namev(SrcLoc sl, Gpu_Image    image,    char const* fmt, Span<Arg const> args);
void              Gpu_Namev(SrcLoc sl, Gpu_Shader   shader,   char const* fmt, Span<Arg const> args);
void              Gpu_Namev(SrcLoc sl, Gpu_Pipeline pipeline, char const* fmt, Span<Arg const> args);

template <class T, class... A>
void Gpu_NamefImpl(SrcLoc sl, T obj, FmtStr<A...> fmt, A... args) {
	Gpu_Namev(sl, obj, fmt, { Arg_Make(args)..., });
}

#define Gpu_Name(obj)            Gpu_NamefImpl(SrcLoc_Here(), obj, #obj)
#define Gpu_Namef(obj, fmt, ...) Gpu_NamefImpl(SrcLoc_Here(), obj, fmt, ##__VA_ARGS__)