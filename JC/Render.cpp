#include "JC/Render.h"

#include "JC/Array.h"
#include "JC/Gpu.h"
#include "JC/File.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Log.h"
#include "JC/Map.h"
#include "JC/Math.h"

#include "stb/stb_image.h"

namespace JC::Render {

//--------------------------------------------------------------------------------------------------

DefErr(Render, AtlasFmt);
DefErr(Render, DuplicateSpriteName);
DefErr(Render, ImageFmt);
DefErr(Render, LoadImage);
DefErr(Render, SpriteNotFound);

//--------------------------------------------------------------------------------------------------

struct SpriteObj {
	U32  imageIdx;
	Vec2 uv1;
	Vec2 uv2;
	Vec2 size;
	Str  name;
};

struct Scene {
	Mat4  projView;
	U64   drawCmdBufferAddr;
};

struct PushConstants {
	U64  sceneBufferAddr;
};

struct DrawCmd {
	Vec2 pos;
	Vec2 size;
	Vec2 uv1;
	Vec2 uv2;
	Vec4 color;
	Vec4 borderColor;
	F32  border;
	F32  cornerRadius;
	F32  rotation;
	U32  textureIdx;
};

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxDrawCmds = 1 * 1024 * 1024;

struct DBuffer {
	U32         size;
	Gpu::Buffer buffers[Gpu::MaxFrames];
	U64         bufferAddrs[Gpu::MaxFrames];
	Gpu::Buffer stagingBuffers[Gpu::MaxFrames];
	void*       stagingPtrs[Gpu::MaxFrames];
};

static Mem::Allocator*     allocator;
static Mem::TempAllocator* tempAllocator;
static Log::Logger*        logger;
static U32                 windowWidth;
static U32                 windowHeight;
static U32                 frameIdx;
static Gpu::Image          depthImage;
static Gpu::Shader         vertexShader;
static Gpu::Shader         fragmentShader;
static Gpu::Pipeline       pipeline;
static DBuffer             sceneBuffer;
static DBuffer             drawCmdBuffer;
static U64                 drawCmdCount;
static Array<Gpu::Image>   spriteImages;
static Array<SpriteObj>    spriteObjs;
static Map<Str, U32>       spriteObjsByName;

//--------------------------------------------------------------------------------------------------

void DestroyDBuffer(DBuffer dbuffer) {
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		Gpu::DestroyBuffer(dbuffer.buffers[i]);
		Gpu::DestroyBuffer(dbuffer.stagingBuffers[i]);
	}
};

static Res<DBuffer> CreateDBuffer(U32 size) {
	DBuffer dbuffer;
	dbuffer.size = size;
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		if (Res<> r = Gpu::CreateBuffer(size, Gpu::BufferUsage::Storage).To(dbuffer.buffers[i]); !r) {
			DestroyDBuffer(dbuffer);
			return r.err;
		}
		dbuffer.bufferAddrs[i] = Gpu::GetBufferAddr(dbuffer.buffers[i]);
		if (Res<> r = Gpu::CreateBuffer(size, Gpu::BufferUsage::Staging).To(dbuffer.stagingBuffers[i]); !r) {
			DestroyDBuffer(dbuffer);
			return r.err;
		}
		if (Res<> r = Gpu::MapBuffer(dbuffer.stagingBuffers[i], 0, size).To(dbuffer.stagingPtrs[i]); !r) {
			DestroyDBuffer(dbuffer);
			return r.err;
		}
	}
	return dbuffer;
}

static void FillDBuffer(DBuffer* dbuffer, U32 size) {
	Gpu::BufferBarrier(dbuffer->buffers[frameIdx], Gpu::Stage::VertexShaderRead, Gpu::Stage::TransferDst);
	Gpu::CopyBuffer(dbuffer->stagingBuffers[frameIdx], 0, dbuffer->buffers[frameIdx], 0, (U64)size);
	Gpu::BufferBarrier(dbuffer->buffers[frameIdx], Gpu::Stage::TransferDst, Gpu::Stage::VertexShaderRead);
}

//--------------------------------------------------------------------------------------------------

static Res<Gpu::Shader> LoadShader(Str path) {
	Span<U8> data;
	if (Res<> r = File::ReadAll(tempAllocator, path).To(data); !r) { return r.err; }
	return Gpu::CreateShader(data.data, data.len);
}

//--------------------------------------------------------------------------------------------------

Res<> Init(const InitDesc* initDesc) {
	allocator     = initDesc->allocator;
	tempAllocator = initDesc->tempAllocator;
	logger        = initDesc->logger;
	windowWidth   = initDesc->windowWidth;
	windowHeight  = initDesc->windowHeight;

	spriteImages.Init(allocator);
	spriteObjs.Init(allocator);
	spriteObjs.Add(SpriteObj{});	// reserve 0 for invalid
	spriteObjsByName.Init(allocator);

	if (Res<> r = Gpu::CreateImage(windowWidth, windowHeight, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::DepthAttachment).To(depthImage); !r) { return r; }

	if (Res<> r = LoadShader("Shaders/Vert.spv").To(vertexShader); !r) { return r; }
	if (Res<> r = LoadShader("Shaders/Frag.spv").To(fragmentShader); !r) { return r; }
	if (Res<> r = Gpu::CreateGraphicsPipeline({ vertexShader, fragmentShader }, { Gpu::GetImageFormat(Gpu::GetSwapchainImage()) }, Gpu::ImageFormat::D32_Float).To(pipeline); !r) { return r; }
	if (Res<> r = CreateDBuffer(sizeof(Scene)).To(sceneBuffer); !r) { return r; }
	if (Res<> r = CreateDBuffer(MaxDrawCmds * sizeof(DrawCmd)).To(drawCmdBuffer); !r) { return r; }

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	for (U64 i = 0; i < spriteImages.len; i++) {
		Gpu::DestroyImage(spriteImages[i]);
	}
	DestroyDBuffer(sceneBuffer);
	DestroyDBuffer(drawCmdBuffer);
	Gpu::DestroyPipeline(pipeline);
	Gpu::DestroyShader(vertexShader);
	Gpu::DestroyShader(fragmentShader);
	Gpu::DestroyImage(depthImage);
}

//--------------------------------------------------------------------------------------------------

Res<> WindowResized(U32 inWindowWidth, U32 inWindowHeight) {
	windowWidth  = inWindowWidth;
	windowHeight = inWindowHeight;

	if (Res<> r = Gpu::RecreateSwapchain(windowWidth, windowHeight); !r) {
		return r;
	}

	Gpu::WaitIdle();
	Gpu::DestroyImage(depthImage);
	if (Res<> r = Gpu::CreateImage(windowWidth, windowHeight, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::DepthAttachment).To(depthImage); !r) {
		return r;
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<Gpu::Image> LoadImage(Str path) {
	Span<U8> data;
	if (Res<> r = File::ReadAll(Mem::tempAllocator, path).To(data); !r) { return r.err; }

	int width = 0;
	int height = 0;
	int channels = 0;
	U8* imageData = (U8*)stbi_load_from_memory(data.data, (int)data.len, &width, &height, &channels, 0);
	if (!imageData) {
		return Err_LoadImage("path", path, "desc", stbi_failure_reason());
	}
	if (channels != 3 && channels != 4) {
		return Err_ImageFmt("path", path, "channels", channels);
	}
	Defer { stbi_image_free(imageData); };

	Gpu::Buffer stagingBuffer;
	if (Res<> r = Gpu::CreateBuffer(width * height * 4, Gpu::BufferUsage::Staging).To(stagingBuffer); !r) {
		return r.err;
	}
	Defer { Gpu::DestroyBuffer(stagingBuffer); };

	void* stagingPtr;
	if (Res<> r = Gpu::MapBuffer(stagingBuffer, 0, 0).To(stagingPtr); !r) {
		return r.err;
	}

	if (channels == 4) {
		memcpy(stagingPtr, imageData, width * height * channels);
	} else {
		U8* in = imageData;
		U8* out = (U8*)stagingPtr;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				*out++ = *in++;
				*out++ = *in++;
				*out++ = *in++;
				*out++ = 0xff;
			}
		}
	}

	Gpu::Image image;
	if (Res<> r = Gpu::CreateImage(width, height, Gpu::ImageFormat::R8G8B8A8_UNorm, Gpu::ImageUsage::Sampled).To(image); !r) {
		return r.err;
	}

	Gpu::ImageBarrier(image, Gpu::Stage::None, Gpu::Stage::TransferDst);
	Gpu::CopyBufferToImage(stagingBuffer, 0, image);
	Gpu::ImageBarrier(image, Gpu::Stage::TransferDst, Gpu::Stage::FragmentShaderSample);

	return image;
}

//--------------------------------------------------------------------------------------------------

Res<> LoadSpriteAtlas(Str imagePath, Str atlasPath) {
	Gpu::Image image;
	if (Res<> r = LoadImage(imagePath).To(image); !r) { return r; }
	const U32 imageIdx = Gpu::BindImage(image);
	spriteImages.Add(image);

	Span<U8> data;
	if (Res<> r = File::ReadAll(Mem::tempAllocator, atlasPath).To(data); !r) { return r.err; }	// TODO: ctx
	Json::Doc* doc = 0;
	if (Res<> r = Json::Parse(Mem::tempAllocator, Mem::tempAllocator, Str((const char*)data.data, data.len)).To(doc); !r) { return r.err; }	// TODO: ctx

	Json::Elem root = Json::GetRoot(doc);
	if (root.type != Json::Type::Arr) { return Err_AtlasFmt(); }	// TODO: ctx
	Span<Json::Elem> elems = Json::GetArr(doc, root);

	for (U64 i = 0; i < elems.len; i++) {
		if (elems[i].type != Json::Type::Obj) { return Err_AtlasFmt(); }	// TODO: ctx
		Json::Obj obj = Json::GetObj(doc, elems[i]);
		if (obj.keys.len != 5) { return Err_AtlasFmt(); }
		if (Json::GetStr(doc, obj.keys[0]) != "x"   ) { return Err_AtlasFmt(); }
		if (Json::GetStr(doc, obj.keys[1]) != "y"   ) { return Err_AtlasFmt(); }
		if (Json::GetStr(doc, obj.keys[2]) != "w"   ) { return Err_AtlasFmt(); }
		if (Json::GetStr(doc, obj.keys[3]) != "h"   ) { return Err_AtlasFmt(); }
		if (Json::GetStr(doc, obj.keys[4]) != "name") { return Err_AtlasFmt(); }

		const U64 ix   = Json::GetU64(doc, obj.vals[0]);
		const U64 iy   = Json::GetU64(doc, obj.vals[1]);
		const U64 iw   = Json::GetU64(doc, obj.vals[2]);
		const U64 ih   = Json::GetU64(doc, obj.vals[3]);
		const Str name = Json::GetStr(doc, obj.vals[4]);

		const F32 imageWidth = (F32)Gpu::GetImageWidth(image);
		const F32 imageHeight = (F32)Gpu::GetImageHeight(image);
		const F32 x = (F32)ix / imageWidth;
		const F32 y = (F32)iy / imageHeight;
		const F32 w = (F32)iw;
		const F32 h = (F32)ih;

		if (spriteObjsByName.FindOrNull(name)) {
			return Err_DuplicateSpriteName("path", atlasPath, "name", name);
		}

		SpriteObj* const spriteObj = spriteObjs.Add(SpriteObj {
			.imageIdx = imageIdx,
			.uv1      = { x, y },
			.uv2      = { x + (w / imageWidth), y + (h / imageHeight) },
			.size     = { w, h },
			.name     = Copy(allocator, name),
		});
		spriteObjsByName.Put(spriteObj->name, (U32)(spriteObj - spriteObjs.data));
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Sprite> GetSprite(Str name) {
	const U32* const idx = spriteObjsByName.FindOrNull(name);
	if (!idx) {
		return Err_SpriteNotFound();
	}
	return Sprite { .handle = (U64)(*idx) };
}

//--------------------------------------------------------------------------------------------------

Vec2 GetSpriteSize(Sprite sprite) {
	Assert(sprite.handle < spriteObjs.len);
	return spriteObjs[sprite.handle].size;
}

//--------------------------------------------------------------------------------------------------

void BeginFrame() {
	frameIdx = Gpu::GetFrameIdx();

	drawCmdStagingPtrsCur[frameIdx] = drawCmdStagingPtrs[frameIdx];
}

//--------------------------------------------------------------------------------------------------

void EndFrame() {
	const Gpu::Image swapchainImage = Gpu::GetSwapchainImage();

	const F32 fw = (F32)windowWidth;
	const F32 fh = (F32)windowHeight;
	sceneStagingBufferPtrs[frameIdx]->projView = Math::Ortho(
		-fw / 2.0f,
		 fw / 2.0f,
		-fh / 2.0f,
		 fh / 2.0f,
		-100.0f,
		 100.0f
	);
	sceneStagingBufferPtrs[frameIdx]->drawCmdBufferAddr = drawCmdBufferAddrs[frameIdx];
	Gpu::BufferBarrier(sceneBuffers[frameIdx], Gpu::Stage::VertexShaderRead, Gpu::Stage::TransferDst);
	Gpu::CopyBuffer(sceneStagingBuffers[frameIdx], 0, sceneBuffers[frameIdx], 0, sizeof(Scene));
	Gpu::BufferBarrier(sceneBuffers[frameIdx], Gpu::Stage::TransferDst, Gpu::Stage::VertexShaderRead);

	Gpu::BufferBarrier(drawCmdBuffers[frameIdx], Gpu::Stage::VertexShaderRead, Gpu::Stage::TransferDst);
	Gpu::CopyBuffer(drawCmdStagingBuffers[frameIdx], 0, drawCmdBuffers[frameIdx], 0, drawCmdCounts[frameIdx] * sizeof(DrawCmd));
	Gpu::BufferBarrier(drawCmdBuffers[frameIdx], Gpu::Stage::TransferDst, Gpu::Stage::VertexShaderRead);

	const Gpu::Pass pass = {
		.pipeline         = pipeline,
		.colorAttachments = { swapchainImage },
		.depthAttachment  = depthImage,
		.viewport         = { .x = 0.0f, .y = 0.0f, .w = (F32)windowWidth, .h = (F32)windowHeight },
		.scissor          = { .x = 0,    .y = 0,    .w = windowWidth,      .h = windowHeight },
		.clear            = true,
	};
	Gpu::BeginPass(&pass);
	PushConstants pushConstants = { .sceneBufferAddr = sceneBufferAddrs[frameIdx] };
	Gpu::PushConstants(pipeline, &pushConstants, sizeof(pushConstants));
	Gpu::Draw(6, drawCmdCounts[frameIdx]);
	Gpu::EndPass();
}

//--------------------------------------------------------------------------------------------------

static void AllocStagingMem() {
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
	}

}

static DrawCmd* AllocDrawCmds(U32 n) {
	const U32 sizeNeeded = (drawCmdCounts[frameIdx] + n) * sizeof(DrawCmd);
	if (sizeNeeded > drawCmdStagingBufferSizes[frameIdx]) {
		U32 newSize = drawCmdStagingBufferSizes[frameIdx] * 2;
		while (newSize < sizeNeeded && newSize < DrawCmdStagingBufferMaxSize) {
			newSize *= 2;
		}

		Gpu::Buffer newBuffer;
		if (Res<> r = Gpu::CreateBuffer(newSize, Gpu::BufferUsage::Storage).To(newBuffer); !r) {
			return r.err;
		}

		Gpu::Buffer newStagingBuffer;
		if (Res<> r = Gpu::CreateBuffer(newSize, Gpu::BufferUsage::Staging).To(newStagingBuffer); !r) {
			Gpu::DestroyBuffer(newBuffer);
			return r.err;
		}

		const U64 newBufferAddr = Gpu::GetBufferAddr(newBuffer);

		void* newStagingBufferPtr;
		if (Res<> r = Gpu::MapBuffer(newStagingBuffer, 0, newSize).To(newStagingBufferPtr); !r) {
			Gpu::DestroyBuffer(newBuffer);
			Gpu::DestroyBuffer(newStagingBuffer);
			return r.err;
		}
		memcpy(newStagingBufferPtr, drawCmdStagingBufferPtrs[frameIdx], drawCmdCounts[frameIdx] * sizeof(DrawCmd));

		Gpu::DestroyBuffer(drawCmdBuffers[frameIdx]);
		Gpu::DestroyBuffer(drawCmdStagingBuffers[frameIdx]);

		drawCmdBuffers[frameIdx]            = newBuffer;
		drawCmdBufferAddrs[frameIdx]        = newBufferAddr;
		drawCmdStagingBuffers[frameIdx]     = newStagingBuffer;
		drawCmdStagingBufferPtrs[frameIdx]  = newStagingBufferPtr;
		drawCmdStagingBufferSizes[frameIdx] = newSize;
	}

	DrawCmd* const drawCmds = ((DrawCmd*)drawCmdStagingBufferPtrs) + drawCmdCounts[frameIdx];
	drawCmdCounts[frameIdx] += n;

	return drawCmds;
}

//--------------------------------------------------------------------------------------------------

void DrawSprite(Sprite sprite, Vec2 pos) {
	SpriteObj* const spriteObj = &spriteObjs[sprite.handle];
	DrawCmd* drawCmd;
	if (Res<> r = AllocDrawCmds(1) =  {
		.pos          = pos,
		.size         = spriteObj->size,
		.uv1          = spriteObj->uv1,
		.uv2          = spriteObj->uv2,
		.color        = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
		.borderColor  = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
		.border       = 0.0f,
		.cornerRadius = 0.0f,
		.rotation     = 0.0f,
		.textureIdx   = spriteObj->imageIdx,
	};
}

void DrawSprite(Sprite sprite, Vec2 pos, F32 scale, F32 rotation, Vec4 color) {
	SpriteObj* const spriteObj = &spriteObjs[sprite.handle];
	*AllocDrawCmds(1) =  {
		.pos          = pos,
		.size         = spriteObj->size,
		.uv1          = spriteObj->uv1,
		.uv2          = spriteObj->uv2,
		.color        = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
		.borderColor  = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
		.border       = 0.0f,
		.cornerRadius = 0.0f,
		.rotation     = rotation,
		.textureIdx   = spriteObj->imageIdx,
	};
}

//--------------------------------------------------------------------------------------------------

void DrawRect(Vec2 pos, Vec2 size, Vec4 color, Vec4 borderColor, F32 border, F32 cornerRadius) {
	*AllocDrawCmds(1) =  {
		.pos          = pos,
		.size         = size,
		.uv1          = Vec2(),
		.uv2          = Vec2(),
		.color        = color,
		.borderColor  = borderColor,
		.border       = border,
		.cornerRadius = 0.0f,
		.rotation     = 0.0f,
		.textureIdx   = 0,
	};
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Render