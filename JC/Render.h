#pragma once

#include "JC/Common.h"

namespace JC {

struct FileApi;
struct Log;
struct Mem;
struct Mat4;
struct TempMem;
struct WindowPlatformData;

//--------------------------------------------------------------------------------------------------

struct RenderApiInit {
	FileApi*            fileApi            = 0;
	Log*                log                = 0;
	Mem*                mem                = 0;
	TempMem*            tempMem            = 0;
	u32                 width              = 0;
	u32                 height             = 0;
	WindowPlatformData* windowPlatformData = {};
};

struct Buffer   { u64 handle = 0; };
struct Texture  { u64 handle = 0; };
struct Shader   { u64 handle = 0; };
struct Pipeline { u64 handle = 0; };

enum struct TextureFormat {
};

struct RenderApi {
	static constexpr ErrCode Err_Version  = { .ns = "render", .code = 1 };
	static constexpr ErrCode Err_NoLayer  = { .ns = "render", .code = 2 };
	static constexpr ErrCode Err_NoDevice = { .ns = "render", .code = 3 };
	static constexpr ErrCode Err_NoMem    = { .ns = "render", .code = 4 };
	static constexpr ErrCode Err_Resize   = { .ns = "render", .code = 5 };

	virtual Res<>    Init(const RenderApiInit* init) = 0;
	virtual void     Shutdown() = 0;
	virtual Res<>    Draw(const Mat4* view, const Mat4* proj) = 0;
	virtual Res<>    ResizeSwapchain(u32 width, u32 height) = 0;
	virtual Buffer   CreateBuffer(u64 len) = 0;
	virtual void     DestroyBuffer(Buffer buffer) = 0;
	virtual void     CreateTexture() = 0;
	virtual void     DestroyTexture() = 0;
	virtual u32      AddBindlessTexture(Texture texture) = 0;
	virtual Shader   CreateShader(const void* code, u64 len) = 0;
	virtual void     DestroyShader() = 0;
	virtual Pipeline CreatePipeline(Shader vertexShader, Shader fragmentShader) = 0;
	virtual void     DestroyPipeline() = 0;
	virtual void     BindPipeline(Pipeline pipeline) = 0;
	virtual void     BindIndexBuffer(Buffer buffer) = 0;

	virtual Res<>    BeginFrame() = 0;
	virtual Res<>    BeginRendering() = 0;
	virtual Res<>    EndRendering() = 0;
	virtual Res<>    EndFrame(Image drawImage) = 0;

	virtual Res<>    Draw() = 0;
};

RenderApi* GetRenderApi();

//--------------------------------------------------------------------------------------------------

}	// namespace JC