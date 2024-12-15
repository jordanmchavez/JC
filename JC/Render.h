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

namespace BufferFlags {
	constexpr u64 Uniform           = 1 << 0;
	constexpr u64 Vertex            = 1 << 0;
	constexpr u64 Index             = 1 << 1;
	constexpr u64 Storage           = 1 << 2;
	constexpr u64 Static            = 1 << 3;
	constexpr u64 Staging           = 1 << 4;
	constexpr u64 TransferSrc       = 1 << 5;
	constexpr u64 TransferDst       = 1 << 6;
	constexpr u64 ShaderAddressable = 1 << 7;
};

struct Buffer   { u64 handle = 0; };
struct Image    { u64 handle = 0; };
struct Shader   { u64 handle = 0; };
struct Pipeline { u64 handle = 0; };

struct Api {
	static constexpr ErrCode Err_Version  = { .ns = "render", .code = 1 };
	static constexpr ErrCode Err_NoLayer  = { .ns = "render", .code = 2 };
	static constexpr ErrCode Err_NoDevice = { .ns = "render", .code = 3 };
	static constexpr ErrCode Err_NoMem    = { .ns = "render", .code = 4 };
	static constexpr ErrCode Err_Resize   = { .ns = "render", .code = 5 };

	virtual Res<>    Init(const ApiInit* init) = 0;
	virtual void     Shutdown() = 0;
	virtual Res<>    Draw(const Mat4* view, const Mat4* proj) = 0;
	virtual Res<>    ResizeSwapchain(u32 width, u32 height) = 0;

	virtual Res<Buffer>   CreateBuffer(u64 size, u64 flags) = 0;
	virtual void          DestroyBuffer(Buffer buffer) = 0;
	virtual Res<void*>    MapBuffer(Buffer buffer) = 0;
	virtual void          UnmapBuffer(Buffer buffer) = 0;

	virtual Res<Shader>   CreateShader(const void* data, u64 len) = 0;
	virtual void          DestroyShader(Shader shader) = 0;

	virtual Res<Pipeline> CreatePipeline(Shader vertexShader, Shader fragmentShader, u32 pushConstantsSize) = 0;
	virtual void          DestroyPipeline(Pipeline pipeline);

	virtual Res<>    BeginFrame() = 0;
	virtual Res<>    EndFrame(Image presentImage) = 0;
	virtual Res<>    CmdBeginRendering(Image colorImage, Image depthImage) = 0;
	virtual Res<>    CmdEndRendering() = 0;
	virtual Res<>    CmdCopyBuffer(Buffer srcBuffer, Buffer dstBuffer) = 0;
	virtual Res<>    CmdDraw() = 0;
	virtual Res<>    CmdBindPipeline(Pipeline pipeline) = 0;
};

Api* GetApi();

//--------------------------------------------------------------------------------------------------

}	// namespace Render
}	// namespace JC