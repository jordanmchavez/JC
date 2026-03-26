#pragma warning (disable: 4505)

#include "JC/Draw.h"

#include "JC/Array.h"
#include "JC/File.h"
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

DefErr(Draw, Max);
DefErr(Draw, AtlasFmt);
DefErr(Draw, DuplicateSprite);
DefErr(Draw, ImageFmt);
DefErr(Draw, LoadImage);
DefErr(Draw, SpriteNotFound);
DefErr(Draw, FontNotFound);
DefErr(Draw, BadFontChar);
DefErr(Draw, Ttf);

//--------------------------------------------------------------------------------------------------

static constexpr U32 Cfg_MaxAtlases  = 64;
static constexpr U32 Cfg_MaxSprites  = 64 * 1024;
static constexpr U32 Cfg_MaxFonts    = 64;

static constexpr U32 MaxDrawCmds    = 128 * 1024;
static constexpr U32 MaxCanvases    = 64;
static constexpr U32 MaxPasses      = 64;
static constexpr U32 ErrorImageSize = 64;

//--------------------------------------------------------------------------------------------------

struct Atlas {
	Str        path;
	Gpu::Image image;
	U32        imageIdx = 0;
};

struct SpriteObj {
	U32  imageIdx = 0;
	Vec2 uv1;
	Vec2 uv2;
	Vec2 size;
	Vec2 texelSize;
	Str  name;
};

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
	Str        name;
	Gpu::Image image;
	U32        imageIdx;
	F32        lineHeight;
	F32        base;	// pixels from top of line to baseline
	Vec2       texelSize;
	Glyph      glyphs[MaxGlyphs];
};

struct CanvasObj {
	Gpu::Image       colorImage;
	U32              colorImageIdx;
	Gpu::ImageLayout colorImageLayout;
	Gpu::Image       depthImage;
	Vec2             size;
};

struct Scene {
	Mat4 projViews[MaxCanvases];
};

struct PushConstants {
	U64 sceneBufferAddr = 0;
	U64 drawCmdBufferAddr = 0;
	U32 sceneBufferIdx = 0;
	U32 drawCmdStart = 0;
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
};

struct Pass {
	Canvas canvas;
	U32    drawCmdStart;
	U32    drawCmdEnd;
};

//--------------------------------------------------------------------------------------------------

struct SpriteDef {
	U32 x = 0;
	U32 y = 0;
	U32 w = 0;
	U32 h = 0;
	Str name;
};

Json_Begin(SpriteDef)
	Json_Member("x",    x)
	Json_Member("y",    y)
	Json_Member("w",    w)
	Json_Member("h",    h)
	Json_Member("name", name)
Json_End(SpriteDef)

struct AtlasDef {
	Str             imagePath;
	Span<SpriteDef> sprites;
};
Json_Begin(AtlasDef)
	Json_Member("imagePath", imagePath)
	Json_Member("sprites",   sprites)
Json_End(AtlasDef)

//--------------------------------------------------------------------------------------------------

struct GlyphDef {
	U32 x;
	U32 y;
	U32 w;
	U32 h;
	I32 xOff;
	I32 yOff;
	U32 xAdv;
	U32 ch;
};
Json_Begin(GlyphDef)
	Json_Member("x",    x)
	Json_Member("y",    y)
	Json_Member("w",    w)
	Json_Member("h",    h)
	Json_Member("xOff", xOff)
	Json_Member("yOff", yOff)
	Json_Member("xAdv", xAdv)
	Json_Member("ch",   ch)
Json_End(GlyphDef)

struct FontDef {
	Str            name;
	Str            imagePath;
	U32            lineHeight;
	U32            base;
	Span<GlyphDef> glyphs;
};
Json_Begin(FontDef)
	Json_Member("name",       name)
	Json_Member("imagePath",  imagePath)
	Json_Member("lineHeight", lineHeight)
	Json_Member("base",       base)
	Json_Member("glyphs",     glyphs)
Json_End(FontDef)

//--------------------------------------------------------------------------------------------------

static Mem                  tempMem;
static U32                  windowWidth;
static U32                  windowHeight;
static Gpu::Image           errorImage;
static U32                  errorImageIdx;
static MArray<Atlas>        atlases;
static MArray<SpriteObj>    spriteObjs;
static Map<Str, SpriteObj*> spriteObjsByName;
static MArray<FontObj>      fontObjs;
static MArray<CanvasObj>    canvasObjs;
static Gpu::Image           depthImage;
static Gpu::Shader          vertexShader;
static Gpu::Shader          fragmentShader;
static Gpu::Pipeline        pipeline;
static Gpu::Buffer          sceneBuffers[Gpu::MaxFrames];
static void*                sceneBufferPtrs[Gpu::MaxFrames];
static U64                  sceneBufferAddrs[Gpu::MaxFrames];
static Scene*               scene;
static Gpu::Buffer          drawCmdBuffers[Gpu::MaxFrames];
static void*                drawCmdBufferPtrs[Gpu::MaxFrames];
static U64                  drawCmdBufferAddrs[Gpu::MaxFrames];
static DrawCmd*             drawCmds;
static U32                  drawCmdCount;
static MArray<Pass>         passes;
static Camera               camera;
static U64                  frameIdx;

//--------------------------------------------------------------------------------------------------

static Res<Gpu::Shader> LoadShader(Str path) {
	Span<U8> data;
	if (Res<> r = File::ReadAllBytes(tempMem, path).To(data); !r) { return r.err; }
	return Gpu::CreateShader(data.data, data.len);
}

//--------------------------------------------------------------------------------------------------

Res<> Init(InitDesc const* initDesc) {
	tempMem      = initDesc->tempMem;
	windowWidth  = initDesc->windowWidth;
	windowHeight = initDesc->windowHeight;
	atlases.Init(initDesc->permMem, Cfg_MaxAtlases);
	spriteObjs.Init(initDesc->permMem, Cfg_MaxSprites);
	spriteObjs.Add();// Reserve 0 for invalid
	spriteObjsByName.Init(initDesc->permMem, Cfg_MaxSprites);
	fontObjs.Init(initDesc->permMem, Cfg_MaxFonts);
	fontObjs.Add();// Reserve 0 for invalid
	canvasObjs.Init(initDesc->permMem, MaxCanvases);
	canvasObjs.Add();// Reserve 0 for invalid/swapchain canvas
	scene = Mem::AllocT<Scene>(initDesc->permMem, 1);
	passes.Init(initDesc->permMem, MaxPasses);

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
	canvasObjs[0] = {
		.colorImage       = Gpu::Image(),
		.colorImageIdx    = 0,
		.colorImageLayout = Gpu::ImageLayout::Color,
		.depthImage       = depthImage,
		.size             = windowSize,
	};
	scene->projViews[0] = Math::Ortho(
		0.0f, windowSize.x,
		windowSize.y, 0.0f,
		-100.0f, 100.0f
	);

	camera.pos   = Vec2(0.f, 0.f);
	camera.scale = 1.f;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

void Shutdown() {
	for (U32 i = 0; i < atlases.len; i++) {
		Gpu::DestroyImage(atlases[i].image);
	}
	for (U32 i = 1; i < fontObjs.len; i++) {	// 0 reserved for invalid
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
	scene->projViews[0] = Math::Ortho(
		0.0f, windowSize.x,
		windowSize.y, 0.0f,
		-100.0f, 100.0f
	);

	canvasObjs[0].depthImage = depthImage;
	canvasObjs[0].size       = windowSize;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<Gpu::Image> LoadImage(Str path) {
	Span<U8> data; TryTo(File::ReadAllBytes(tempMem, path), data);

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

Res<> LoadSprites(Str path) {
	for (U32 i = 0; i < atlases.len; i++) {
		if (File::PathsEq(path, atlases[i].path)) {
			return Ok();
		}
	}

	if (!atlases.HasCapacity()) { return Err_Max("type", "atlases", "max", Cfg_MaxAtlases); }

	Str json; TryTo(File::ReadAllStr(tempMem, path), json);
	AtlasDef atlasDef; Try(Json::JsonToObject(tempMem, json, &atlasDef));

	Gpu::Image image; TryTo(LoadImage(atlasDef.imagePath), image);
	U32 const imageIdx    = Gpu::GetImageBindIdx(image);
	F32 const imageWidth  = (F32)Gpu::GetImageWidth(image);
	F32 const imageHeight = (F32)Gpu::GetImageHeight(image);
	atlases.Add({
		.path     = StrDb::Intern(path),
		.image    = image,
		.imageIdx = imageIdx,
	});

	if (!spriteObjs.HasCapacity(atlasDef.sprites.len)) { return Err_Max("type", "sprites", "max", Cfg_MaxSprites); }

	for (U64 i = 0; i < atlasDef.sprites.len; i++) {
		SpriteDef* const spriteDef = &atlasDef.sprites[i];
		if (spriteObjsByName.FindOrZero(spriteDef->name)) {
			return Err_DuplicateSprite("path", path, "name", spriteDef->name);
		}
		F32 const x = (F32)spriteDef->x;
		F32 const y = (F32)spriteDef->y;
		F32 const w = (F32)spriteDef->w;
		F32 const h = (F32)spriteDef->h;

		spriteObjsByName.Put(
			spriteDef->name,
			spriteObjs.Add({
				.imageIdx  = imageIdx,
				.uv1       = { x / imageWidth, y / imageHeight },
				.uv2       = { (x + w) / imageWidth, (y + h) / imageHeight },
				.size      = { w, h },
				.texelSize = { 1.f / imageWidth, 1.f / imageHeight },
				.name      = spriteDef->name,	// Json interns this for us
			})
		);
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Sprite> GetSprite(Str name) {
	SpriteObj* spriteObj = spriteObjsByName.FindOrZero(name);
	if (!spriteObj) {
		return Err_SpriteNotFound("name", name);
	}
	return Sprite { .handle = (U64)(spriteObj - spriteObjs.data) };
}

//--------------------------------------------------------------------------------------------------

Vec2 GetSpriteSize(Sprite sprite) {
	Assert(sprite.handle > 0 && sprite.handle < spriteObjs.len);
	return spriteObjs[sprite.handle].size;
}

//--------------------------------------------------------------------------------------------------

Res<> LoadFont(Str path) {
	if (!fontObjs.HasCapacity()) { return Err_Max("type", "fonts", "max", Cfg_MaxFonts); }

	Str json; TryTo(File::ReadAllStr(tempMem, path), json);
	FontDef fontDef; Try(Json::JsonToObject(tempMem, json, &fontDef));


	Gpu::Image image; TryTo(LoadImage(fontDef.imagePath), image);
	U32 const imageIdx    = Gpu::GetImageBindIdx(image);
	F32 const imageWidth  = (F32)Gpu::GetImageWidth(image);
	F32 const imageHeight = (F32)Gpu::GetImageHeight(image);

	FontObj* const fontObj = fontObjs.Add();
	fontObj->name       = fontDef.name;
	fontObj->image      = image;
	fontObj->imageIdx   = imageIdx;
	fontObj->lineHeight = (F32)fontDef.lineHeight;
	fontObj->base       = (F32)fontDef.base;
	fontObj->texelSize  = { 1.f / imageWidth, 1.f / imageHeight };

	for (U64 i = 0; i < fontDef.glyphs.len; i++) {
		GlyphDef const* const glyphDef = &fontDef.glyphs[i];
		// TODO: more validation, here and in sprite
		if (glyphDef->ch >= MaxGlyphs) {
			//Errorf("Ignoring out-of-range font character '%c' (%u) in %s", (char)glyphJson->ch, glyphJson->ch, fontPath);
		}
		F32 const x = (F32)glyphDef->x;
		F32 const y = (F32)glyphDef->y;
		F32 const w = (F32)glyphDef->w;
		F32 const h = (F32)glyphDef->h;
		fontObj->glyphs[glyphDef->ch] = {
			.imageIdx = imageIdx,
			.uv1      = { x / imageWidth, y / imageHeight },
			.uv2      = { (x + w) / imageWidth, (y + h) / imageHeight },
			.size     = { w, h },
			.off      = { (F32)glyphDef->xOff, (F32)glyphDef->yOff },
			.xAdv     = (F32)glyphDef->xAdv,
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
			//Logf("Using error image for missing ascii char '%c' in %s", (char)i, fontPath);
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


	return Ok();
}

//--------------------------------------------------------------------------------------------------

Res<Font> GetFont(Str name) {
	for (U64 i = 1; i < fontObjs.len; i++) {
		if (name == fontObjs[i].name) {
			return Font { .handle = i };
		}
	}
	return Err_FontNotFound("name", name);
}

F32 GetFontLineHeight(Font font) {
	Assert(font.handle > 0 && font.handle < fontObjs.len);
	return fontObjs[font.handle].lineHeight;
}

//--------------------------------------------------------------------------------------------------

Res<Canvas> CreateCanvas(U32 width, U32 height) {
	Assert(canvasObjs.HasCapacity());

	Gpu::Image canvasColorImage;
	Try(Gpu::CreateImage(width, height, Gpu::ImageFormat::B8G8R8A8_UNorm, Gpu::ImageUsage::Color | Gpu::ImageUsage::Sampled).To(canvasColorImage));

	Gpu::Image canvasDepthImage;
	if (Res<> r = Gpu::CreateImage(width, height, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::Depth).To(canvasDepthImage); !r) {
		Gpu::DestroyImage(canvasColorImage);
		return r.err;
	}

	Vec2 const size = Vec2((F32)width, (F32)height);
	CanvasObj* canvasObj = canvasObjs.Add();
	U64 canvasObjIdx = (U64)(canvasObj = canvasObjs.data);
	*canvasObj = {
		.colorImage       = canvasColorImage,
		.colorImageIdx    = Gpu::GetImageBindIdx(canvasColorImage),
		.colorImageLayout = Gpu::ImageLayout::Undefined,
		.depthImage       = canvasDepthImage,
		.size             = size,
	};
	Gpu_Namef(canvasColorImage, "canvasColor#%u", canvasObjIdx);
	Gpu_Namef(canvasDepthImage, "canvasDepth#%u", canvasObjIdx);

	scene->projViews[canvasObjIdx] = Math::Ortho(
		0.0f, size.x,
		size.y, 0.0f,
		-100.0f, 100.0f
	);

	return Canvas { .handle = canvasObjIdx };
}

//--------------------------------------------------------------------------------------------------

void BeginFrame(const Gpu::FrameData* gpuFrameData) {
	frameIdx = gpuFrameData->frameIdx;

	canvasObjs[0].colorImage = gpuFrameData->swapchainImage;

	drawCmds = (DrawCmd*)drawCmdBufferPtrs[frameIdx];
	drawCmdCount = 0;

	passes.len = 0;
	passes.Add({
		.canvas       = { .handle = 0 },
		.drawCmdStart = 0,
		.drawCmdEnd   = U32Max,
	});

	camera.pos = Vec2(0.f, 0.f);
	camera.scale = 1.f;
}

//--------------------------------------------------------------------------------------------------

void EndFrame() {
	memcpy(sceneBufferPtrs[frameIdx], scene, sizeof(Scene));

	passes[passes.len - 1].drawCmdEnd = drawCmdCount;
	for (U64 i = 0; i < passes.len; i++) {
		Pass const* const pass = &passes[i];
		U32 const passDrawCmdCount = pass->drawCmdEnd - pass->drawCmdStart;
		if (passDrawCmdCount == 0) {
			continue;
		}

		CanvasObj* canvasObj = &canvasObjs[pass->canvas.handle];
		Gpu::Pass const gpuPass = {
			.pipeline         = pipeline,
			.colorAttachments = { canvasObj->colorImage },
			.depthAttachment  = canvasObj->depthImage,
			.viewport         = { .x = 0.0f, .y = 0.0f, .width = canvasObj->size.x,      .height = canvasObj->size.y },
			.scissor          = { .x = 0,    .y = 0,    .width = (U32)canvasObj->size.x, .height = (U32)canvasObj->size.y },
			.clear            = true,
		};

		if (pass->canvas.handle) {	// not swapchain canvas
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
			.sceneBufferIdx    = (U32)pass->canvas.handle,
			.drawCmdStart      = pass->drawCmdStart,
		};
		Gpu::PushConstants(pipeline, &pushConstants, sizeof(pushConstants));
		Gpu::Draw(6, passDrawCmdCount);
		Gpu::EndPass();

		if (pass->canvas.handle) {	// not swapchain canvas
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
	Assert(passes.HasCapacity());
	passes[passes.len - 1].drawCmdEnd = drawCmdCount;
	passes.Add({
		.canvas       = { .handle = 0 },
		.drawCmdStart = drawCmdCount,
		.drawCmdEnd   = U32Max,
	});
}

void SetCanvas(Canvas canvas) {
	Assert(canvas.handle > 0 && canvas.handle < canvasObjs.len);
	Assert(passes.HasCapacity());
	passes[passes.len - 1].drawCmdEnd = drawCmdCount;
	passes.Add({
		.canvas       = canvas,
		.drawCmdStart = drawCmdCount,
		.drawCmdEnd   = U32Max,
	});
}

//--------------------------------------------------------------------------------------------------

void SetCamera(Camera cameraIn) { camera = cameraIn; }

void ClearCamera() { camera = { .pos = Vec2(0.f, 0.f), .scale = 1.f }; }

//--------------------------------------------------------------------------------------------------

static DrawCmd* AllocDrawCmds(U32 n) {
	Assert(drawCmdCount + n <= MaxDrawCmds);
	DrawCmd* ptr = drawCmds + drawCmdCount;
	drawCmdCount += n;
	return ptr;
}

//--------------------------------------------------------------------------------------------------

static inline Vec3 MakePos(F32 x, F32 y, F32 z) {
	return Vec3(
		Math::Round((x - camera.pos.x) * camera.scale),
		Math::Round((y - camera.pos.y) * camera.scale),
		z
	);
}

//--------------------------------------------------------------------------------------------------

void DrawRect(DrawRectDesc drawRectDesc) {
	F32 x = drawRectDesc.pos.x;
	F32 y = drawRectDesc.pos.y;

	switch (drawRectDesc.origin) {
		case Origin::BottomLeft:
		case Origin::BottomCenter:
		case Origin::BottomRight:
			y -= drawRectDesc.size.y;
			break;
		case Origin::Left:
		case Origin::Center:
		case Origin::Right:
			y -= drawRectDesc.size.y * 0.5f;
			break;
	}
	switch (drawRectDesc.origin) {
		case Origin::BottomCenter:
		case Origin::Center:
		case Origin::TopCenter:
			x -= drawRectDesc.size.x * 0.5f;
			break;
		case Origin::BottomRight:
		case Origin::Right:
		case Origin::TopRight:
			x -= drawRectDesc.size.x;
			break;
	}

	DrawCmd* const drawCmd = AllocDrawCmds(1);
	drawCmd->textureIdx   = 0;
	drawCmd->pos          = MakePos(x, y, drawRectDesc.z);
	drawCmd->size         = Vec2(drawRectDesc.size.x * camera.scale, drawRectDesc.size.y * camera.scale);
	drawCmd->uv1          = Vec2(0.f, 0.f);
	drawCmd->uv2          = Vec2(0.f, 0.f);
	drawCmd->color        = drawRectDesc.color;
	drawCmd->outlineWidth = 0.f;
	drawCmd->outlineColor = Vec4(0.f, 0.f, 0.f, 0.f);
}

//--------------------------------------------------------------------------------------------------

void DrawSprite(DrawSpriteDesc drawSpriteDesc) {
	Assert(drawSpriteDesc.sprite.handle > 0 && drawSpriteDesc.sprite.handle < spriteObjs.len);
	SpriteObj const* const spriteObj = &spriteObjs[drawSpriteDesc.sprite.handle];

	F32 x = drawSpriteDesc.pos.x;
	F32 y = drawSpriteDesc.pos.y;
	F32 const scaledSizeX = spriteObj->size.x * drawSpriteDesc.scale.x;
	F32 const scaledSizeY = spriteObj->size.y * drawSpriteDesc.scale.y;
	switch (drawSpriteDesc.origin) {
		case Origin::BottomLeft:
		case Origin::BottomCenter:
		case Origin::BottomRight:
			y -= scaledSizeY;
			break;
		case Origin::Left:
		case Origin::Center:
		case Origin::Right:
			y -= scaledSizeY * 0.5f;
			break;
	}
	switch (drawSpriteDesc.origin) {
		case Origin::BottomCenter:
		case Origin::Center:
		case Origin::TopCenter:
			x -= scaledSizeX * 0.5f;
			break;
		case Origin::BottomRight:
		case Origin::Right:
		case Origin::TopRight:
			x -= scaledSizeX;
			break;
	}

	DrawCmd* const drawCmd = AllocDrawCmds(1);
	drawCmd->textureIdx   = spriteObj->imageIdx;
	drawCmd->pos          = MakePos(x, y, drawSpriteDesc.z);
	drawCmd->size         = Vec2(scaledSizeX * camera.scale, scaledSizeY * camera.scale);
	drawCmd->uv1          = spriteObj->uv1;
	drawCmd->uv2          = spriteObj->uv2;
	drawCmd->color        = drawSpriteDesc.color;
	drawCmd->outlineWidth = drawSpriteDesc.outlineWidth;
	drawCmd->outlineColor = drawSpriteDesc.outlineColor;

	if (drawSpriteDesc.flip) {
		Swap(drawCmd->uv1.x, drawCmd->uv2.x);
	}
	if (drawSpriteDesc.outlineWidth > 0.f) {
		drawCmd->pos.x -= 1 * camera.scale;
		drawCmd->pos.y -= 1 * camera.scale;
		drawCmd->size.x += 2 * camera.scale;
		drawCmd->size.y += 2 * camera.scale;
		drawCmd->uv1.x -= spriteObj->texelSize.x;
		drawCmd->uv1.y -= spriteObj->texelSize.y;
		drawCmd->uv2.x += spriteObj->texelSize.x;
		drawCmd->uv2.y += spriteObj->texelSize.y;
	}
}

//--------------------------------------------------------------------------------------------------

static F32 StrWidth(FontObj const* fontObj, Str str) {
	if (str.len == 0) {
		return 0;
	}

	F32 width = 0;
	for (U32 i = 0; i < str.len - 1; i++) {
		Glyph const* glyph = &fontObj->glyphs[str[i]];
		width += glyph->xAdv;
	}
	Glyph const* lastGlyph = &fontObj->glyphs[str[str.len - 1]];
	width += lastGlyph->off.x + lastGlyph->size.x;
	//width += lastGlyph->xAdv;
	return width;
}

//--------------------------------------------------------------------------------------------------

void DrawStr(DrawStrDesc drawStrDesc) {
	Assert(drawStrDesc.font.handle > 0 && drawStrDesc.font.handle < fontObjs.len);
	FontObj const* fontObj = &fontObjs[drawStrDesc.font.handle];

	F32 x = drawStrDesc.pos.x;
	F32 y = drawStrDesc.pos.y;
	switch (drawStrDesc.origin) {
		case Origin::BottomLeft:
		case Origin::BottomCenter:
		case Origin::BottomRight:
			y -= fontObj->lineHeight * drawStrDesc.scale.y;
			break;
		case Origin::Left:
		case Origin::Center:
		case Origin::Right:
			y -= (fontObj->lineHeight / 2.f) * drawStrDesc.scale.y;
			break;
		case Origin::BaselineLeft:
		case Origin::BaselineCenter:
		case Origin::BaselineRight:
			y -= fontObj->base * drawStrDesc.scale.y;
			break;
	}
	switch (drawStrDesc.origin) {
		case Origin::BottomCenter:
		case Origin::Center:
		case Origin::TopCenter:
			x -= StrWidth(fontObj, drawStrDesc.str) * drawStrDesc.scale.x * 0.5f;
			break;
		case Origin::BottomRight:
		case Origin::Right:
		case Origin::TopRight:
			x -= StrWidth(fontObj, drawStrDesc.str) * drawStrDesc.scale.x;
			break;
	}
	for (U32 i = 0; i < drawStrDesc.str.len; i++) {
		Glyph const* glyph = &fontObj->glyphs[drawStrDesc.str[i]];
		DrawCmd* drawCmd = AllocDrawCmds(1);
		drawCmd->textureIdx   = glyph->imageIdx;
		drawCmd->pos          = MakePos(x + (glyph->off.x * drawStrDesc.scale.x), y + (glyph->off.y * drawStrDesc.scale.y), drawStrDesc.z);
		drawCmd->size         = Vec2(glyph->size.x * drawStrDesc.scale.x * camera.scale, glyph->size.y * drawStrDesc.scale.y * camera.scale);
		drawCmd->uv1          = glyph->uv1;
		drawCmd->uv2          = glyph->uv2;
		drawCmd->color        = drawStrDesc.color;
		drawCmd->outlineWidth = drawStrDesc.outlineWidth;
		drawCmd->outlineColor = drawStrDesc.outlineColor;
		if (drawStrDesc.outlineWidth > 0.f) {
			drawCmd->pos.x -= 1 * camera.scale;
			drawCmd->pos.y -= 1 * camera.scale;
			drawCmd->size.x += 2 * camera.scale;
			drawCmd->size.y += 2 * camera.scale;
			drawCmd->uv1.x -= fontObj->texelSize.x;
			drawCmd->uv1.y -= fontObj->texelSize.y;
			drawCmd->uv2.x += fontObj->texelSize.x;
			drawCmd->uv2.y += fontObj->texelSize.y;
		}
		x += glyph->xAdv * drawStrDesc.scale.x;
	}
}

//--------------------------------------------------------------------------------------------------

void DrawCanvas(DrawCanvasDesc drawCanvasDesc) {
	Assert(drawCanvasDesc.canvas.handle > 0 && drawCanvasDesc.canvas.handle < canvasObjs.len);
	CanvasObj const* canvasObj = &canvasObjs[drawCanvasDesc.canvas.handle];

	DrawCmd* drawCmd = AllocDrawCmds(1);
	drawCmd->textureIdx   = canvasObj->colorImageIdx;
	drawCmd->pos          = Vec3(Math::Round(drawCanvasDesc.pos.x), Math::Round(drawCanvasDesc.pos.y), drawCanvasDesc.z);
	drawCmd->size         = Vec2(canvasObj->size.x * drawCanvasDesc.scale.x * camera.scale, canvasObj->size.y * drawCanvasDesc.scale.y * camera.scale);
	drawCmd->uv1          = Vec2(0.f, 0.f);
	drawCmd->uv2          = Vec2(1.f, 1.f);
	drawCmd->color        = drawCanvasDesc.color;
	drawCmd->outlineColor = Vec4(0.f, 0.f, 0.f, 0.f);
	drawCmd->outlineWidth = 0.f;
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Draw