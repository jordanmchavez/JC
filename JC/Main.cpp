#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/FS.h"
#include "JC/Hash.h"
#include "JC/Json.h"
#include "JC/Log.h"
#include "JC/Map.h"
#include "JC/Math.h"
#include "JC/Random.h"
#include "JC/Render.h"
#include "JC/Unicode.h"
#include "JC/UnitTest.h"
#include "JC/Window.h"
#include <stdio.h>
#include "stb/stb_image.h"
#include "JC/MinimalWindows.h"
#include "JC/Render_Vk.h"

#undef LoadImage

using namespace JC;

//--------------------------------------------------------------------------------------------------

DefErr(Game, LoadImage);
DefErr(Game, ImageFmt);
DefErr(Game, AtlasNameAlreadyExists);

//--------------------------------------------------------------------------------------------------

struct OrthoCamera {
};

struct PerspectiveCamera {
	Vec3 pos    = {};
	Vec3 x      = { -1.0f, 0.0f,  0.0f };
	Vec3 y      = {  0.0f, 1.0f,  0.0f };
	Vec3 z      = {  0.0f, 0.0f, -1.0f };
	f32  yaw    = 0.0f;
	f32  pitch  = 0.0f;
	f32  fov    = 0.0f;
	f32  aspect = 0.0f;

	void Forward(f32 delta) { pos = Vec3::AddScaled(pos, z, delta); }
	void Left   (f32 delta) { pos = Vec3::AddScaled(pos, x, delta); }
	void Up     (f32 delta) { pos = Vec3::AddScaled(pos, y, delta); }

	void Yaw(f32 delta) {
		yaw += delta;
		z = Mat3::Mul(Mat3::RotateY(yaw), Vec3 { 0.0f, 0.0f, -1.0f });
		x = Vec3::Cross(Vec3 { 0.0f, 1.0f, 0.0f }, z);
	}

	void Pitch(f32 delta) {
		pitch += delta;
		z = Mat3::Mul(Mat3::AxisAngle(x, pitch), z);
		y = Vec3::Cross(z, x);
	}

	Mat4 GetProjView() {
		return Mat4::Mul(
			Mat4::Look(pos, x, y, z),
			Mat4::Perspective(DegToRad(fov), aspect, 0.01f, 100000000.0f)
		);
	}
};

struct Sprite {
	Mat4 model    = {};
	Vec2 uv1      = {};
	Vec2 uv2      = {};
	u32  imageIdx = {};
	u32  pad[3]   = {};
};

struct PushConstants {
	Mat4 projView         = {};
	u64  spriteBufferAddr = 0;
};

//--------------------------------------------------------------------------------------------------

struct Game : App {
	struct AtlasEntry {
		u32 imageIdx = 0;
		s8  name     = {};
		Vec2 uv1     = {};
		Vec2 uv2     = {};
	};

	Arena*               temp             = 0;
	Arena*               perm             = 0;
	Log*                 log              = 0;
	u32                  windowWidth      = 0;
	u32                  windowHeight     = 0;
	Array<Render::Image> atlasImages      = {};
	Array<AtlasEntry>    atlasEntries     = {};
	Map<s8, u32>         atlasEntryMap    = {};
	Render::Buffer       spriteBuffer     = {};
	u64                  spriteBufferAddr = 0;
	Render::Shader       vertexShader     = {};
	Render::Shader       fragmentShader   = {};
	Render::Pipeline     pipeline         = {};
	PerspectiveCamera    cam              = {};

	Res<Render::Image> LoadImage(s8 path) {
		Span<u8> data;
		if (Res<> r = FS::ReadAll(temp, path).To(data); !r) { return r.err; }

		int width = 0;
		int height = 0;
		int channels = 0;
		u8* imageData = (u8*)stbi_load_from_memory(data.data, (int)data.len, &width, &height, &channels, 0);
		if (!imageData) {
			return Err_LoadImage("path", path, "desc", stbi_failure_reason());
		}
		if (channels != 3 && channels != 4) {
			return Err_ImageFmt("path", path, "channels", channels);
		}
		Defer { stbi_image_free(imageData); };

		Render::Image image;
		if (Res<> r = Render::CreateImage(width, height, Render::ImageFormat::R8G8B8A8_UNorm, Render::ImageUsage::Sampled).To(image); !r) { return r.err; }

		Render::ImageBarrier(image, Render::Stage::None, Render::Stage::TransferDst);
		void* ptr = Render::BeginImageUpdate(image);
		if (channels == 4) {
			MemCpy(ptr, imageData, width * height * channels);
		} else {
			u8* in = imageData;
			u8* out = (u8*)ptr;
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					*out++ = *in++;
					*out++ = *in++;
					*out++ = *in++;
					*out++ = 0xff;
				}
			}
		}
		Render::EndImageUpdate(image);
		Render::ImageBarrier(image, Render::Stage::TransferDst, Render::Stage::ShaderSampled);

		return image;
	}

	Res<Render::Shader> LoadShader(s8 path) {
		Span<u8> data;
		if (Res<> r = FS::ReadAll(temp, path).To(data); !r) { return r; }
		return Render::CreateShader(data.data, data.len);
	}

	Res<Json::Elem> LoadJson(s8 path) {
		Span<u8> data;
		if (Res<> r = FS::ReadAll(temp, path).To(data); !r) { return r.err; }
		return Json::Parse(temp, temp, s8((const char*)data.data, data.len));
	}

	Res<> LoadAtlas(s8 fileName) {
		Render::Image image = {};
		if (Res<> r = LoadImage(Fmt(temp, "Assets/{}.png", fileName)).To(image); !r) { return r; }
		atlasImages.Add(image);
		const u32 imageIdx = Render::BindImage(image);

		Json::Elem jsonRoot = {};
		if (Res<> r = LoadJson(Fmt(temp, "Assets/{}.atlas", fileName)).To(jsonRoot); !r) { return r; }

		Span<Json::Elem> jsonArr = {};
		if (Res<> r = Json::GetArr(jsonRoot).To(jsonArr); !r) { return r; }

		const f32 imageWidth  = (float)Render::GetImageWidth(image);
		const f32 imageHeight = (float)Render::GetImageHeight(image);
		for (u64 i = 0; i < jsonArr.len; i++) {
			Span<Json::KeyVal> jsonObj = {};
			if (Res<> r = Json::GetObj(jsonArr[i]).To(jsonObj); !r) { return r; }
			Assert(jsonObj[0].key == "x");
			Assert(jsonObj[1].key == "y");
			Assert(jsonObj[2].key == "w");
			Assert(jsonObj[3].key == "h");
			Assert(jsonObj[4].key == "name");

			i64 ix    = 0;
			i64 iy    = 0;
			i64 iw    = 0;
			i64 ih    = 0;
			s8  name = {};

			if (Res<> r = Json::GetI64(jsonObj[0].val).To(ix);    !r) { return r; }
			if (Res<> r = Json::GetI64(jsonObj[1].val).To(iy);    !r) { return r; }
			if (Res<> r = Json::GetI64(jsonObj[2].val).To(iw);    !r) { return r; }
			if (Res<> r = Json::GetI64(jsonObj[3].val).To(ih);    !r) { return r; }
			if (Res<> r = Json::GetS8 (jsonObj[4].val).To(name); !r) { return r; }

			const f32 x = (f32)ix / imageWidth;
			const f32 y = (f32)iy / imageHeight;
			const f32 w = (f32)iw / imageWidth;
			const f32 h = (f32)ih / imageHeight;

			AtlasEntry* entry = atlasEntries.Add();
			entry->imageIdx = imageIdx;
			entry->uv1      = { x, y },
			entry->uv2      = { x + w, y + h },
			entry->name     = Copy(perm, name);

			if (name != "") {
				if (atlasEntryMap.Find(name)) {
					return Err_AtlasNameAlreadyExists("filename", fileName, "name", name);
				}
				atlasEntryMap.Put(entry->name, (u32)(entry - atlasEntries.data));
			}
		}

		return Ok();
	}

	AtlasEntry* FindAtlasEntry(s8 name) {
		Opt<u32> idx = atlasEntryMap.Find(name);
		Assert(idx.hasVal);
		return &atlasEntries[idx.val];
	}

	Sprite* sprites = 0;
	u32 spritesLen = 0;

	Res<> Init(Arena* tempIn, Arena* permIn, Log* logIn, const Window::State* windowState) override {
		temp = tempIn;
		perm = permIn;
		log = logIn;
		windowWidth  = windowState->width;
		windowHeight = windowState->height;
		
		atlasImages.Init(perm);
		atlasEntries.Init(perm);
		atlasEntryMap.Init(perm);

		if (Res<> r = LoadAtlas("fnw"); !r) { return r; }

		struct TerrainWeight {
			s8  terrain = {};
			u32 weight  = 0;
		};
		TerrainWeight terrainWeights[] = {
			{ "ground1",    100 },              
			{ "ground2",     50 },              
			{ "ground3",     10 },              
			{ "groundSpark",  2 },              
			{ "grass",        5 },              
			{ "bush",         4 },              
			{ "rocks1",       4 },              
			{ "rocks2",       2 },              
			{ "tree1",        4 },              
			{ "tree2",        3 },              
			{ "tree3",        2 },              
			{ "tree4",        1 },              
			{ "rock1",        3 },              
			{ "rock2",        1 },              
			{ "iron",         1 },              
			{ "sulfur",       1 },              
			{ "obsidian1",    3 },              
			{ "obsidian2",    2 },              
			{ "obsidian3",    1 },              
			{ "petr1",        2 },              
			{ "petr2",        1 },              
		};
		u32 maxWeight = 0;
		for (u64 i = 0; i < LenOf(terrainWeights); i++) {
			maxWeight += terrainWeights[i].weight;
			terrainWeights[i].weight = maxWeight;
		}

		constexpr u32 spritesWidth  = 1024;
		constexpr u32 spritesHeight = 1024;
		spritesLen = spritesWidth * spritesHeight;
		sprites = perm->AllocT<Sprite>(spritesLen);
		for (u32 y = 0; y < spritesWidth; y++) {
			for (u32 x = 0; x < spritesHeight; x++) {
				
				const u32 weight = Random::NextU64() % maxWeight;
				s8 terrain = {};
				for (u32 i = 0; i < LenOf(terrainWeights); i++) {
					if (weight <= terrainWeights[i].weight) {
						terrain = terrainWeights[i].terrain;
						break;
					}
				}
				Assert(terrain != "");

				AtlasEntry* atlasEntry = FindAtlasEntry(terrain);
				sprites[y * spritesWidth + x] = {
					.model    = Mat4::Translate(Vec3 { (f32)x, (f32)y, 0.0f }),
					.uv1      = atlasEntry->uv1,
					.uv2      = atlasEntry->uv2,
					.imageIdx = atlasEntry->imageIdx,
				};
			}
		}

		if (Res<> r = Render::CreateBuffer(spritesLen * sizeof(Sprite), Render::BufferUsage::Storage).To(spriteBuffer); !r) { return r; }
		spriteBufferAddr = Render::GetBufferAddr(spriteBuffer);

		Sprite* spritePtr = (Sprite*)Render::BeginBufferUpdate(spriteBuffer);
		for (u32 i = 0; i < spritesLen; i++) {
			*spritePtr = sprites[i];
			spritePtr++;
		}
		Render::EndBufferUpdate(spriteBuffer);

		if (Res<> r = LoadShader("Shaders/sprite.vert.spv").To(vertexShader); !r) { return r; }
		if (Res<> r = LoadShader("Shaders/sprite.frag.spv").To(fragmentShader); !r) { return r; }
		if (Res<> r = Render::CreateGraphicsPipeline(
			{ vertexShader, fragmentShader },
			{ Render::GetSwapchainImageFormat() },
			Render::ImageFormat::Undefined
		).To(pipeline); !r) { return r; }

		cam.aspect = (f32)windowWidth / (f32)windowHeight;
		cam.fov = 45.0f;
		cam.pos = { 64.0f, 64.0f, -100.0f };

		return Ok();
	}

	void Shutdown() override {
		DestroyPipeline(pipeline);
		DestroyShader(vertexShader);
		DestroyShader(fragmentShader);
		Render::DestroyBuffer(spriteBuffer);
		for (u64 i = 0; i < atlasImages.len; i++) {
			Render::DestroyImage(atlasImages[i]);
		}
		atlasImages.Clear();
	}

	bool keyDown[(u32)Event::Key::Max] = {};

	static constexpr f32 camMovePerSec  = 20.00f;
	static constexpr f32 camRotateSpeed = 0.001f;

	Res<> Events(Span<Event::Event> events) override {
		for (const Event::Event* e = events.Begin(); e != events.End(); e++) {
			switch (e->type) {
				case Event::Type::Exit:
					Exit();
					break;
	
				case Event::Type::Key: 
					keyDown[(u32)e->key.key] = e->key.down;
					break;

				case Event::Type::MouseMove:
					cam.Yaw(-(f32)e->mouseMove.x * camRotateSpeed);
					cam.Pitch((f32)e->mouseMove.y * camRotateSpeed);
					break;

				case Event::Type::WindowResized:
					windowWidth  = e->windowResized.width;
					windowHeight = e->windowResized.height;
					cam.aspect = (f32)windowWidth / (f32)windowHeight;
					break;
			}
		}

		return Ok();
	}

	Res<> Update(double secs) override {
		const f32 fsecs = (f32)secs;
		const f32 m = keyDown[(u32)Event::Key::ShiftLeft] ? camMovePerSec * 5.0f : camMovePerSec;
		if (keyDown[(u32)Event::Key::Escape]) { Exit(); }
		if (keyDown[(u32)Event::Key::W     ]) { cam.Forward(-m * fsecs); }
		if (keyDown[(u32)Event::Key::S     ]) { cam.Forward( m * fsecs); }
		if (keyDown[(u32)Event::Key::A     ]) { cam.Left   (-m * fsecs); }
		if (keyDown[(u32)Event::Key::D     ]) { cam.Left   ( m * fsecs); }
		if (keyDown[(u32)Event::Key::Space ]) { cam.Up     ( m * fsecs); }
		if (keyDown[(u32)Event::Key::C     ]) { cam.Up     (-m * fsecs); }

		return Ok();
	}

	Res<> Draw() override {
		const Render::Image swapchainImage = Render::GetSwapchainImage();

		Render::ImageBarrier(Render::GetSwapchainImage(), Render::Stage::None, Render::Stage::ColorAttachment);

		const Render::Pass pass = {
			.pipeline         = pipeline,
			.colorAttachments = { swapchainImage },
			.depthAttachment  = {},
			.viewport         = { .x = 0.0f, .y = 0.0f, .w = (f32)windowWidth, .h = (f32)windowHeight },
			.scissor          = { .x = 0,    .y = 0,    .w = windowWidth,      .h = windowHeight },
		};

		Render::BeginPass(&pass);
		PushConstants pushConstants = {
			.projView         = cam.GetProjView(),
			.spriteBufferAddr = spriteBufferAddr,
		};
		Render::PushConstants(pipeline, &pushConstants, sizeof(pushConstants));
		Render::Draw(6, spritesLen);
		Render::EndPass();

		Render::ImageBarrier(Render::GetSwapchainImage(), Render::Stage::ColorAttachment, Render::Stage::Present);

		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

int main(int argc, const char** argv) {
	Game game;
	RunApp(&game, argc, argv);
	return 0;
}