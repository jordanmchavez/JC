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
#include "JC/UnitTest.h"
#include "JC/Window.h"
#include "stb/stb_image.h"
#include "JC/Render_Vk.h"
#include <math.h>

#undef LoadImage

using namespace JC;

//--------------------------------------------------------------------------------------------------

DefErr(Game, LoadImage);
DefErr(Game, ImageFmt);
DefErr(Game, AtlasNameAlreadyExists);

//--------------------------------------------------------------------------------------------------

struct OrthoCamera {
	Vec3 pos        = {};
	f32  halfWidth  = 0.0f;
	f32  halfHeight = 0.0f;

	void Set(f32 fov, f32 aspect, f32 z) {
		pos.z      = z;
		halfWidth  = z * tanf(fov / 2.0f);
		halfHeight = halfWidth / aspect;
	}

	Mat4 GetProjView() {
		return Mat4::Ortho(
			pos.x - halfWidth,
			pos.x + halfWidth,
			pos.y - halfHeight,
			pos.y + halfHeight,
			10.0f,
			-10.0f
		);
	};
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

//--------------------------------------------------------------------------------------------------

struct Game : App {
	struct SpriteDrawCmd {
		Mat4 model      = {};
		Vec2 uv1        = {};
		Vec2 uv2        = {};
		u32  diffuseIdx = {};
		u32  normalIdx  = {};
		u32  pad[2]     = {};
	};

	struct Light {
		Vec3  position = {};
		float radius   = 0.0f;
		Vec3  color    = {};
		float pad      = 0.0f;
	};

	struct Scene {
		Mat4  projView                = {};
		Light lights[4]               = {};
		u64   spriteDrawCmdBufferAddr = 0;
	};

	struct PushConstants {
		u64  sceneBufferAddr = 0;
	};

	struct AtlasEntry {
		u32  imageIdx = 0;
		s8   name     = {};
		Vec2 uv1      = {};
		Vec2 uv2      = {};
	};

	Arena*               temp                                        = 0;
	Arena*               perm                                        = 0;
	Log*                 log                                         = 0;
	u32                  windowWidth                                 = 0;
	u32                  windowHeight                                = 0;
	Render::Image        depthImage                                  = {};
	Render::Buffer       sceneBuffers[Render::MaxFrames]             = {};
	u64                  sceneBufferAddrs[Render::MaxFrames]         = {};
	Array<Render::Image> atlasImages                                 = {};
	Array<AtlasEntry>    atlasEntries                                = {};
	Map<s8, u32>         atlasEntryMap                               = {};
	Render::Buffer       spriteDrawCmdBuffers[Render::MaxFrames]     = {};
	u64                  spriteDrawCmdBufferAddrs[Render::MaxFrames] = {};
	Render::Shader       vertexShader                                = {};
	Render::Shader       fragmentShader                              = {};
	Render::Pipeline     pipeline                                    = {};
	OrthoCamera          cam                                         = {};

	static constexpr u32 MapSize = 128;
	static constexpr u32 MaxSprites = 128 * 128 * 2;

	const AtlasEntry* map[MapSize * MapSize] = {};
	const AtlasEntry* hero = 0;
	Vec3              heroPos = {};

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
		const Render::StagingMem stagingMem = Render::AllocStagingMem(width * height * 4);
		if (channels == 4) {
			MemCpy(stagingMem.ptr, imageData, width * height * channels);
		} else {
			u8* in = imageData;
			u8* out = (u8*)stagingMem.ptr;
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					*out++ = *in++;
					*out++ = *in++;
					*out++ = *in++;
					*out++ = 0xff;
				}
			}
		}
		Render::UpdateImage(image, stagingMem);
		Render::ImageBarrier(image, Render::Stage::TransferDst, Render::Stage::FragmentShaderSample);

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

			if (Res<> r = Json::GetI64(jsonObj[0].val).To(ix);   !r) { return r; }
			if (Res<> r = Json::GetI64(jsonObj[1].val).To(iy);   !r) { return r; }
			if (Res<> r = Json::GetI64(jsonObj[2].val).To(iw);   !r) { return r; }
			if (Res<> r = Json::GetI64(jsonObj[3].val).To(ih);   !r) { return r; }
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

	const AtlasEntry* FindAtlasEntry(s8 name) {
		Opt<u32> idx = atlasEntryMap.Find(name);
		Assert(idx.hasVal);
		return &atlasEntries[idx.val];
	}

	Res<> Init(Arena* tempIn, Arena* permIn, Log* logIn, const Window::State* windowState) override {
		temp = tempIn;
		perm = permIn;
		log = logIn;
		windowWidth  = windowState->width;
		windowHeight = windowState->height;

		if (Res<> r = Render::CreateImage(windowWidth, windowHeight, Render::ImageFormat::D32_Float, Render::ImageUsage::DepthAttachment).To(depthImage); !r) { return r; }

		for (u32 i = 0; i < Render::MaxFrames; i++) {
			if (Res<> r = Render::CreateBuffer(sizeof(Scene), Render::BufferUsage::Storage).To(sceneBuffers[i]); !r) { return r; }
			sceneBufferAddrs[i] = Render::GetBufferAddr(sceneBuffers[i]);
		}
		
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
			{ "thorns",       1 },              
		};
		u32 maxWeight = 0;
		for (u64 i = 0; i < LenOf(terrainWeights); i++) {
			maxWeight += terrainWeights[i].weight;
			terrainWeights[i].weight = maxWeight;
		}

		for (u32 y = 0; y < MapSize; y++) {
			for (u32 x = 0; x < MapSize; x++) {
				
				const u32 weight = Random::NextU64() % maxWeight;
				s8 terrain = {};
				for (u32 i = 0; i < LenOf(terrainWeights); i++) {
					if (weight < terrainWeights[i].weight) {
						terrain = terrainWeights[i].terrain;
						break;
					}
				}
				Assert(terrain != "");

				map[y * MapSize + x] = FindAtlasEntry(terrain);
			}
		}
		hero = FindAtlasEntry("hero");
		heroPos = { 64 * 16.0f, 64 * 16.0f, 0.0f };

		for (u32 i = 0; i < Render::MaxFrames; i++) {
			if (Res<> r = Render::CreateBuffer(MaxSprites * sizeof(SpriteDrawCmd), Render::BufferUsage::Storage).To(spriteDrawCmdBuffers[i]); !r) { return r; }
			spriteDrawCmdBufferAddrs[i] = Render::GetBufferAddr(spriteDrawCmdBuffers[i]);
		}

		if (Res<> r = LoadShader("Shaders/sprite.vert.spv").To(vertexShader); !r) { return r; }
		if (Res<> r = LoadShader("Shaders/sprite.frag.spv").To(fragmentShader); !r) { return r; }
		if (Res<> r = Render::CreateGraphicsPipeline(
			{ vertexShader, fragmentShader },
			{ Render::GetImageFormat(Render::GetSwapchainImage()) },
			Render::ImageFormat::D32_Float
		).To(pipeline); !r) { return r; }

		cam.Set(45.0f, (f32)windowWidth / (f32)windowHeight, 500);

		return Ok();
	}

	void Shutdown() override {
		DestroyPipeline(pipeline);
		DestroyShader(vertexShader);
		DestroyShader(fragmentShader);
		for (u32 i = 0; i < Render::MaxFrames; i++) {
			Render::DestroyBuffer(spriteDrawCmdBuffers[i]);
		}
		for (u64 i = 0; i < atlasImages.len; i++) {
			Render::DestroyImage(atlasImages[i]);
		}
		for (u32 i = 0; i < Render::MaxFrames; i++) {
			Render::DestroyBuffer(sceneBuffers[i]);
		}
		atlasImages.Clear();
		Render::DestroyImage(depthImage);
	}

	bool keyDown[(u32)Event::Key::Max] = {};

	static constexpr f32 heroMovePerSec  = 20.00f;
	static constexpr f32 camZoomPerWheel = 10.0f;

	Res<> Events(Span<Event::Event> events) override {
		for (const Event::Event* e = events.Begin(); e != events.End(); e++) {
			switch (e->type) {
				case Event::Type::Exit:
					Exit();
					break;
	
				case Event::Type::Key: 
					keyDown[(u32)e->key.key] = e->key.down;
					break;

				case Event::Type::MouseWheel: {
					const f32 m = keyDown[(u32)Event::Key::ShiftLeft] ? camZoomPerWheel * 5.0f : camZoomPerWheel;
					cam.Set(45.0f, (f32)windowWidth / (f32)windowHeight, cam.pos.z - m * e->mouseWheel.delta);
					break;
				}

				case Event::Type::WindowResized:
					windowWidth  = e->windowResized.width;
					windowHeight = e->windowResized.height;
					Render::WaitIdle();
					Render::DestroyImage(depthImage);
					if (Res<> r = Render::CreateImage(windowWidth, windowHeight, Render::ImageFormat::D32_Float, Render::ImageUsage::DepthAttachment).To(depthImage); !r) { return r; }
					cam.Set(45.0f, (f32)windowWidth / (f32)windowHeight, cam.pos.z);
					break;
			}
		}

		return Ok();
	}

	float lightColorIntensity = 1.0f;
	static constexpr double MaxLightChangePeriod = 0.1;
	double lightChangeRem = MaxLightChangePeriod;

	Res<> Update(double secs) override {
		const f32 fsecs = (f32)secs;
		const f32 m = (keyDown[(u32)Event::Key::ShiftLeft] || keyDown[(u32)Event::Key::ShiftRight]) ? heroMovePerSec * 5.0f : heroMovePerSec;
		if (keyDown[(u32)Event::Key::Escape]) { Exit(); }
		if (keyDown[(u32)Event::Key::W     ]) { heroPos.y +=  m * fsecs; }
		if (keyDown[(u32)Event::Key::S     ]) { heroPos.y += -m * fsecs; }
		if (keyDown[(u32)Event::Key::A     ]) { heroPos.x += -m * fsecs; }
		if (keyDown[(u32)Event::Key::D     ]) { heroPos.x +=  m * fsecs; }

		cam.pos.x = heroPos.x;
		cam.pos.y = heroPos.y;

		lightChangeRem -= secs;
		if (lightChangeRem <= 0.0) {
			lightChangeRem = MaxLightChangePeriod;
			//lightColorIntensity = 0.8f + Random::NextF32() * 0.2f;
		}

		return Ok();
	}


	Res<> Draw() override {
		const u32 frameIdx = Render::GetFrameIdx();

		const Render::Image swapchainImage = Render::GetSwapchainImage();

		Render::ImageBarrier(swapchainImage, Render::Stage::PresentOld, Render::Stage::ColorAttachment);

		const Render::StagingMem sceneBufferStagingMem = Render::AllocStagingMem(sizeof(Scene));
		Scene* const scene = (Scene*)sceneBufferStagingMem.ptr;
		scene->projView  = cam.GetProjView(),
		MemSet(scene->lights, 0, sizeof(scene->lights));
		scene->lights[0] = {
			.position = Vec3(64.0f * 16.0f, 64.0f * 16.0f, 1.0f),
			.radius   = 32.0f * 16.0f,
			.color    = Vec3(0.8f, 0.7f, 0.5f),
		};
		scene->lights[1] = {
			.position = Vec3(heroPos.x, heroPos.y, 1.0f),
			.radius   = 8.0f * 16.0f,
			.color    = Vec3(0.4f, 0.6f, 0.5f),
		};
		scene->lights[2] = { {}, 1.0f, {} };
		scene->lights[3] = { {}, 1.0f, {} };
		scene->spriteDrawCmdBufferAddr = spriteDrawCmdBufferAddrs[frameIdx],
		Render::BufferBarrier(sceneBuffers[frameIdx], Render::Stage::VertexShaderRead, Render::Stage::TransferDst);
		Render::UpdateBuffer(sceneBuffers[frameIdx], 0, sceneBufferStagingMem);
		Render::BufferBarrier(sceneBuffers[frameIdx], Render::Stage::TransferDst, Render::Stage::VertexShaderRead);

		f32 startX = Clamp(cam.pos.x - cam.halfWidth,  0.0f, (f32)MapSize * 16.0f);
		f32 endX   = Clamp(cam.pos.x + cam.halfWidth,  0.0f, (f32)MapSize * 16.0f);
		f32 startY = Clamp(cam.pos.y - cam.halfHeight, 0.0f, (f32)MapSize * 16.0f);
		f32 endY   = Clamp(cam.pos.y + cam.halfHeight, 0.0f, (f32)MapSize * 16.0f);
		const u32 startRow = (u32)(startY / 16.0f);
		const u32 endRow   = (u32)(endY   / 16.0f);
		const u32 startCol = (u32)(startX / 16.0f);
		const u32 endCol   = (u32)(endX   / 16.0f);
		const u32 spriteCount = 1 + (endCol - startCol) * (endRow - startRow);

		const Render::StagingMem spriteDrawCmdStagingMem = Render::AllocStagingMem(spriteCount * sizeof(SpriteDrawCmd));
		SpriteDrawCmd* spriteDrawCmds = (SpriteDrawCmd*)spriteDrawCmdStagingMem.ptr;
		*spriteDrawCmds++ = {
			.model      = Mat4::Translate(Vec3 { heroPos.x, heroPos.y, 0.0f }),
			.uv1        = hero->uv1,
			.uv2        = hero->uv2,
			.diffuseIdx = hero->imageIdx,
			.normalIdx  = 0,
		};
		for (u32 row = startRow; row < endRow; row++) {
			for (u32 col = startCol; col < endCol; col++) {
				const AtlasEntry* const atlasEntry = map[row * MapSize + col];
				Assert((u64)atlasEntry < (u64)map + sizeof(map));
				Assert(atlasEntry->imageIdx < atlasEntries.len);
				*spriteDrawCmds++ = {
					.model      = Mat4::Translate(Vec3 { (f32)col * 16.0f, (f32)row * 16.0f, 0.0f }),
					.uv1        = atlasEntry->uv1,
					.uv2        = atlasEntry->uv2,
					.diffuseIdx = atlasEntry->imageIdx,
					.normalIdx  = 0,
				};
			}
		}
		Render::BufferBarrier(spriteDrawCmdBuffers[frameIdx], Render::Stage::VertexShaderRead, Render::Stage::TransferDst);
		Render::UpdateBuffer(spriteDrawCmdBuffers[frameIdx], 0, spriteDrawCmdStagingMem);
		Render::BufferBarrier(spriteDrawCmdBuffers[frameIdx], Render::Stage::TransferDst, Render::Stage::VertexShaderRead);

		const Render::Pass pass = {
			.pipeline         = pipeline,
			.colorAttachments = { swapchainImage },
			.depthAttachment  = depthImage,
			.viewport         = { .x = 0.0f, .y = 0.0f, .w = (f32)windowWidth, .h = (f32)windowHeight },
			.scissor          = { .x = 0,    .y = 0,    .w = windowWidth,      .h = windowHeight },
		};
		Render::BeginPass(&pass);
		PushConstants pushConstants = { .sceneBufferAddr = sceneBufferAddrs[frameIdx] };
		Render::PushConstants(pipeline, &pushConstants, sizeof(pushConstants));
		Render::Draw(6, spriteCount);
		Render::EndPass();

		Render::ImageBarrier(swapchainImage, Render::Stage::ColorAttachment, Render::Stage::Present);

		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static Game game;

int main(int argc, const char** argv) {
	RunApp(&game, argc, argv);
	return 0;
}