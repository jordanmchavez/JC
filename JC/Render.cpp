#include "JC/Render.h"

#include "JC/Array.h"
#include "JC/Gpu.h"
#include "JC/File.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Log.h"
#include "JC/Map.h"

#include "3rd/stb/stb_image.h"

namespace JC::Render {

//--------------------------------------------------------------------------------------------------

DefErr(Render, AtlasFmt);
DefErr(Render, DuplicateSpriteName);
DefErr(Render, ImageFmt);
DefErr(Render, LoadImage);
DefErr(Render, SpriteNotFound);

//--------------------------------------------------------------------------------------------------

struct SpriteDrawCmd {
	Vec3 xyz        = {};
	U32  textureIdx = 0;
	Vec2 uv1        = {};
	Vec2 uv2        = {};
};

struct Scene {
	Mat4  projView                = {};
	U64   spriteDrawCmdBufferAddr = 0;
};

struct PushConstants {
	U64  sceneBufferAddr = 0;
};

struct SpriteObj {
	U32  imageIdx;
	Vec2 uv1;
	Vec2 uv2;
	Str  name;
};

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxSprites = 1024 * 1024;

static Mem::Allocator*     allocator;
static Mem::TempAllocator* tempAllocator;
static Log::Logger*        logger;
static U32                 windowWidth;
static U32                 windowHeight;
static Gpu::Image          depthImage;
static Gpu::Buffer         sceneBuffers[Gpu::MaxFrames];
static U64                 sceneBufferAddrs[Gpu::MaxFrames];
static Gpu::Buffer         spriteDrawCmdBuffers[Gpu::MaxFrames];
static U64                 spriteDrawCmdBufferAddrs[Gpu::MaxFrames];
static Gpu::Shader         vertexShader;
static Gpu::Shader         fragmentShader;
static Gpu::Pipeline       pipeline;
static Mat4                projView;
static Array<Gpu::Image>   spriteImages;
static Array<SpriteObj>    spriteObjs;
static Map<Str, U32>       spriteObjsByName;
static U32                 frameIdx;
static Gpu::Image          swapchainImage;
static Gpu::StagingMem     spriteDrawCmdStagingMem;
static U32                 spriteDrawCmdCount;

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
	spriteObjsByName.Init(allocator);

	if (Res<> r = Gpu::CreateImage(windowWidth, windowHeight, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::DepthAttachment).To(depthImage); !r) { return r; }

	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		if (Res<> r = Gpu::CreateBuffer(sizeof(Scene), Gpu::BufferUsage::Storage).To(sceneBuffers[i]); !r) { return r; }
		sceneBufferAddrs[i] = Gpu::GetBufferAddr(sceneBuffers[i]);
	}
		
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		if (Res<> r = Gpu::CreateBuffer(MaxSprites * sizeof(SpriteDrawCmd), Gpu::BufferUsage::Storage).To(spriteDrawCmdBuffers[i]); !r) { return r; }
		spriteDrawCmdBufferAddrs[i] = Gpu::GetBufferAddr(spriteDrawCmdBuffers[i]);
	}

	if (Res<> r = LoadShader("Shaders/SpriteVert.spv").To(vertexShader); !r) { return r; }
	if (Res<> r = LoadShader("Shaders/SpriteFrag.spv").To(fragmentShader); !r) { return r; }
	
	if (Res<> r = Gpu::CreateGraphicsPipeline({ vertexShader, fragmentShader }, { Gpu::GetImageFormat(Gpu::GetSwapchainImage()) }, Gpu::ImageFormat::D32_Float).To(pipeline); !r) { return r; }

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	for (U64 i = 0; i < spriteImages.len; i++) {
		Gpu::DestroyImage(spriteImages[i]);
	}
	DestroyPipeline(pipeline);
	DestroyShader(vertexShader);
	DestroyShader(fragmentShader);
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		Gpu::DestroyBuffer(spriteDrawCmdBuffers[i]);
	}
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		Gpu::DestroyBuffer(sceneBuffers[i]);
	}
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
	Gpu::UpdateImage(image, stagingMem);
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
		const F32 w = (F32)iw / imageWidth;
		const F32 h = (F32)ih / imageHeight;

		if (spriteObjsByName.FindOrNull(name)) {
			return Err_DuplicateSpriteName("path", atlasPath, "name", name);
		}

		SpriteObj* const spriteObj = spriteObjs.Add(SpriteObj {
			.imageIdx = imageIdx,
			.uv1      = { x, y },
			.uv2      = { x + w, y + h },
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

void SetProjView(const Mat4* inProjView) {
	projView = *inProjView;
}

//--------------------------------------------------------------------------------------------------

Res<> BeginFrame() {
	Gpu::SwapchainStatus swapchainStatus = {};
	if (Res<> r = Gpu::BeginFrame().To(swapchainStatus); !r) {
		return r;
	}
	if (swapchainStatus == Gpu::SwapchainStatus::NeedsRecreate) {
		if (Res<> r = Gpu::RecreateSwapchain(windowWidth, windowHeight); !r) {
			return r;
		}
		Logf("Recreated swapchain after BeginFrame() with w={}, h={}", windowWidth, windowHeight);
		return Err_SkipFrame();
	}

	frameIdx = Gpu::GetFrameIdx();

	swapchainImage = Gpu::GetSwapchainImage();
	Gpu::ImageBarrier(swapchainImage, Gpu::Stage::PresentOld, Gpu::Stage::ColorAttachment);

	spriteDrawCmdStagingMem = Gpu::AllocStagingMem(MaxSprites * sizeof(SpriteDrawCmd));
	spriteDrawCmdCount = 0;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<> EndFrame() {
	const Gpu::StagingMem sceneBufferStagingMem = Gpu::AllocStagingMem(sizeof(Scene));
	Scene* const scene = (Scene*)sceneBufferStagingMem.ptr;
	scene->projView  = projView;
	scene->spriteDrawCmdBufferAddr = spriteDrawCmdBufferAddrs[frameIdx],
	Gpu::BufferBarrier(sceneBuffers[frameIdx], Gpu::Stage::VertexShaderRead, Gpu::Stage::TransferDst);
	Gpu::UpdateBuffer(sceneBuffers[frameIdx], 0, sceneBufferStagingMem);
	Gpu::BufferBarrier(sceneBuffers[frameIdx], Gpu::Stage::TransferDst, Gpu::Stage::VertexShaderRead);

	Gpu::BufferBarrier(spriteDrawCmdBuffers[frameIdx], Gpu::Stage::VertexShaderRead, Gpu::Stage::TransferDst);
	Gpu::UpdateBuffer(spriteDrawCmdBuffers[frameIdx], 0, spriteDrawCmdStagingMem);
	Gpu::BufferBarrier(spriteDrawCmdBuffers[frameIdx], Gpu::Stage::TransferDst, Gpu::Stage::VertexShaderRead);

	const Gpu::Pass pass = {
		.pipeline         = pipeline,
		.colorAttachments = { swapchainImage },
		.depthAttachment  = depthImage,
		.viewport         = { .x = 0.0f, .y = 0.0f, .w = (F32)windowWidth, .h = (F32)windowHeight },
		.scissor          = { .x = 0,    .y = 0,    .w = windowWidth,      .h = windowHeight },
	};
	Gpu::BeginPass(&pass);
	PushConstants pushConstants = { .sceneBufferAddr = sceneBufferAddrs[frameIdx] };
	Gpu::PushConstants(pipeline, &pushConstants, sizeof(pushConstants));
	Gpu::Draw(6, spriteDrawCmdCount);
	Gpu::EndPass();

	Gpu::ImageBarrier(swapchainImage, Gpu::Stage::ColorAttachment, Gpu::Stage::Present);

	Gpu::SwapchainStatus swapchainStatus = {};
	if (Res<> r = Gpu::EndFrame().To(swapchainStatus); !r) {
		return r;
	}
	if (swapchainStatus == Gpu::SwapchainStatus::NeedsRecreate) {
		if (Res<> r = Gpu::RecreateSwapchain(windowWidth, windowHeight); !r) {
			return r;
		}
		Logf("Recreated swapchain after EndFrame() with w={}, h={}", windowWidth, windowHeight);
		return Err_SkipFrame();
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

void DrawSprites(Span<DrawSprite> drawSprites) {
	// TODO: dynamically resize the buffer
	Assert(spriteDrawCmdCount + drawSprites.len <= MaxSprites);

	SpriteDrawCmd* ptr = ((SpriteDrawCmd*)spriteDrawCmdStagingMem.ptr) + spriteDrawCmdCount;
	spriteDrawCmdCount += (U32)drawSprites.len;
	for (U64 i = 0; i < drawSprites.len; i++) {
		SpriteObj* const spriteObj = &spriteObjs[drawSprites[i].sprite.handle];
		*ptr++ = {
			.xyz        = drawSprites[i].xyz,
			.textureIdx = spriteObj->imageIdx,
			.uv1        = spriteObj->uv1,
			.uv2        = spriteObj->uv2,
		};
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Render