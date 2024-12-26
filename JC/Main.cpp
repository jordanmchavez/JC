#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Fmt.h"
#include "JC/FS.h"
#include "JC/Log.h"
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

//--------------------------------------------------------------------------------------------------

struct OrthoCamera {
};

struct PerspectiveCamera {
};

struct AtlasEntry {
	float x1 = 0.0f;
	float y1 = 0.0f;
	float x2 = 0.0f;
	float y2 = 0.0f;
};

struct Atlas {
	Render::Image image      = {};
	AtlasEntry*   entries    = 0;
	u32           entriesLen = 0;
};

struct Sprite {
	Mat2 model     = {};
	Vec2 xyz       = {};
	Vec2 uv1       = {};
	Vec2 uv2       = {};
	u32  textureId = {};
	u32  pad       = {};
};

struct PushConstants {
	Mat4 viewProj         = {};
	u64  spriteBufferAddr = 0;
};

//--------------------------------------------------------------------------------------------------

struct Game : App {
	struct AtlasEntry {
		s8   name      = {};
		Vec2 uv1       = {};
		Vec2 uv2       = {};
		u64  imageAddr = 0;
	};

	Arena*               temp         = 0;
	Arena*               perm         = 0;
	Log*                 log          = 0;
	u32                  windowWidth  = 0;
	u32                  windowHeight = 0;
	Array<Render::Image> atlasImages  = {};
	Array<AtlasEntry>    atlasEntries = {};

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

	Res<> LoadAtlas(s8 name) {
		Render::Image image = {};
		if (Res<> r = LoadImage(Fmt(temp, "{}.png", name)).To(image); !r) { return r; }

		//if (Res<> r = LoadJson
		return Ok();
	}


	Res<> Init(Arena* tempIn, Arena* permIn, Log* logIn, const Window::State* windowState) override {
		temp = tempIn;
		perm = permIn;
		log = logIn;
		windowWidth  = windowState->width;
		windowHeight = windowState->height;
		
		return Ok();
	}

	void Shutdown() override {
	}

	Res<> Events(Span<Event::Event> events) override {
		for (const Event::Event* e = events.Begin(); e != events.End(); e++) {
			switch (e->type) {
				case Event::Type::Exit:
					Exit();
					break;
	
				case Event::Type::Key:
					if (e->key.key == Event::Key::Escape) { Exit(); }
					break;

				case Event::Type::MouseMove:
					break;

				case Event::Type::WindowResized:
					windowWidth  = e->windowResized.width;
					windowHeight = e->windowResized.height;
					break;
			}
		}

		return Ok();
	}

	Res<> Update(double secs) override {
		secs;
		return Ok();
	}

	Res<> Draw() override {
		Render::ImageBarrier(Render::GetSwapchainImage(), Render::Stage::None, Render::Stage::Present);
		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

int main(int argc, const char** argv) {
	Game game;
	RunApp(&game, argc, argv);
	return 0;
}