#include "JC/Draw.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/File.h"
#include "JC/HandlePool.h"
#include "JC/Hash.h"
#include "JC/Gpu.h"
#include "JC/Json.h"
#include "JC/Log.h"
#include "JC/Map.h"
#include "JC/Math.h"
#include "JC/StrDb.h"

#include "stb/stb_image.h"

namespace JC::Draw {

//--------------------------------------------------------------------------------------------------

DefErr(Draw, AtlasFmt);
DefErr(Draw, DuplicateSpriteName);
DefErr(Draw, ImageFmt);
DefErr(Draw, LoadImage);
DefErr(Draw, SpriteNotFound);
DefErr(Draw, BadFontChar);
DefErr(Draw, Ttf);

//--------------------------------------------------------------------------------------------------

static constexpr U32 MaxDrawCmds     = 128 * 1024;
static constexpr U32 MaxSpriteImages = 256;
static constexpr U32 MaxSprites      = 1 * MB;
static constexpr U32 MaxFonts        = 256;
static constexpr U32 MaxCanvases     = 64;
static constexpr U32 MaxPasses       = 64;
static constexpr U32 ErrorImageSize  = 64;

//--------------------------------------------------------------------------------------------------

struct SpriteFileEntry {
	U32 x = 0;
	U32 y = 0;
	U32 w = 0;
	U32 h = 0;
	Str name;
};
Json_Begin(SpriteFileEntry)
	Json_Member("x",    x)
	Json_Member("y",    y)
	Json_Member("w",    w)
	Json_Member("h",    h)
	Json_Member("name", name)
Json_End(SpriteFileEntry)

struct SpriteImage {
	Str        path;
	Gpu::Image image;
	U32        imageIdx = 0;
};

struct SpriteObj {
	U32  imageIdx = 0;
	Vec2 uv1;
	Vec2 uv2;
	Vec2 size;
	Str  name;
};

//--------------------------------------------------------------------------------------------------

struct FontFileGlyph {
	U32 x    = 0;
	U32 y    = 0;
	U32 w    = 0;
	U32 h    = 0;
	I32 xOff = 0;
	I32 yOff = 0;
	U32 xAdv = 0;
	U32 ch   = 0;
};
Json_Begin(FontFileGlyph)
	Json_Member("x",    x)
	Json_Member("y",    y)
	Json_Member("w",    w)
	Json_Member("h",    h)
	Json_Member("xOff", xOff)
	Json_Member("yOff", yOff)
	Json_Member("xAdv", xAdv)
	Json_Member("ch",   ch)
Json_End(FontFileGlyph)

struct FontFile {
	U32                 lineHeight = 0;
	U32                 base = 0;
	Span<FontFileGlyph> glyphs;
};
Json_Begin(FontFile)
	Json_Member("lineHeight", lineHeight)
	Json_Member("base",       base)
	Json_Member("glyphs",     glyphs)
Json_End(FontFile)


struct Glyph {
	U32  imageIdx;
	Vec2 uv1;
	Vec2 uv2;
	Vec2 size;
	Vec2 off;
	F32  xAdv = 0.f;
};

static constexpr U16 MaxGlyphs = 127; // Exclude 128=DEL
struct FontObj {
	Str        path;
	Gpu::Image image;
	U32        imageIdx;
	F32        lineHeight;
	F32        base;	// pixels from top of line to baseline
	Glyph      glyphs[MaxGlyphs];
};

//--------------------------------------------------------------------------------------------------

struct CanvasObj {
	Gpu::Image       colorImage;
	U32              colorImageIdx;
	Gpu::ImageLayout colorImageLayout;
	Gpu::Image       depthImage;
	Vec2             size;
};

using CanvasPool = HandlePool<CanvasObj, Canvas>;

//--------------------------------------------------------------------------------------------------

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
	U32  textureIdx = 0;
	Vec3 pos;
	Vec2 size;
	Vec2 uv1;
	Vec2 uv2;
	Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
	Vec4 outlineColor;
	F32  outlineWidth = 0.f;
	F32  rotation = 0.f;
};

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
static Gpu::Image    errorImage;
static U32           errorImageIdx;
static SpriteImage*  spriteImages;
static U32           spriteImagesLen;
static SpriteObj*    spriteObjs;
static U32           spriteObjsLen;
static Map<Str, U32> spriteObjsByName;
static FontObj*      fontObjs;
static U32           fontObjsLen;
static CanvasPool    canvasObjs;
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
static Canvas        swapchainCanvas;
static Pass*         passes;
static U32           passesLen;

//--------------------------------------------------------------------------------------------------

static Res<Gpu::Shader> LoadShader(Str path) {
	Span<U8> data;
	if (Res<> r = File::ReadAll(path).To(data); !r) { return r.err; }
	return Gpu::CreateShader(data.data, data.len);
}

//--------------------------------------------------------------------------------------------------

Res<> Init(InitDef const* initDef) {
	permMem      = initDef->permMem;
	tempMem      = initDef->tempMem;
	windowWidth  = initDef->windowWidth;
	windowHeight = initDef->windowHeight;

	spriteImages    = Mem::AllocT<SpriteImage>(permMem, MaxSpriteImages);
	spriteImagesLen = 0;
	spriteObjs      = Mem::AllocT<SpriteObj>(permMem, MaxSprites);
	spriteObjsLen   = 1;	// reserve 0 for invalid
	fontObjs        = Mem::AllocT<FontObj>(permMem, MaxFonts);
	fontObjsLen     = 1;	// reserve 0 for invalid
	passes          = Mem::AllocT<Pass>(permMem, MaxPasses);
	passesLen       = 0;
	spriteObjsByName.Init(permMem, MaxSprites);
	canvasObjs.Init(permMem, MaxCanvases);

	TryTo(Gpu::CreateImage(ErrorImageSize, ErrorImageSize, Gpu::ImageFormat::B8G8R8A8_UNorm, Gpu::ImageUsage::Sampled | Gpu::ImageUsage::Copy), errorImage);
	errorImageIdx = Gpu::GetImageBindIdx(errorImage);
	Gpu_Name(errorImage);
	U8* errorImageData = Mem::AllocT<U8>(tempMem, ErrorImageSize * ErrorImageSize * 4);
	for (U32 i = 0; i < ErrorImageSize * ErrorImageSize * 4; i++) {
		errorImageData[i] = 0xff;
	}
	Try(Gpu::ImmediateCopyToImage(errorImageData, errorImage, Gpu::BarrierStage::VertexShader_SamplerRead, Gpu::ImageLayout::ShaderRead));
	Try(Gpu::ImmediateWait());

	TryTo(Gpu::CreateImage(windowWidth, windowHeight, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::Depth), depthImage);
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
	for (U32 i = 0; i < spriteImagesLen; i++) {
		Gpu::DestroyImage(spriteImages[i].image);
	}
	for (U32 i = 0; i < fontObjsLen; i++) {
		Gpu::DestroyImage(fontObjs[i].image);
	}
	for (U32 i = 0; i < Gpu::MaxFrames; i++) {
		Gpu::DestroyBuffer(sceneBuffers[i]);
		Gpu::DestroyBuffer(drawCmdBuffers[i]);
	}
	Gpu::DestroyPipeline(pipeline);
	Gpu::DestroyShader(vertexShader);
	Gpu::DestroyShader(fragmentShader);
	Gpu::DestroyImage(depthImage);
	Gpu::DestroyImage(errorImage);
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
	Span<U8> data; TryTo(File::ReadAll(path), data);

	int width = 0;
	int height = 0;
	U8* imageData = (U8*)stbi_load_from_memory(data.data, (int)data.len, &width, &height, 0, 4);
	if (!imageData) {
		return Err_LoadImage("path", path, "desc", stbi_failure_reason());
	}
	Defer { stbi_image_free(imageData); };

	Gpu::Image image;
	Try(Gpu::CreateImage(width, height, Gpu::ImageFormat::R8G8B8A8_UNorm, Gpu::ImageUsage::Sampled | Gpu::ImageUsage::Copy).To(image));
	Gpu_Namef(image, "%s", path);
	Try(Gpu::ImmediateCopyToImage(imageData, image, Gpu::BarrierStage::VertexShader_SamplerRead, Gpu::ImageLayout::ShaderRead));

	return image;
}

//--------------------------------------------------------------------------------------------------

Res<> LoadSprites(Str imagePath, Str spritesPath) {
	Assert(spriteImagesLen < MaxSpriteImages);

	Gpu::Image image; TryTo(LoadImage(imagePath), image);
	U32 const imageIdx    = Gpu::GetImageBindIdx(image);
	F32 const imageWidth  = (F32)Gpu::GetImageWidth(image);
	F32 const imageHeight = (F32)Gpu::GetImageHeight(image);
	spriteImages[spriteImagesLen++] = {
		.path     = StrDb::Intern(imagePath),
		.image    = image,
		.imageIdx = imageIdx,
	};

	Span<char> json; TryTo(File::ReadAllZ(spritesPath), json);
	Span<SpriteFileEntry> entries; Try(Json::ToArray(tempMem, tempMem, json.data, (U32)json.len, &entries));
	Assert(spriteObjsLen + entries.len <= MaxSprites);
	for (U64 i = 0; i < entries.len; i++) {
		SpriteFileEntry const* const entry = &entries[i];
		if (spriteObjsByName.FindOrNull(entry->name)) {
			return Err_DuplicateSpriteName("path", spritesPath, "name", entry->name);
		}
		F32 const x = (F32)entry->x;
		F32 const y = (F32)entry->y;
		F32 const w = (F32)entry->w;
		F32 const h = (F32)entry->h;

		spriteObjsByName.Put(entry->name, spriteObjsLen);
		spriteObjs[spriteObjsLen++] = {
			.imageIdx = imageIdx,
			.uv1      = { x / imageWidth, y / imageHeight },
			.uv2      = { (x + w) / imageWidth, (y + h) / imageHeight },
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
		return Err_SpriteNotFound("name", name);
	}
	return Sprite { .handle = (U64)(*idx) };
}

//--------------------------------------------------------------------------------------------------

Vec2 GetSpriteSize(Sprite sprite) {
	Assert(sprite.handle > 0 && sprite.handle < spriteObjsLen);
	return spriteObjs[sprite.handle].size;
}

//--------------------------------------------------------------------------------------------------

Res<Font> LoadFont(Str fontPath, Str imagePath) {
	Assert(fontObjsLen < MaxFonts);

	Gpu::Image image; TryTo(LoadImage(imagePath), image);
	U32 const imageIdx    = Gpu::GetImageBindIdx(image);
	F32 const imageWidth  = (F32)Gpu::GetImageWidth(image);
	F32 const imageHeight = (F32)Gpu::GetImageHeight(image);

	Span<char> json; TryTo(File::ReadAllZ(fontPath), json);
	FontFile fontFile; Try(Json::ToObj(tempMem, tempMem, json.data, (U32)json.len, &fontFile));

	FontObj* const fontObj = &fontObjs[fontObjsLen++];
	fontObj->path       = StrDb::Intern(fontPath);
	fontObj->image      = image;
	fontObj->imageIdx   = imageIdx;
	fontObj->lineHeight = (F32)fontFile.lineHeight;
	fontObj->base       = (F32)fontFile.base;

	for (U64 i = 0; i < fontFile.glyphs.len; i++) {
		FontFileGlyph const* const fontFileGlyph = &fontFile.glyphs[i];
		// TODO: more validation, here and in sprite
		if (fontFileGlyph->ch >= MaxGlyphs) {
			//Errorf("Ignoring out-of-range font character '%c' (%u) in %s", (char)fontFileGlyph->ch, fontFileGlyph->ch, fontPath);
		}
		F32 const x = (F32)fontFileGlyph->x;
		F32 const y = (F32)fontFileGlyph->y;
		F32 const w = (F32)fontFileGlyph->w;
		F32 const h = (F32)fontFileGlyph->h;
		fontObj->glyphs[fontFileGlyph->ch] = {
			.imageIdx = imageIdx,
			.uv1      = { x / imageWidth, y / imageHeight },
			.uv2      = { (x + w) / imageWidth, (y + h) / imageHeight },
			.size     = { w, h },
			.off      = { (F32)fontFileGlyph->xOff, (F32)fontFileGlyph->yOff },
			.xAdv     = (F32)fontFileGlyph->xAdv,
		};
	}
	for (U16 i = 0; i < 32; i++) {
		fontObj->glyphs[i] = {
			.imageIdx = errorImageIdx,
			.uv1      = { 0.f, 0.f },
			.uv2      = { 1.f, 1.f },
			.size     = { fontObj->lineHeight, fontObj->lineHeight },
			.off      = { 0.f, 0.f },
			.xAdv     = fontObj->lineHeight,
		};
	}
	for (U16 i = 32; i < MaxGlyphs; i++) {
		if (!fontObj->glyphs[i].imageIdx) {
			Logf("Using error image for missing ascii char '%c' in %s", (char)i, fontPath);
			fontObj->glyphs[i] = {
				.imageIdx = errorImageIdx,
				.uv1      = { 0.f, 0.f },
				.uv2      = { 1.f, 1.f },
				.size     = { fontObj->lineHeight, fontObj->lineHeight },
				.off      = { 0.f, 0.f },
				.xAdv     = fontObj->lineHeight,
			};
		}
	}


	return Font { .handle = (U64)(fontObj - fontObjs) };
}

//--------------------------------------------------------------------------------------------------

Str GetFontPath(Font font) {
	Assert(font.handle > 0 && font.handle < fontObjsLen);
	return fontObjs[font.handle].path;
}

F32 GetFontLineHeight(Font font) {
	Assert(font.handle > 0 && font.handle < fontObjsLen);
	return fontObjs[font.handle].lineHeight;
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

void BeginFrame(const Gpu::FrameData* gpuFrameData) {
	frameIdx = gpuFrameData->frameIdx;
	CanvasObj* const canvasObj = canvasObjs.Get(swapchainCanvas);
	canvasObj->colorImage = gpuFrameData->swapchainImage;

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

void  DrawSprite(SpriteDrawDef drawDef) {
	Assert(drawDef.sprite.handle > 0 && drawDef.sprite.handle < spriteObjsLen);
	SpriteObj const* const spriteObj = &spriteObjs[drawDef.sprite.handle];

	DrawCmd* const drawCmd = AllocDrawCmds(1);
	drawCmd->textureIdx   = spriteObj->imageIdx;
	drawCmd->pos          = Vec3(drawDef.pos.x, drawDef.pos.y, drawDef.z);
	drawCmd->size         = Math::Mul(spriteObj->size, drawDef.scale);
	drawCmd->uv1          = spriteObj->uv1;
	drawCmd->uv2          = spriteObj->uv2;
	drawCmd->color        = drawDef.color;
	drawCmd->outlineColor = drawDef.outlineColor;
	drawCmd->outlineWidth = drawDef.outlineWidth;
	drawCmd->rotation     = drawDef.rotation;
}

//--------------------------------------------------------------------------------------------------

static F32 StrWidth(FontObj const* fontObj, Str str, Vec2 scale) {
	if (str.len == 0) {
		return 0;
	}

	F32 width = 0;
	for (U32 i = 0; i < str.len - 1; i++) {
		Glyph const* const glyph = &fontObj->glyphs[str[i]];
		width += glyph->xAdv * scale.x;
	}
	Glyph const* const lastGlyph = &fontObj->glyphs[str[str.len - 1]];
	// This way gives the correct length but causes aliasing issues with pixel-perfect fonts using linear filtering
	//width += (lastGlyph->off.x + lastGlyph->size.x) * scale.x;
	// this line makes the center slightly offset but doesn't have aliasing problems
	width += lastGlyph->xAdv * scale.x;
	return width;
}

//--------------------------------------------------------------------------------------------------

void DrawFont(FontDrawDef drawDef) {
	Assert(drawDef.font.handle > 0 && drawDef.font.handle < fontObjsLen);
	FontObj const* const fontObj = &fontObjs[drawDef.font.handle];

	F32 x = drawDef.pos.x;
	F32 y = drawDef.pos.y;

	switch (drawDef.origin) {
		case Origin::BottomLeft:
		case Origin::BottomCenter:
		case Origin::BottomRight:
			y -= (fontObj->lineHeight - fontObj->base) * drawDef.scale.y;
			break;
		case Origin::TopLeft:
		case Origin::TopCenter:
		case Origin::TopRight:
			y += fontObj->base * drawDef.scale.y;
			break;
	}
	switch (drawDef.origin) {
		case Origin::BottomCenter:
		case Origin::Center:
		case Origin::TopCenter:
			x -= StrWidth(fontObj, drawDef.str, drawDef.scale) * 0.5f;
			break;
		case Origin::BottomRight:
		case Origin::Right:
		case Origin::TopRight:
			x -= StrWidth(fontObj, drawDef.str, drawDef.scale);
			break;
	}

	F32 const lineTop = y - (fontObj->base * drawDef.scale.y);
	for (U32 i = 0; i < drawDef.str.len; i++) {
		Glyph const* glyph = &fontObj->glyphs[drawDef.str[i]];
		Vec2 const drawTopLeft = Vec2(
			x       + (glyph->off.x * drawDef.scale.x),
			lineTop + (glyph->off.y * drawDef.scale.y)
		);
		Vec2 const drawCenter = Vec2(
			drawTopLeft.x + (glyph->size.x * drawDef.scale.x * 0.5f),
			drawTopLeft.y + (glyph->size.y * drawDef.scale.y * 0.5f)
		);
		DrawCmd* const drawCmd = AllocDrawCmds(1);
		drawCmd->textureIdx   = glyph->imageIdx;
		drawCmd->pos          = Vec3(drawCenter.x, drawCenter.y, drawDef.z);
		drawCmd->size         = Math::Mul(glyph->size, drawDef.scale);
		drawCmd->uv1          = glyph->uv1;
		drawCmd->uv2          = glyph->uv2;
		drawCmd->color        = drawDef.color;
		drawCmd->outlineColor = Vec4(0.f, 0.f, 0.f, 0.f);
		drawCmd->outlineWidth = 0.f;
		drawCmd->rotation     = 0.f;
		x += glyph->xAdv * drawDef.scale.x;
	}
}

//--------------------------------------------------------------------------------------------------

void DrawCanvas(CanvasDrawDef drawDef) {
	CanvasObj const* const canvasObj = canvasObjs.Get(drawDef.canvas);

	DrawCmd* const drawCmd = AllocDrawCmds(1);
	drawCmd->textureIdx   = canvasObj->colorImageIdx;
	drawCmd->pos          = Vec3(drawDef.pos.x, drawDef.pos.y, drawDef.z);
	drawCmd->size         = Math::Mul(canvasObj->size, drawDef.scale);
	drawCmd->uv1          = Vec2(0.f, 0.f);
	drawCmd->uv2          = Vec2(1.f, 1.f);
	drawCmd->color        = drawDef.color;
	drawCmd->outlineColor = Vec4(0.f, 0.f, 0.f, 0.f);
	drawCmd->outlineWidth = 0.f;
	drawCmd->rotation     = drawDef.rotation;
}

//--------------------------------------------------------------------------------------------------

void DrawRect(RectDrawDef drawDef) {
	DrawCmd* const drawCmd = AllocDrawCmds(1);
	drawCmd->textureIdx   = 0;
	drawCmd->pos          = Vec3(drawDef.pos.x, drawDef.pos.y, drawDef.z);
	drawCmd->size         = drawDef.size;
	drawCmd->uv1          = Vec2(0.f, 0.f);
	drawCmd->uv2          = Vec2(0.f, 0.f);
	drawCmd->color        = drawDef.color;
	drawCmd->outlineColor = Vec4(0.f, 0.f, 0.f, 0.f);
	drawCmd->outlineWidth = 0.f;
	drawCmd->rotation     = 0.f;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Draw