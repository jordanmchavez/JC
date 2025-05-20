#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Camera.h"
#include "JC/Event.h"
#include "JC/Math.h"
#include "JC/Render.h"
#include "JC/Window.h"
#include <math.h>

namespace JC {

//--------------------------------------------------------------------------------------------------
/*
	Res<Render::Shader> LoadShader(s8 path) {
		Span<u8> data;
		if (Res<> r = FS::ReadAll(temp, path).To(data); !r) { return r; }
		return Render::CreateShader(data.data, data.len);
	}

	void Shutdown() override {
		DestroyPipeline(pipeline);
		DestroyShader(vertexShader);
		DestroyShader(fragmentShader);
		for (U32 i = 0; i < Render::MaxFrames; i++) {
			Render::DestroyBuffer(spriteDrawCmdBuffers[i]);
		}
		for (U64 i = 0; i < atlasImages.len; i++) {
			Render::DestroyImage(atlasImages[i]);
		}
		for (U32 i = 0; i < Render::MaxFrames; i++) {
			Render::DestroyBuffer(sceneBuffers[i]);
		}
		atlasImages.Clear();
		Render::DestroyImage(depthImage);
	}
*/

struct Game : App {
	Mem::Allocator*     allocator                                   = 0;
	Mem::TempAllocator* tempAllocator                               = 0;
	Log::Logger*        logger                                      = 0;
	U32                 windowWidth                                 = 0;
	U32                 windowHeight                                = 0;
	Render::Sprite      shipSprite                                  = {};
	Vec2                shipPos                                     = {};

	//---------------------------------------------------------------------------------------------

	Res<> Init(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn, const Window::State* windowState) override {
		allocator     = allocatorIn;
		tempAllocator = tempAllocatorIn;
		logger        = loggerIn;
		windowWidth   = windowState->width;
		windowHeight  = windowState->height;

		if (Res<> r = Render::CreateImage(windowWidth, windowHeight, Render::ImageFormat::D32_Float, Render::ImageUsage::DepthAttachment).To(depthImage); !r) { return r; }

		for (U32 i = 0; i < Render::MaxFrames; i++) {
			if (Res<> r = Render::CreateBuffer(sizeof(Scene), Render::BufferUsage::Storage).To(sceneBuffers[i]); !r) { return r; }
			sceneBufferAddrs[i] = Render::GetBufferAddr(sceneBuffers[i]);
		}
		
		for (U32 i = 0; i < Render::MaxFrames; i++) {
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

		camera.Set(45.0f, (F32)windowWidth / (F32)windowHeight, 500);

		Sprite::LoadAtlas("Assets/SpaceShooter.png", "Assets/SpaceShooter.atlas");
		shipSprite = Sprite::Get("Ship1");

		return Ok();
	}

	//---------------------------------------------------------------------------------------------

	void Shutdown() override {
	}

	//---------------------------------------------------------------------------------------------

	Res<> Events(Span<Event::Event> events) override {
		for (U64 i = 0; i < events.len; i++) {
			switch (events[i].type) {
				case Event::Type::Exit: {
					Exit();
					break;
				}

				case Event::Type::WindowResized:
					windowWidth  = events[i].windowResized.width;
					windowHeight = events[i].windowResized.height;
					Render::WaitIdle();
					Render::DestroyImage(depthImage);
					if (Res<> r = Render::CreateImage(windowWidth, windowHeight, Render::ImageFormat::D32_Float, Render::ImageUsage::DepthAttachment).To(depthImage); !r) { return r; }
					camera.Set(45.0f, (F32)windowWidth / (F32)windowHeight, camera.pos.z);
					break;
			}
		}
		return Ok();
	}

	//---------------------------------------------------------------------------------------------

	Res<> Update(double secs) override {
		secs;
		return Ok();
	}

	//---------------------------------------------------------------------------------------------

	Res<> Draw() override {
		const U32 frameIdx = Render::GetFrameIdx();

		const Render::Image swapchainImage = Render::GetSwapchainImage();

		Render::ImageBarrier(swapchainImage, Render::Stage::PresentOld, Render::Stage::ColorAttachment);

		const Render::StagingMem sceneBufferStagingMem = Render::AllocStagingMem(sizeof(Scene));
		Scene* const scene = (Scene*)sceneBufferStagingMem.ptr;
		scene->projView  = camera.GetProjView(),
		scene->spriteDrawCmdBufferAddr = spriteDrawCmdBufferAddrs[frameIdx],
		Render::BufferBarrier(sceneBuffers[frameIdx], Render::Stage::VertexShaderRead, Render::Stage::TransferDst);
		Render::UpdateBuffer(sceneBuffers[frameIdx], 0, sceneBufferStagingMem);
		Render::BufferBarrier(sceneBuffers[frameIdx], Render::Stage::TransferDst, Render::Stage::VertexShaderRead);

		f32 startX = Clamp(cam.pos.x - cam.halfWidth,  0.0f, (f32)MapSize * 16.0f);
		f32 endX   = Clamp(cam.pos.x + cam.halfWidth,  0.0f, (f32)MapSize * 16.0f);
		f32 startY = Clamp(cam.pos.y - cam.halfHeight, 0.0f, (f32)MapSize * 16.0f);
		f32 endY   = Clamp(cam.pos.y + cam.halfHeight, 0.0f, (f32)MapSize * 16.0f);
		const U32 startRow = (U32)(startY / 16.0f);
		const U32 endRow   = (U32)(endY   / 16.0f);
		const U32 startCol = (U32)(startX / 16.0f);
		const U32 endCol   = (U32)(endX   / 16.0f);
		const U32 spriteCount = 1 + (endCol - startCol) * (endRow - startRow);

		const Render::StagingMem spriteDrawCmdStagingMem = Render::AllocStagingMem(spriteCount * sizeof(SpriteDrawCmd));
		SpriteDrawCmd* spriteDrawCmds = (SpriteDrawCmd*)spriteDrawCmdStagingMem.ptr;
		*spriteDrawCmds++ = ;

		Render::BufferBarrier(spriteDrawCmdBuffers[frameIdx], Render::Stage::VertexShaderRead, Render::Stage::TransferDst);
		Render::UpdateBuffer(spriteDrawCmdBuffers[frameIdx], 0, spriteDrawCmdStagingMem);
		Render::BufferBarrier(spriteDrawCmdBuffers[frameIdx], Render::Stage::TransferDst, Render::Stage::VertexShaderRead);

		const Render::Pass pass = {
			.pipeline         = pipeline,
			.colorAttachments = { swapchainImage },
			.depthAttachment  = depthImage,
			.viewport         = { .x = 0.0f, .y = 0.0f, .w = (F32)windowWidth, .h = (F32)windowHeight },
			.scissor          = { .x = 0,    .y = 0,    .w = windowWidth,      .h = windowHeight },
		};
		Render::BeginPass(&pass);
		PushConstants pushConstants = { .sceneBufferAddr = sceneBufferAddrs[frameIdx] };
		Render::PushConstants(pipeline, &pushConstants, sizeof(pushConstants));
		Render::Draw(6, spriteCount);
		Render::EndPass();

		Render::ImageBarrier(swapchainImage, Render::Stage::ColorAttachment, Render::Stage::Present);







		Array<Sprite::Sprite> sprites(tempAllocator);
		sprites.Add(shipSprite);
		Sprite::Draw(sprites);

		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static Game game;

App* GetApp() { return &game; }

//--------------------------------------------------------------------------------------------------

}	// namespace JC