#pragma once

#include "JC/Common.h"

namespace JC {

struct FileApi;
struct Log;
struct Mem;
struct Mat4;
struct TempMem;
struct WindowPlatformData;

namespace Render {

//--------------------------------------------------------------------------------------------------

struct ApiInit {
	Log*                log                = 0;
	Mem*                mem                = 0;
	TempMem*            tempMem            = 0;
	u32                 width              = 0;
	u32                 height             = 0;
	WindowPlatformData* windowPlatformData = {};
};

enum struct MemUsage {
	Device,
	HostWrite,
	HostRead,
};

enum struct BufferUsage {
	Uniform,
	Vertex,
	Index,
	Storage,
	Indirect,
	TransferSrc,
	TransferDst,
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

struct Api {
	static constexpr ErrCode Err_Version  = { .ns = "render", .code = 1 };
	static constexpr ErrCode Err_NoLayer  = { .ns = "render", .code = 2 };
	static constexpr ErrCode Err_NoDevice = { .ns = "render", .code = 3 };
	static constexpr ErrCode Err_NoMem    = { .ns = "render", .code = 4 };
	static constexpr ErrCode Err_Resize   = { .ns = "render", .code = 5 };

	virtual Res<>         Init(const ApiInit* init) = 0;
	virtual void          Shutdown() = 0;

	virtual Res<>         ResizeSwapchain(u32 width, u32 height) = 0;

	virtual Res<Buffer>   CreateBuffer(u64 size, BufferUsage usage) = 0;
	virtual void          DestroyBuffer(Buffer buffer) = 0;
	virtual u64           GetBufferAddr(Buffer buffer) = 0;
	virtual void*         BeginBufferUpdate(Buffer buffer) = 0;
	virtual void          EndBufferUpdate(Buffer buffer) = 0;
	virtual void          BufferBarrier(Buffer buffer, ...) = 0;	// src/dst stage/access

	virtual Res<Image>    CreateImage(u32 width, u32 height, ImageFormat format, ImageUsage usage, Sampler sampler, void* data) = 0;
	virtual void          DestroyImage(Image image) = 0;
	virtual void          ImageBarrier(Image image, ...) = 0;	// src/dst layout/stage/access

	virtual Res<Shader>   CreateShader(const void* data, u64 len, s8 entry) = 0;
	virtual void          DestroyShader(Shader shader) = 0;

	virtual Res<Pipeline> CreateGraphicsPipeline(...) = 0;
	virtual Res<Pipeline> CreateComputePipeline(...) = 0;
	virtual void          DestroyPipeline(Pipeline pipeline);

	virtual Res<>         BeginFrame() = 0;
	virtual Res<>         EndFrame() = 0;

	virtual Res<>         BeginPass() = 0;
	virtual void          EndPass() = 0;
};

Api* GetApi();

//--------------------------------------------------------------------------------------------------

}	// namespace Render
}	// namespace JC