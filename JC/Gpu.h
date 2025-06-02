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
	Mem::Allocator*             allocator          = 0;
	Mem::TempAllocator*         tempAllocator      = 0;
	Log::Logger*                logger             = 0;
	U32                         windowWidth        = 0;
	U32                         windowHeight       = 0;
	const Window::PlatformDesc* windowPlatformDesc = {};
};

struct Buffer    { U64 handle = 0; };
struct Sampler   { U64 handle = 0; };
struct Image     { U64 handle = 0; };
struct Shader    { U64 handle = 0; };
struct Pipeline  { U64 handle = 0; };
struct Cmd       { U64 handle = 0; };

namespace BufferUsage {
	using Flags = U32;
	constexpr Flags Storage  = 1 << 0;
	constexpr Flags Index    = 1 << 1;
	constexpr Flags CpuWrite = 1 << 2;
}

namespace ImageUsage {
	using Flags = U32;
	constexpr Flags Sampled         = 1 << 0;
	constexpr Flags ColorAttachment = 1 << 1;
	constexpr Flags DepthAttachment = 1 << 2;
	constexpr Flags CpuWrite        = 1 << 3;
}

enum struct ImageFormat {
	Undefined = 0,
	B8G8R8A8_UNorm,
	R8G8B8A8_UNorm,
	D32_Float,
};

struct Frame {
	Cmd   cmd;
	Image swapchainImage;
};

struct Viewport {
	F32 x;
	F32 y;
	F32 w;
	F32 h;
};

enum struct Stage {
	Invalid = 0,
	ColorAttachmentOutput,
	PresentSrc,
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

Res<>         RecreateSwapchain(U32 width, U32 height);

Res<Buffer>   CreateBuffer(U64 size, U32 flags);
void          DestroyBuffer(Buffer buffer);
U64           GetBufferAddr(Buffer buffer);

Res<Image>    CreateImage(U32 width, U32 height, ImageFormat format, U32 flags);
void          DestroyImage(Image image);
U32           GetImageWidth(Image image);	// TODO; -> IVec2 or IExtent or something
U32           GetImageHeight(Image image);
ImageFormat   GetImageFormat(Image image);
U32           GetImageBindIdx(Image image);

Res<Shader>   CreateShader(const void* data, U64 len);
void          DestroyShader(Shader shader);

Res<Pipeline> CreateGraphicsPipeline(Span<Shader> shaders, Span<ImageFormat> colorAttachmentFormats, ImageFormat depthAttachmentFormat);
void          DestroyPipeline(Pipeline pipeline);

Res<Frame>    BeginFrame();	// TODO: instead return an array of cmds, one per thread
Res<>         EndFrame();

Res<Cmd>      BeginImmediateCmds();
Res<>         EndImmediateCmds(Stage waitStage);

void          CmdBeginPass(Cmd cmd, const Pass* pass);
void          CmdEndPass(Cmd cmd);

void*         CmdBeginBufferUpload(Cmd cmd, Buffer buffer, U64 offset, U64 size);
void          CmdEndBufferUpload(Cmd cmd, Buffer buffer);

void*         CmdBeginImageUpload(Cmd cmd, Image image);
void          CmdEndImageUpload(Cmd cmd, Image image);

void          CmdBufferBarrier(Cmd cmd, Buffer buffer, Stage srcStage, Stage dstStage);
void          CmdImageBarrier(Cmd cmd, Image image, Stage srcStage, Stage dstStage);
void          CmdDebugBarrier(Cmd cmd);

void          CmdBindIndexBuffer(Cmd cmd, Buffer buffer);

void          CmdPushConstants(Cmd cmd, Pipeline pipeline, const void* data, U32 len);

void          CmdDraw(Cmd cmd, U32 vertexCount, U32 instanceCount);
void          CmdDrawIndexed(Cmd cmd, U32 indexCount);

void          WaitIdle();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Gpu