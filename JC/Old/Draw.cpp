#include "JC/Draw.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/FS.h"
#include "JC/Gpu.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Log.h"
#include "JC/Map.h"
#include "JC/Math.h"
#include "JC/Pool.h"

#include "stb/stb_image.h"

//--------------------------------------------------------------------------------------------------

Err_Def(Draw, AtlasFmt);
Err_Def(Draw, DuplicateSpriteName);
Err_Def(Draw, ImageFmt);
Err_Def(Draw, LoadImage);
Err_Def(Draw, SpriteNotFound);

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxDrawCmds = 1 * 1024 * 1024;
static constexpr U32 MaxCanvases = 64;

//--------------------------------------------------------------------------------------------------

struct SpriteObj {
	U32  imageIdx;
	Vec2 uv1;
	Vec2 uv2;
	Vec2 size;
	Str  name;
};

struct Scene {
	Mat4  projViews[MaxCanvases + 1];	// +1 for the
};

struct PushConstants {
	U64  sceneBufferAddr;
	U64  drawCmdBufferAddr;
	U32  sceneBufferIdx;
	U32  drawCmdStart;
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

struct CanvasObj {
	Gpu::Image       colorImage;
	U32              colorImageIdx;
	Gpu::ImageLayout colorImageLayout;
	Gpu::Image       depthImage;
	Vec2             size;
};

using CanvasPool = HandlePool<CanvasObj, Canvas, MaxCanvases>;

struct Pass {
	Canvas canvas;
	U32    drawCmdStart;
	U32    drawCmdEnd;
};

//--------------------------------------------------------------------------------------------------

static U32                windowWidth;
static U32                windowHeight;
static Gpu::Image         depthImage;
static Gpu::Shader        vertexShader;
static Gpu::Shader        fragmentShader;
static Gpu::Pipeline      pipeline;
static Gpu::Buffer        sceneBuffer;
static U64                sceneBufferAddr;
static Scene              scene;
static Gpu::Buffer        drawCmdBuffer;
static U64                drawCmdBufferAddr;
static DrawCmd*           drawCmds;
static U32                drawCmdCount;
static CanvasPool         canvasObjs;
static Canvas             swapchainCanvas;
static Array<Gpu::Image>  spriteImages;
static Array<SpriteObj>   spriteObjs;
static Map<Str, U32>      spriteObjsByName;
static Array<Pass>        passes;

//--------------------------------------------------------------------------------------------------

static Res<Gpu::Shader> LoadShader(Str path) {
	Span<U8> data;
	if (Res<> r = FS::ReadAll(tempAllocator, path).To(data); !r) { return r.err; }
	return Gpu::CreateShader(data.data, data.len);
}

//--------------------------------------------------------------------------------------------------

Res<> Init(const InitDesc* initDesc) {
	allocator     = initDesc->allocator;
	tempAllocator = initDesc->tempAllocator;
	logger        = initDesc->logger;
	windowWidth   = initDesc->windowWidth;
	windowHeight  = initDesc->windowHeight;

	JC_CHECK_RES(Gpu::CreateImage(windowWidth, windowHeight, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::Depth).To(depthImage));
	JC_GPU_NAME(depthImage);

	JC_CHECK_RES(LoadShader("Shaders/SpriteVert.spv").To(vertexShader));
	JC_GPU_NAME(vertexShader);
	JC_CHECK_RES(LoadShader("Shaders/SpriteFrag.spv").To(fragmentShader));
	JC_GPU_NAME(fragmentShader);

	JC_CHECK_RES(Gpu::CreateGraphicsPipeline(
		{ vertexShader, fragmentShader },
		{ Gpu::GetSwapchainImageFormat() },
		Gpu::ImageFormat::D32_Float
	).To(pipeline));
	JC_GPU_NAME(pipeline);

	JC_CHECK_RES(Gpu::CreateBuffer(sizeof(Scene), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Addr | Gpu::BufferUsage::Copy).To(sceneBuffer));
	JC_GPU_NAME(sceneBuffer);
	sceneBufferAddr = Gpu::GetBufferAddr(sceneBuffer);

	JC_CHECK_RES(Gpu::CreateBuffer(MaxDrawCmds * sizeof(DrawCmd), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Addr | Gpu::BufferUsage::Copy).To(drawCmdBuffer));
	JC_GPU_NAME(drawCmdBuffer);
	drawCmdBufferAddr = Gpu::GetBufferAddr(drawCmdBuffer);

	const Vec2 windowSize = Vec2((F32)windowWidth, (F32)windowHeight);
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


	spriteImages.Init(allocator);
	spriteObjs.Init(allocator);
	spriteObjs.Add(SpriteObj{});	// reserve 0 for invalid
	spriteObjsByName.Init(allocator);
	passes.Init(allocator);

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	for (U64 i = 0; i < spriteImages.len; i++) {
		Gpu::DestroyImage(spriteImages[i]);
	}
	Gpu::DestroyBuffer(sceneBuffer);
	Gpu::DestroyBuffer(drawCmdBuffer);
	Gpu::DestroyPipeline(pipeline);
	Gpu::DestroyShader(vertexShader);
	Gpu::DestroyShader(fragmentShader);
	Gpu::DestroyImage(depthImage);
}

//--------------------------------------------------------------------------------------------------

Res<> WindowResized(U32 inWindowWidth, U32 inWindowHeight) {
	windowWidth  = inWindowWidth;
	windowHeight = inWindowHeight;

	JC_CHECK_RES(Gpu::RecreateSwapchain(windowWidth, windowHeight));
	Gpu::WaitIdle();
	Gpu::DestroyImage(depthImage);
	JC_CHECK_RES(Gpu::CreateImage(windowWidth, windowHeight, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::Depth).To(depthImage));

	const Vec2 windowSize = Vec2((F32)windowWidth, (F32)windowHeight);
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
	if (Res<> r = FS::ReadAll(tempAllocator, path).To(data); !r) { return r.err; }

	int width = 0;
	int height = 0;
	int channels = 0;
	U8* imageData = (U8*)stbi_load_from_memory(data.data, (int)data.len, &width, &height, &channels, 0);
	if (!imageData) {
		return Err_LoadImage("path", path, "desc", stbi_failure_reason());
	}
	JC_DEFER { stbi_image_free(imageData); };

	if (channels != 3 && channels != 4) {
		return Err_ImageFmt("path", path, "channels", channels);
	}

	U8* copySrc = imageData;
	if (channels != 4) {
		copySrc = (U8*)tempAllocator->Alloc(width * height * 4);
		U8* imageDataIter = imageData;
		U8* copySrcIter   = copySrc;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				*copySrcIter++ = *imageDataIter++;
				*copySrcIter++ = *imageDataIter++;
				*copySrcIter++ = *imageDataIter++;
				*copySrcIter++ = 0xff;
			}
		}
	}

	Gpu::Image image;
	JC_CHECK_RES(Gpu::CreateImage(width, height, Gpu::ImageFormat::R8G8B8A8_UNorm, Gpu::ImageUsage::Sampled | Gpu::ImageUsage::Copy).To(image));
	JC_CHECK_RES(Gpu::ImmediateCopyToImage(copySrc, image, Gpu::BarrierStage::VertexShader_SamplerRead, Gpu::ImageLayout::ShaderRead));

	return image;
}

//--------------------------------------------------------------------------------------------------

Res<> LoadSpriteAtlas(Str imagePath, Str atlasPath) {
	Gpu::Image image;
	if (Res<> r = LoadImage(imagePath).To(image); !r) { return r; }
	const U32 imageIdx = Gpu::GetImageBindIdx(image);
	spriteImages.Add(image);

	Span<U8> data;
	if (Res<> r = FS::ReadAll(tempAllocator, atlasPath).To(data); !r) { return r.err; }	// TODO: ctx
	Json::Doc* doc = 0;
	if (Res<> r = Json::Parse(tempAllocator, tempAllocator, Str((const char*)data.data, data.len)).To(doc); !r) { return r.err; }	// TODO: ctx

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

		const F32 imageWidth  = (F32)Gpu::GetImageWidth(image);
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
	JC_ASSERT(sprite.handle < spriteObjs.len);
	return spriteObjs[sprite.handle].size;
}

//--------------------------------------------------------------------------------------------------

Res<Canvas> CreateCanvas(U32 width, U32 height) {
	Gpu::Image canvasColorImage;
	JC_CHECK_RES(Gpu::CreateImage(width, height, Gpu::ImageFormat::B8G8R8A8_UNorm, Gpu::ImageUsage::Color | Gpu::ImageUsage::Sampled).To(canvasColorImage));

	Gpu::Image canvasDepthImage;
	if (Res<> r = Gpu::CreateImage(width, height, Gpu::ImageFormat::D32_Float,      Gpu::ImageUsage::Depth).To(canvasDepthImage); !r) {
		Gpu::DestroyImage(canvasColorImage);
		return r.err;
	}

	const Vec2 size = Vec2((F32)width, (F32)height);
	CanvasPool::Entry* const entry = canvasObjs.Alloc();
	entry->obj = {
		.colorImage       = canvasColorImage,
		.colorImageIdx    = Gpu::GetImageBindIdx(canvasColorImage),
		.colorImageLayout = Gpu::ImageLayout::Undefined,
		.depthImage       = canvasDepthImage,
		.size             = size,
	};
	JC_GPU_NAMEF(canvasColorImage, "canvasColor#{}", entry->idx);
	JC_GPU_NAMEF(canvasDepthImage, "canvasDepth#{}", entry->idx);

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

void BeginFrame(Gpu::Frame frame) {
	CanvasObj* const canvasObj = canvasObjs.Get(swapchainCanvas);
	canvasObj->colorImage = frame.swapchainImage;

	drawCmds = (DrawCmd*)Gpu::AllocStaging(MaxDrawCmds * sizeof(DrawCmd));
	drawCmdCount = 0;

	passes.len = 0;
	passes.Add(Pass {
		.canvas       = swapchainCanvas,
		.drawCmdStart = 0,
		.drawCmdEnd   = U32Max,
	});

}

//--------------------------------------------------------------------------------------------------

void EndFrame() {
	void* const sceneStaging = Gpu::AllocStaging(sizeof(Scene));
	memcpy(sceneStaging, &scene, sizeof(Scene));
	Gpu::BufferBarrier(sceneBuffer, 0, sizeof(Scene), Gpu::BarrierStage::VertexShader_StorageRead, Gpu::BarrierStage::Copy_Write);
	Gpu::CopyStagingToBuffer(sceneStaging, sizeof(Scene), sceneBuffer, 0);
	Gpu::BufferBarrier(sceneBuffer, 0, sizeof(Scene), Gpu::BarrierStage::Copy_Write, Gpu::BarrierStage::VertexShader_StorageRead);

	Gpu::BufferBarrier(drawCmdBuffer, 0, drawCmdCount * sizeof(DrawCmd), Gpu::BarrierStage::VertexShader_StorageRead, Gpu::BarrierStage::Copy_Write);
	Gpu::CopyStagingToBuffer(drawCmds, drawCmdCount * sizeof(DrawCmd), drawCmdBuffer, 0);
	Gpu::BufferBarrier(drawCmdBuffer, 0, drawCmdCount * sizeof(DrawCmd), Gpu::BarrierStage::Copy_Write, Gpu::BarrierStage::VertexShader_StorageRead);

	passes[passes.len - 1].drawCmdEnd = drawCmdCount;
	for (U64 i = 0; i < passes.len; i++) {
		const Pass* const pass = &passes[i];
		U32 const passDrawCmdCount = pass->drawCmdEnd - pass->drawCmdStart;
		if (passDrawCmdCount == 0) {
			continue;
		}

		CanvasPool::Entry* const entry = canvasObjs.GetEntry(pass->canvas);
		CanvasObj* canvasObj = &entry->obj;
		const Gpu::Pass gpuPass = {
			.pipeline         = pipeline,
			.colorAttachments = { canvasObj->colorImage },
			.depthAttachment  = canvasObj->depthImage,
			.viewport         = { .x = 0.0f, .y = 0.0f, .w = canvasObj->size.x,      .h = canvasObj->size.y },
			.scissor          = { .x = 0,    .y = 0,    .w = (U32)canvasObj->size.x, .h = (U32)canvasObj->size.y },
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
			.sceneBufferAddr   = sceneBufferAddr,
			.drawCmdBufferAddr = drawCmdBufferAddr,
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

void SetCanvas(Canvas canvas) {
	passes[passes.len - 1].drawCmdEnd = drawCmdCount;
	passes.Add(Pass {
		.canvas       = canvas.handle ? canvas : swapchainCanvas,
		.drawCmdStart = drawCmdCount,
		.drawCmdEnd   = U32Max,
	});
}

//--------------------------------------------------------------------------------------------------

static DrawCmd* AllocDrawCmds(U32 n) {
	JC_ASSERT(drawCmdCount + n < MaxDrawCmds);
	DrawCmd* ptr = drawCmds + drawCmdCount;
	drawCmdCount += n;
	return ptr;
}

//--------------------------------------------------------------------------------------------------

void DrawSprite(Sprite sprite, Vec2 pos) {
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
		.rotation     = 0.0f,
		.textureIdx   = spriteObj->imageIdx,
	};
}

void DrawSprite(Sprite sprite, Vec2 pos, Vec2 scale, F32 rotation, Vec4 color) {
	SpriteObj* const spriteObj = &spriteObjs[sprite.handle];
	*AllocDrawCmds(1) =  {
		.pos          = pos,
		.size         = Math::Mul(spriteObj->size, scale),
		.uv1          = spriteObj->uv1,
		.uv2          = spriteObj->uv2,
		.color        = color,
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
		.cornerRadius = cornerRadius,
		.rotation     = 0.0f,
		.textureIdx   = 0,
	};
}

//--------------------------------------------------------------------------------------------------

void DrawCanvas(Canvas canvas, Vec2 pos, Vec2 scale) {
	CanvasObj* const canvasObj = canvasObjs.Get(canvas);
	*AllocDrawCmds(1) =  {
		.pos          = pos,
		.size         = Math::Mul(canvasObj->size, scale),
		.uv1          = Vec2(0.f, 0.f),
		.uv2          = Vec2(1.f, 1.f),
		.color        = Vec4(1.f, 1.f, 1.f, 1.f),
		.borderColor  = Vec4(1.f, 1.f, 1.f, 1.f),
		.border       = 0.0f,
		.cornerRadius = 0.0f,
		.rotation     = 0.0f,
		.textureIdx   = canvasObj->colorImageIdx
	};
}