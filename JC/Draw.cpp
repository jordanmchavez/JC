#include "JC/Draw.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/FS.h"
#include "JC/HandlePool.h"
#include "JC/Hash.h"
#include "JC/Gpu.h"
#include "JC/Json.h"
#include "JC/Log.h"
#include "JC/Map.h"
#include "JC/Math.h"

#include "stb/stb_image.h"

namespace JC::Draw {

//--------------------------------------------------------------------------------------------------

DefErr(Draw, AtlasFmt);
DefErr(Draw, DuplicateSpriteName);
DefErr(Draw, ImageFmt);
DefErr(Draw, LoadImage);
DefErr(Draw, SpriteNotFound);

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxDrawCmds          = 128 * 1024;
static constexpr U32 MaxSpriteAtlasImages = 64;
static constexpr U32 MaxSprites           = 1 * MB;
static constexpr U32 MaxCanvases          = 64;
static constexpr U32 MaxPasses            = 64;

//--------------------------------------------------------------------------------------------------

struct SpriteAtlasEntry {
	U32 x;
	U32 y;
	U32 w;
	U32 h;
	Str name;
};
Json_Begin(SpriteAtlasEntry)
	Json_Member("x",    x)
	Json_Member("y",    y)
	Json_Member("w",    w)
	Json_Member("h",    h)
	Json_Member("name", name)
Json_End(SpriteAtlasEntry)

struct SpriteObj {
	U32  imageIdx;
	Vec2 uv1;
	Vec2 uv2;
	Vec2 size;
	Str  name;
};

struct Scene {
	Mat4 projViews[MaxCanvases + 1];	// +1 for the no-canvas pass
};

struct PushConstants {
	U64 sceneBufferAddr;
	U64 drawCmdBufferAddr;
	U32 sceneBufferIdx;
	U32 drawCmdStart;
};

struct DrawCmd {
	Vec2 pos;
	Vec2 size;
	Vec2 uv1;
	Vec2 uv2;
	Vec4 color;
	F32  rotation;
	U32  textureIdx;
};

struct CanvasObj {
	Gpu::Image       colorImage;
	U32              colorImageIdx;
	Gpu::ImageLayout colorImageLayout;
	Gpu::Image       depthImage;
	Vec2             size;
};

using CanvasPool = HandlePool<CanvasObj, Canvas>;

struct Pass {
	Canvas canvas;
	U32    drawCmdStart;
	U32    drawCmdEnd;
};

//--------------------------------------------------------------------------------------------------

static Mem           permMem;
static Mem           tempMem;
static U32           windowWidth;
static U32           windowHeight;
static Gpu::Image    depthImage;
static Gpu::Shader   vertexShader;
static Gpu::Shader   fragmentShader;
static Gpu::Pipeline pipeline;
static Gpu::Buffer   sceneBuffers[Gpu::MaxFrames];
static void*         sceneBufferPtrs[Gpu::MaxFrames];
static U64           sceneBufferAddrs[Gpu::MaxFrames];
static Scene         scene;
static Gpu::Buffer   drawCmdBuffers[Gpu::MaxFrames];
static void*         drawCmdBufferPtrs[Gpu::MaxFrames];
static U64           drawCmdBufferAddrs[Gpu::MaxFrames];
static DrawCmd*      drawCmds;
static U32           drawCmdCount;
static CanvasPool    canvasObjs;
static Canvas        swapchainCanvas;
static Pass*         passes;
static U32           passesLen;
static Gpu::Image*   spriteAtlasImages;
static U32           spriteAtlasImagesLen;
static SpriteObj*    spriteObjs;
static U32           spriteObjsLen;
static Map<Str, U32> spriteObjsByName;

//--------------------------------------------------------------------------------------------------

static Res<Gpu::Shader> LoadShader(Str path) {
	Span<U8> data;
	if (Res<> r = FS::ReadAll(tempMem, path).To(data); !r) { return r.err; }
	return Gpu::CreateShader(data.data, data.len);
}

//--------------------------------------------------------------------------------------------------

Res<> Init(InitDesc const* initDesc) {
	permMem      = initDesc->permMem;
	tempMem      = initDesc->tempMem;
	windowWidth  = initDesc->windowWidth;
	windowHeight = initDesc->windowHeight;

	canvasObjs.Init(permMem, MaxCanvases);
	passes               = Mem::AllocT<Pass>(permMem, MaxPasses);
	passesLen            = 0;
	spriteAtlasImages    = Mem::AllocT<Gpu::Image>(permMem, MaxSpriteAtlasImages);
	spriteAtlasImagesLen = 0;
	spriteObjs           = Mem::AllocT<SpriteObj>(permMem, MaxSprites);
	spriteObjsLen        = 1;	// reserve 0 for invalid since this is the handle value
	spriteObjsByName.Init(permMem, MaxSprites);

	Try(Gpu::CreateImage(windowWidth, windowHeight, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::Depth).To(depthImage));
	Gpu_Name(depthImage);

	Try(LoadShader("Shaders/SpriteVert.spv").To(vertexShader));
	Gpu_Name(vertexShader);
	Try(LoadShader("Shaders/SpriteFrag.spv").To(fragmentShader));
	Gpu_Name(fragmentShader);

	Try(Gpu::CreateGraphicsPipeline(
		{ vertexShader, fragmentShader },
		{ Gpu::GetSwapchainImageFormat() },
		Gpu::ImageFormat::D32_Float
	).To(pipeline));
	Gpu_Name(pipeline);

	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		TryTo(Gpu::CreateBuffer(sizeof(Scene), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Addr, Gpu::MemoryLocation::Cpu), sceneBuffers[i]);
		Gpu_Namef(sceneBuffers[i], "sceneBuffers[%u]", i);
		TryTo(Gpu::MapBuffer(sceneBuffers[i]), sceneBufferPtrs[i]);
		sceneBufferAddrs[i] = Gpu::GetBufferGpuAddr(sceneBuffers[i]);

		TryTo(Gpu::CreateBuffer(MaxDrawCmds * sizeof(DrawCmd), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Addr, Gpu::MemoryLocation::Cpu), drawCmdBuffers[i]);
		Gpu_Namef(drawCmdBuffers[i], "drawCmdBuffers[%u]", i);
		TryTo(Gpu::MapBuffer(drawCmdBuffers[i]), drawCmdBufferPtrs[i]);
		drawCmdBufferAddrs[i] = Gpu::GetBufferGpuAddr(drawCmdBuffers[i]);
	}

	Vec2 const windowSize = Vec2((F32)windowWidth, (F32)windowHeight);
	CanvasPool::Entry* const entry = canvasObjs.Alloc();
	entry->obj = {
		.colorImage       = Gpu::Image(),
		.colorImageIdx    = 0,
		.colorImageLayout = Gpu::ImageLayout::Color,
		.depthImage       = depthImage,
		.size             = windowSize,
	};
	swapchainCanvas = entry->Handle();
	scene.projViews[entry->idx] = Math::Ortho(
		0.0f, windowSize.x,
		windowSize.y, 0.0f,
		-100.0f, 100.0f
	);

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	for (U32 i = 0; i < spriteAtlasImagesLen; i++) {
		Gpu::DestroyImage(spriteAtlasImages[i]);
	}
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		Gpu::DestroyBuffer(sceneBuffers[i]);
		Gpu::DestroyBuffer(drawCmdBuffers[i]);
	}
	Gpu::DestroyPipeline(pipeline);
	Gpu::DestroyShader(vertexShader);
	Gpu::DestroyShader(fragmentShader);
	Gpu::DestroyImage(depthImage);
}

//--------------------------------------------------------------------------------------------------

Res<> ResizeWindow(U32 windowWidthIn, U32 windowHeightIn) {
	windowWidth  = windowWidthIn;
	windowHeight = windowHeightIn;

	Try(Gpu::RecreateSwapchain(windowWidth, windowHeight));
	Gpu::WaitIdle();
	Gpu::DestroyImage(depthImage);
	Try(Gpu::CreateImage(windowWidth, windowHeight, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::Depth).To(depthImage));

	Vec2 const windowSize = Vec2((F32)windowWidth, (F32)windowHeight);
	scene.projViews[canvasObjs.GetEntry(swapchainCanvas)->idx] = Math::Ortho(
		0.0f, windowSize.x,
		windowSize.y, 0.0f,
		-100.0f, 100.0f
	);

	CanvasObj* const canvasObj = canvasObjs.Get(swapchainCanvas);
	canvasObj->depthImage = depthImage;
	canvasObj->size = windowSize;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<Gpu::Image> LoadImage(Str path) {
	Span<U8> data;
	if (Res<> r = FS::ReadAll(tempMem, path).To(data); !r) { return r.err; }

	int width = 0;
	int height = 0;
	U8* imageData = (U8*)stbi_load_from_memory(data.data, (int)data.len, &width, &height, 0, 4);
	if (!imageData) {
		return Err_LoadImage("path", path, "desc", stbi_failure_reason());
	}
	Defer { stbi_image_free(imageData); };

	Gpu::Image image;
	Try(Gpu::CreateImage(width, height, Gpu::ImageFormat::R8G8B8A8_UNorm, Gpu::ImageUsage::Sampled | Gpu::ImageUsage::Copy).To(image));
	Try(Gpu::ImmediateCopyToImage(imageData, image, Gpu::BarrierStage::VertexShader_SamplerRead, Gpu::ImageLayout::ShaderRead));

	return image;
}

//--------------------------------------------------------------------------------------------------

Res<> LoadSpriteAtlas(Str imagePath, Str atlasPath) {
	Assert(spriteAtlasImagesLen < MaxSpriteAtlasImages);

	Gpu::Image image; TryTo(LoadImage(imagePath), image);
	U32 const imageIdx = Gpu::GetImageBindIdx(image);
	F32 const imageWidth  = (F32)Gpu::GetImageWidth(image);
	F32 const imageHeight = (F32)Gpu::GetImageHeight(image);
	spriteAtlasImages[spriteAtlasImagesLen++] = image;

	Span<char> json; TryTo(FS::ReadAllZ(tempMem, atlasPath), json);
	Span<SpriteAtlasEntry> entries; Try(Json::ToArray(tempMem, tempMem, json.data, (U32)json.len, &entries));
	Assert(spriteObjsLen + entries.len <= MaxSprites);
	for (U64 i = 0; i < entries.len; i++) {
		SpriteAtlasEntry* const entry = &entries[i];
		if (spriteObjsByName.FindOrNull(entry->name)) {
			return Err_DuplicateSpriteName("path", atlasPath, "name", entry->name);
		}
		F32 const x = (F32)entry->x / imageWidth;
		F32 const y = (F32)entry->y / imageHeight;
		F32 const w = (F32)entry->w;
		F32 const h = (F32)entry->h;

		spriteObjsByName.Put(entry->name, spriteObjsLen);
		spriteObjs[spriteObjsLen++] = {
			.imageIdx = imageIdx,
			.uv1      = { x, y },
			.uv2      = { x + (w / imageWidth), y + (h / imageHeight) },
			.size     = { w, h },
			.name     = entry->name,
		};
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Sprite> GetSprite(Str name) {
	U32 const* const idx = spriteObjsByName.FindOrNull(name);
	if (!idx) {
		return Err_SpriteNotFound();
	}
	return Sprite { .handle = (U64)(*idx) };
}

//--------------------------------------------------------------------------------------------------

Vec2 GetSpriteSize(Sprite sprite) {
	Assert(sprite.handle > 0 && sprite.handle < spriteObjsLen);
	return spriteObjs[sprite.handle].size;
}

//--------------------------------------------------------------------------------------------------

Res<Canvas> CreateCanvas(U32 width, U32 height) {
	Gpu::Image canvasColorImage;
	Try(Gpu::CreateImage(width, height, Gpu::ImageFormat::B8G8R8A8_UNorm, Gpu::ImageUsage::Color | Gpu::ImageUsage::Sampled).To(canvasColorImage));

	Gpu::Image canvasDepthImage;
	if (Res<> r = Gpu::CreateImage(width, height, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::Depth).To(canvasDepthImage); !r) {
		Gpu::DestroyImage(canvasColorImage);
		return r.err;
	}

	Vec2 const size = Vec2((F32)width, (F32)height);
	CanvasPool::Entry* const entry = canvasObjs.Alloc();
	entry->obj = {
		.colorImage       = canvasColorImage,
		.colorImageIdx    = Gpu::GetImageBindIdx(canvasColorImage),
		.colorImageLayout = Gpu::ImageLayout::Undefined,
		.depthImage       = canvasDepthImage,
		.size             = size,
	};
	Gpu_Namef(canvasColorImage, "canvasColor#%u", entry->idx);
	Gpu_Namef(canvasDepthImage, "canvasDepth#%u", entry->idx);

	scene.projViews[entry->idx] = Math::Ortho(
		0.0f, size.x,
		size.y, 0.0f,
		-100.0f, 100.0f
	);

	return entry->Handle();
}

//--------------------------------------------------------------------------------------------------

void DestroyCanvas(Canvas canvas) {
	CanvasObj* const canvasObj = canvasObjs.Get(canvas);
	Gpu::DestroyImage(canvasObj->colorImage);
	Gpu::DestroyImage(canvasObj->depthImage);
	canvasObjs.Free(canvas);
}

//--------------------------------------------------------------------------------------------------

static U64 frameIdx;

void BeginFrame(const Gpu::Frame* frame) {
	frameIdx = frame->frameIdx;
	CanvasObj* const canvasObj = canvasObjs.Get(swapchainCanvas);
	canvasObj->colorImage = frame->swapchainImage;

	drawCmds = (DrawCmd*)drawCmdBufferPtrs[frameIdx];
	drawCmdCount = 0;

	passesLen = 0;
	passes[passesLen++] = {
		.canvas       = swapchainCanvas,
		.drawCmdStart = 0,
		.drawCmdEnd   = U32Max,
	};

}

//--------------------------------------------------------------------------------------------------

void EndFrame() {
	memcpy(sceneBufferPtrs[frameIdx], &scene, sizeof(Scene));

	passes[passesLen - 1].drawCmdEnd = drawCmdCount;
	for (U64 i = 0; i < passesLen; i++) {
		Pass const* const pass = &passes[i];
		U32 const passDrawCmdCount = pass->drawCmdEnd - pass->drawCmdStart;
		if (passDrawCmdCount == 0) {
			continue;
		}

		CanvasPool::Entry* const entry = canvasObjs.GetEntry(pass->canvas);
		CanvasObj* canvasObj = &entry->obj;
		Gpu::Pass const gpuPass = {
			.pipeline         = pipeline,
			.colorAttachments = { canvasObj->colorImage },
			.depthAttachment  = canvasObj->depthImage,
			.viewport         = { .x = 0.0f, .y = 0.0f, .width = canvasObj->size.x,      .height = canvasObj->size.y },
			.scissor          = { .x = 0,    .y = 0,    .width = (U32)canvasObj->size.x, .height = (U32)canvasObj->size.y },
			.clear            = true,
		};

		if (pass->canvas.handle != swapchainCanvas.handle) {
			if (canvasObj->colorImageLayout == Gpu::ImageLayout::Undefined) {
				Gpu::ImageBarrier(
					canvasObj->colorImage,
					Gpu::BarrierStage::None,
					Gpu::ImageLayout::Undefined,
					Gpu::BarrierStage::ColorOutput_ColorWrite,
					Gpu::ImageLayout::Color
				);
			} else if (canvasObj->colorImageLayout == Gpu::ImageLayout::ShaderRead) {
				Gpu::ImageBarrier(
					canvasObj->colorImage,
					Gpu::BarrierStage::FragmentShader_SamplerRead,
					Gpu::ImageLayout::ShaderRead,
					Gpu::BarrierStage::ColorOutput_ColorWrite,
					Gpu::ImageLayout::Color
				);
			}
			canvasObj->colorImageLayout = Gpu::ImageLayout::Color;
		}

		Gpu::BeginPass(&gpuPass);
		PushConstants pushConstants = {
			.sceneBufferAddr   = sceneBufferAddrs[frameIdx],
			.drawCmdBufferAddr = drawCmdBufferAddrs[frameIdx],
			.sceneBufferIdx    = entry->idx,
			.drawCmdStart      = pass->drawCmdStart,
		};
		Gpu::PushConstants(pipeline, &pushConstants, sizeof(pushConstants));
		Gpu::Draw(6, passDrawCmdCount);
		Gpu::EndPass();

		if (pass->canvas.handle != swapchainCanvas.handle) {
			Gpu::ImageBarrier(
				canvasObj->colorImage,
				Gpu::BarrierStage::ColorOutput_ColorWrite,
				Gpu::ImageLayout::Color,
				Gpu::BarrierStage::VertexShader_SamplerRead,
				Gpu::ImageLayout::ShaderRead
			);
			canvasObj->colorImageLayout = Gpu::ImageLayout::ShaderRead;
		}
	}
}

//--------------------------------------------------------------------------------------------------

void SetDefaultCanvas() {
	passes[passesLen - 1].drawCmdEnd = drawCmdCount;
	passes[passesLen++] = {
		.canvas       = swapchainCanvas,
		.drawCmdStart = drawCmdCount,
		.drawCmdEnd   = U32Max,
	};
}

void SetCanvas(Canvas canvas) {
	passes[passesLen - 1].drawCmdEnd = drawCmdCount;
	passes[passesLen++] = {
		.canvas       = canvas.handle ? canvas : swapchainCanvas,
		.drawCmdStart = drawCmdCount,
		.drawCmdEnd   = U32Max,
	};
}

//--------------------------------------------------------------------------------------------------

static DrawCmd* AllocDrawCmds(U32 n) {
	Assert(drawCmdCount + n <= MaxDrawCmds);
	DrawCmd* ptr = drawCmds + drawCmdCount;
	drawCmdCount += n;
	return ptr;
}

//--------------------------------------------------------------------------------------------------

void DrawSprite(Sprite sprite, Vec2 pos) {
	Assert(sprite.handle > 0 && sprite.handle < spriteObjsLen);
	SpriteObj* const spriteObj = &spriteObjs[sprite.handle];
	DrawCmd* const drawCmd = AllocDrawCmds(1);
	*drawCmd =  {
		.pos          = pos,
		.size         = spriteObj->size,
		.uv1          = spriteObj->uv1,
		.uv2          = spriteObj->uv2,
		.color        = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
		.rotation     = 0.0f,
		.textureIdx   = spriteObj->imageIdx,
	};
}

void DrawSprite(Sprite sprite, Vec2 pos, Vec2 scale, F32 rotation, Vec4 color) {
	Assert(sprite.handle > 0 && sprite.handle < spriteObjsLen);
	SpriteObj* const spriteObj = &spriteObjs[sprite.handle];
	DrawCmd* const drawCmd = AllocDrawCmds(1);
	*drawCmd =  {
		.pos          = pos,
		.size         = Math::Mul(spriteObj->size, scale),
		.uv1          = spriteObj->uv1,
		.uv2          = spriteObj->uv2,
		.color        = color,
		.rotation     = rotation,
		.textureIdx   = spriteObj->imageIdx,
	};
}

//--------------------------------------------------------------------------------------------------

void DrawRect(Vec2 pos, Vec2 size, Vec4 color) {
	DrawCmd* const drawCmd = AllocDrawCmds(1);
	*drawCmd =  {
		.pos          = pos,
		.size         = size,
		.uv1          = Vec2(),
		.uv2          = Vec2(),
		.color        = color,
		.rotation     = 0.0f,
		.textureIdx   = 0,
	};
}

//--------------------------------------------------------------------------------------------------

void DrawCanvas(Canvas canvas, Vec2 pos, Vec2 scale) {
	CanvasObj* const canvasObj = canvasObjs.Get(canvas);
	DrawCmd* const drawCmd = AllocDrawCmds(1);
	*drawCmd =  {
		.pos          = pos,
		.size         = Math::Mul(canvasObj->size, scale),
		.uv1          = Vec2(0.f, 0.f),
		.uv2          = Vec2(1.f, 1.f),
		.color        = Vec4(1.f, 1.f, 1.f, 1.f),
		.rotation     = 0.0f,
		.textureIdx   = canvasObj->colorImageIdx
	};
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Draw