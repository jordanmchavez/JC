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
static constexpr ErrCode Err_RecreateSwapChain               = { .ns = "render", .code = 5 };
static constexpr ErrCode Err_ShaderTooManyPushConstantBlocks = { .ns = "render", .code = 6 };

//--------------------------------------------------------------------------------------------------

struct Buffer   { u64 handle = 0; };
struct Sampler  { u64 handle = 0; };
struct Texture  { u64 handle = 0; };
struct Shader   { u64 handle = 0; };
struct Pipeline { u64 handle = 0; };

enum struct ResourceState {
	Undefined = 0,
	RenderTarget,
	Present,
};

enum struct BufferUsage {
	Static,
	Upload,
};

enum struct TextureUsage {
	Sampled,
	ColorAttachment,
	DepthStencilAttachment,
};

enum struct TextureFormat {
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

struct Viewport {
	f32 x = 0.0f;
	f32 y = 0.0f;
	f32 w = 0.0f;
	f32 h = 0.0f;
};

struct Pass {
	Pipeline      pipeline      = {};
	Span<Texture> renderTargets = {};
	Texture       depthStencil  = {};
	Viewport      viewport      = {};
	Rect          scissor       = {};
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
Res<>         RecreateSwapChain(u32 width, u32 height);
Texture       GetSwapChainTexture();
TextureFormat GetSwapChainTextureFormat();

Res<Buffer>   CreateBuffer(u64 size, BufferUsage usage);
void          DestroyBuffer(Buffer buffer);
u64           GetBufferAddr(Buffer buffer);
void*         BeginBufferUpload(Buffer buffer);
void          EndBufferUpload(Buffer buffer);

Res<Texture>  CreateTexture(u32 width, u32 height, TextureFormat format, TextureUsage usage, Sampler sampler);
void          DestroyTexture(Texture texture);
void          TextureBarrier(Texture, ResourceState from, ResourceState to);

Res<Shader>   CreateShader(const void* data, u64 len);
void          DestroyShader(Shader shader);

Res<Pipeline> CreateGraphicsPipeline(Span<Shader> shaders, Span<TextureFormat> renderTargetFormats);
void          DestroyPipeline(Pipeline pipeline);

Res<>         BeginFrame();
Res<>         EndFrame();

void          CmdBeginPass(const Pass* pass);
void          CmdEndPass();

void          CmdBindIndexBuffer(Buffer buffer);

void          CmdDrawIndexed(u32 indexCount);

//--------------------------------------------------------------------------------------------------

}	// namespace Render
}	// namespace JC