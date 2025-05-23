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
	U64  sceneBufferAddr = 0;
};

struct DrawCmd {
	Vec2 pos;
	Vec2 size;
	Vec2 uv1;
	Vec2 uv2;
	Vec4 color;
	Vec4 borderColor;
	F32  border;
	F32  borderRadius;
	U32  textureIdx;
	U32  pad;
};

//--------------------------------------------------------------------------------------------------


static Mem::Allocator*     allocator;
static Mem::TempAllocator* tempAllocator;
static Log::Logger*        logger;
static U32                 windowWidth;
static U32                 windowHeight;
static Gpu::Image          depthImage;
static F32                 spriteScale;
static F32                 uiScale;
static U32                 frameIdx;

static Gpu::Pipeline       spritePipeline;
static Array<Gpu::Image>   spriteImages;
static Array<SpriteObj>    spriteObjs;
static Map<Str, U32>       spriteObjsByName;

static Gpu::Shader         vertexShader;
static Gpu::Shader         fragmentShader;
static Gpu::Buffer         sceneBuffers[Gpu::MaxFrames];
static U64                 sceneBufferAddrs[Gpu::MaxFrames];
static Gpu::Pipeline       pipeline;
static Gpu::Buffer         drawCmdBuffers[Gpu::MaxFrames];
static U64                 drawCmdBufferAddrs[Gpu::MaxFrames];
static U32                 drawCmdBufferSize;
static Gpu::StagingMem     drawCmdStagingMem;
static U32                 drawCmdCount;

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
	if (Res<> r = Gpu::CreateGraphicsPipeline({ vertexShader, fragmentShader }, { Gpu::GetImageFormat(Gpu::GetSwapchainImage()) }, Gpu::ImageFormat::D32_Float).To(spritePipeline); !r) { return r; }
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		if (Res<> r = Gpu::CreateBuffer(sizeof(Scene), Gpu::BufferUsage::Storage).To(sceneBuffers[i]); !r) { return r; }
		sceneBufferAddrs[i] = Gpu::GetBufferAddr(sceneBuffers[i]);
	}

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	for (U64 i = 0; i < spriteImages.len; i++) {
		Gpu::DestroyImage(spriteImages[i]);
	}
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		Gpu::DestroyBuffer(drawCmdBuffers[i]);
	}
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		Gpu::DestroyBuffer(sceneBuffers[i]);
	}
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

static void AllocStagingMem() {
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		if (Res<> r = Gpu::CreateBuffer(MaxSprites * sizeof(SpriteDrawCmd), Gpu::BufferUsage::Storage).To(drawCmdBuffers[i]); !r) { return r; }
		drawCmdBufferAddrs[i] = Gpu::GetBufferAddr(drawCmdBuffers[i]);
	}

}
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

	Gpu::Image image;
	if (Res<> r = Gpu::CreateImage(width, height, Gpu::ImageFormat::R8G8B8A8_UNorm, Gpu::ImageUsage::Sampled).To(image); !r) { return r.err; }

	Gpu::ImageBarrier(image, Gpu::Stage::None, Gpu::Stage::TransferDst);
	const Gpu::StagingMem stagingMem = Gpu::AllocStagingMem(width * height * 4);
	if (channels == 4) {
		memcpy(stagingMem.ptr, imageData, width * height * channels);
	} else {
		U8* in = imageData;
		U8* out = (U8*)stagingMem.ptr;
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				*out++ = *in++;
				*out++ = *in++;
				*out++ = *in++;
				*out++ = 0xff;
			}
		}
	}
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

void SetSpriteScale(F32 spriteScaleIn) { spriteScale = spriteScaleIn; }
void SetUiScale(F32 uiScaleIn) { uiScale = uiScaleIn; }

//--------------------------------------------------------------------------------------------------

void BeginFrame() {
	drawCmdCount = 0;
}

//--------------------------------------------------------------------------------------------------

void EndFrame() {
	const Gpu::Image swapchainImage = Gpu::GetSwapchainImage();

	const F32 fw = (F32)windowWidth;
	const F32 fh = (F32)windowHeight;

	const Mat4 spriteProjView = Math::Ortho(
		-fw / spriteScale / 2.0f,
		 fw / spriteScale / 2.0f,
		-fh / spriteScale / 2.0f,
		 fh / spriteScale / 2.0f,
		-100.0f,
		 100.0f
	);

	const Gpu::StagingMem spriteSceneBufferStagingMem = Gpu::AllocStagingMem(sizeof(Scene));
	Scene* const spriteScene = (Scene*)spriteSceneBufferStagingMem.ptr;
	spriteScene->projView          = spriteProjView;
	spriteScene->drawCmdBufferAddr = spriteDrawCmdBufferAddrs[frameIdx],
	Gpu::BufferBarrier(spriteSceneBuffers[frameIdx], Gpu::Stage::VertexShaderRead, Gpu::Stage::TransferDst);
	Gpu::UpdateBuffer(spriteSceneBuffers[frameIdx], 0, spriteSceneBufferStagingMem);
	Gpu::BufferBarrier(spriteSceneBuffers[frameIdx], Gpu::Stage::TransferDst, Gpu::Stage::VertexShaderRead);

	Gpu::BufferBarrier(spriteDrawCmdBuffers[frameIdx], Gpu::Stage::VertexShaderRead, Gpu::Stage::TransferDst);
	Gpu::UpdateBuffer(spriteDrawCmdBuffers[frameIdx], 0, spriteDrawCmdStagingMem);
	Gpu::BufferBarrier(spriteDrawCmdBuffers[frameIdx], Gpu::Stage::TransferDst, Gpu::Stage::VertexShaderRead);

	const Gpu::Pass spritePass = {
		.pipeline         = spritePipeline,
		.colorAttachments = { swapchainImage },
		.depthAttachment  = depthImage,
		.viewport         = { .x = 0.0f, .y = 0.0f, .w = (F32)windowWidth, .h = (F32)windowHeight },
		.scissor          = { .x = 0,    .y = 0,    .w = windowWidth,      .h = windowHeight },
		.clear            = true,
	};
	Gpu::BeginPass(&spritePass);
	PushConstants spritePushConstants = { .sceneBufferAddr = spriteSceneBufferAddrs[frameIdx] };
	Gpu::PushConstants(spritePipeline, &spritePushConstants, sizeof(spritePushConstants));
	Gpu::Draw(6, spriteDrawCmdCount);
	Gpu::EndPass();

	const Mat4 uiProjView = Math::Ortho(
		-fw / uiScale / 2.0f,
		 fw / uiScale / 2.0f,
		-fh / uiScale / 2.0f,
		 fh / uiScale / 2.0f,
		-100.0f,
		 100.0f
	);

	const Gpu::StagingMem uiSceneBufferStagingMem = Gpu::AllocStagingMem(sizeof(Scene));
	Scene* const uiScene = (Scene*)uiSceneBufferStagingMem.ptr;
	uiScene->projView          = uiProjView;
	uiScene->drawCmdBufferAddr = uiDrawCmdBufferAddrs[frameIdx],
	Gpu::BufferBarrier(uiSceneBuffers[frameIdx], Gpu::Stage::VertexShaderRead, Gpu::Stage::TransferDst);
	Gpu::UpdateBuffer(uiSceneBuffers[frameIdx], 0, uiSceneBufferStagingMem);
	Gpu::BufferBarrier(uiSceneBuffers[frameIdx], Gpu::Stage::TransferDst, Gpu::Stage::VertexShaderRead);

	Gpu::BufferBarrier(uiDrawCmdBuffers[frameIdx], Gpu::Stage::VertexShaderRead, Gpu::Stage::TransferDst);
	Gpu::UpdateBuffer(uiDrawCmdBuffers[frameIdx], 0, uiDrawCmdStagingMem);
	Gpu::BufferBarrier(uiDrawCmdBuffers[frameIdx], Gpu::Stage::TransferDst, Gpu::Stage::VertexShaderRead);

	const Gpu::Pass uiPass = {
		.pipeline         = uiPipeline,
		.colorAttachments = { swapchainImage },
		.depthAttachment  = depthImage,
		.viewport         = { .x = 0.0f, .y = 0.0f, .w = (F32)windowWidth, .h = (F32)windowHeight },
		.scissor          = { .x = 0,    .y = 0,    .w = windowWidth,      .h = windowHeight },
		.clear            = false,
	};
	Gpu::BeginPass(&uiPass);
	PushConstants uiPushConstants = { .sceneBufferAddr = uiSceneBufferAddrs[frameIdx] };
	Gpu::PushConstants(uiPipeline, &uiPushConstants, sizeof(uiPushConstants));
	Gpu::Draw(6, uiDrawCmdCount);
	Gpu::EndPass();
}

//--------------------------------------------------------------------------------------------------

void DrawSprite(Sprite sprite, Vec2 pos) {
	// TODO: dynamically resize the buffer
	Assert(spriteDrawCmdCount <= MaxSprites);

	SpriteDrawCmd* ptr = ((SpriteDrawCmd*)spriteDrawCmdStagingMem.ptr) + spriteDrawCmdCount;
	SpriteObj* const spriteObj = &spriteObjs[sprite.handle];
	*ptr = {
		.pos        = pos,
		.uv1        = spriteObj->uv1,
		.uv2        = spriteObj->uv2,
		.size       = spriteObj->size,
		.color      = Vec4(1.0f, 1.0f, 1.0f, 1.0f),
		.rotation   = 0.0f,
		.textureIdx = spriteObj->imageIdx,
	};
	spriteDrawCmdCount++;
}

void DrawSprite(Sprite sprite, Vec2 pos, F32 scale, F32 rotation, Vec4 color) {
	// TODO: dynamically resize the buffer
	Assert(spriteDrawCmdCount <= MaxSprites);

	SpriteDrawCmd* ptr = ((SpriteDrawCmd*)spriteDrawCmdStagingMem.ptr) + spriteDrawCmdCount;
	SpriteObj* const spriteObj = &spriteObjs[sprite.handle];
	*ptr = {
		.pos        = pos,
		.uv1        = spriteObj->uv1,
		.uv2        = spriteObj->uv2,
		.size       = Math::Mul(spriteObj->size, scale),
		.color      = color,
		.rotation   = rotation,
		.textureIdx = spriteObj->imageIdx,
	};
	spriteDrawCmdCount++;
}

//--------------------------------------------------------------------------------------------------

void DrawRect(Vec2 pos, Vec2 size, Vec4 fillColor, Vec4 borderColor, F32 border, F32 cornerRadius) {
	// TODO: dynamically resize the buffer
	Assert(uiDrawCmdCount <= MaxSprites);

	UiDrawCmd* ptr = ((UiDrawCmd*)uiDrawCmdStagingMem.ptr) + uiDrawCmdCount;
	*ptr = {
		.pos          = pos,
		.size         = size,
		.fillColor    = fillColor,
		.borderColor  = borderColor,
		.border       = border,
		.cornerRadius = cornerRadius,
	};
	uiDrawCmdCount++;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Render