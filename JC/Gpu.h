#pragma once

#include "JC/Core.h"

namespace JC::Log { struct Logger; }
namespace JC::Window { struct PlatformDesc; }

namespace JC::Gpu {

//--------------------------------------------------------------------------------------------------

DefErr(Gpu, RecreateSwapchain);

constexpr U32 MaxFrames = 3;

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

enum struct Stage {
	None,
	TransferSrc,
	TransferDst,
	VertexShaderRead,
	FragmentShaderSample,
	ColorAttachment,
	PresentOld,
	Present,
};

enum struct BufferUsage {
	Undefined = 0,
	Storage,
	Index,
	Staging,
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
	F32 x = 0.0f;
	F32 y = 0.0f;
	F32 w = 0.0f;
	F32 h = 0.0f;
};

struct Pass {
	Pipeline    pipeline         = {};
	Span<Image> colorAttachments = {};
	Image       depthAttachment  = {};
	Viewport    viewport         = {};
	Rect        scissor          = {};
	bool        clear            = false;
};

struct StagingMem {
	void* ptr;
	U64   size;
};

Res<>         Init(const InitDesc* initDesc);
void          Shutdown();
void          WaitIdle();
void          DebugBarrier();

StagingMem    AllocStagingMem(U64 size);

Res<>         RecreateSwapchain(U32 width, U32 height);
Image         GetSwapchainImage();

Res<Buffer>   CreateBuffer(U64 size, BufferUsage usage);
void          DestroyBuffer(Buffer buffer);
U64           GetBufferAddr(Buffer buffer);
void          UpdateBuffer(Buffer buffer, U64 offset, StagingMem stagingMem);
void          BufferBarrier(Buffer buffer, Stage src, Stage dst);

Res<Image>    CreateImage(U32 width, U32 height, ImageFormat format, ImageUsage usage);
void          DestroyImage(Image image);
U32           GetImageWidth(Image image);	// TODO; -> IVec2 or IExtent or something
U32           GetImageHeight(Image image);
ImageFormat   GetImageFormat(Image image);
U32           BindImage(Image image);
void          UpdateImage(Image image, StagingMem stagingMem);
void          ImageBarrier(Image image, Stage src, Stage dst);

Res<Shader>   CreateShader	(const void* data, U64 len);
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
U32           GetFrameIdx();

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Gpu