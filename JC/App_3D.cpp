#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Window.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

struct App_3D : App {
	struct Vertex {
		Vec3 pos;
		U32  pad;
	};

	struct Mesh {
		U64 vertexOffset;
		U64 vertexCount;
		U64 indexOffset;
		U64 indexCount;
	};

	struct Material {
		Vec4 color;
	};

	struct Entity {
		U32  meshIdx;
		U32  materialIdx;
		Vec3 pos;
		Vec3 axis;
		F32  angle;
		F32  angleSpeed;
	};

	struct Scene {
		Mat4 view;
		Mat4 proj;
		Mat4 projView;
	};

	struct DrawIndirect {
		U32 indexCount;
		U32 instanceCount;
		U32 firstIndex;
		U32 vertexOffset;
		U32 firstInstance;
	};

	struct PushConstants {
		U64 vertexBufferAddr;
		U64 transformBufferAddr;
		U64 materialBufferAddr;
		U64 sceneBufferAddr;
	};

	//----------------------------------------------------------------------------------------------

	static constexpr U64 VertexBufferSize = 1024 * 1024 * sizeof(Vertex);
	static constexpr U64 IndexBufferSize  = 1024 * 1024 * sizeof(U32);

	Mem::Allocator*     allocator     = 0;
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;

	Gpu::Image          depthImage;
	Gpu::Buffer         vertexBuffer;
	U64                 vertexBufferUsed;
	Gpu::Buffer         indexBuffer;
	U64                 indexBufferUsed;

	Array<Entity>       entities;

	//----------------------------------------------------------------------------------------------

	Res<> PreInit(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn) {
		allocator     = allocatorIn;
		tempAllocator = tempAllocatorIn;
		logger        = loggerIn;

		vertexBufferUsed = 0;
		indexBufferUsed  = 0;

		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	TODO: begin mesh / end mesh
	return construct a mesh thingie
	Res<Vertex*> BeginVertices(U32 n) {
		Gpu::Cmd cmd;
		CheckRes(Gpu::BeginImmediateCmds().To(cmd));
		const U64 sizeNeeded = n * sizeof(Vertex);
		Assert(vertexBufferUsed + sizeNeeded < VertexBufferSize);
		Vertex* const vertices = (Vertex*)Gpu::CmdBeginBufferUpload(cmd, vertexBuffer, vertexBufferUsed, sizeNeeded);
		vertexBufferUsed += sizeNeeded;
		return vertices;
	}

	Res<> EndVertices() {
		CheckRes(Gpu::EndImmediateCmds(Gpu::Stage::TransferDst));
		return Ok();
	}

	//----------------------------------------------------------------------------------------------
	
	Res<U32*> BeginIndices(U32 n) {
		Gpu::Cmd cmd;
		CheckRes(Gpu::BeginImmediateCmds().To(cmd));
		const U64 sizeNeeded = n * sizeof(U32);
		Assert(indexBufferUsed + sizeNeeded < IndexBufferSize);
		U32* const indices = (U32*)Gpu::CmdBeginBufferUpload(cmd, indexBuffer, indexBufferUsed, sizeNeeded);
		indexBufferUsed += sizeNeeded;
		return indices;
	}

	Res<> EndIndices() {
		CheckRes(Gpu::EndImmediateCmds(Gpu::Stage::TransferDst));
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	Res<Mesh> InitCube() {
		Vertex* vertices;
		CheckRes(BeginVertices(12).To(vertices));
		constexpr float r = -0.5f;
		*vertices++ = { .pos = {  r,  r,  r } };
		*vertices++ = { .pos = { -r,  r,  r } };
		*vertices++ = { .pos = { -r, -r,  r } };
		*vertices++ = { .pos = {  r, -r,  r } };
		*vertices++ = { .pos = {  r,  r, -r } };
		*vertices++ = { .pos = { -r,  r, -r } };
		*vertices++ = { .pos = { -r, -r, -r } };
		*vertices++ = { .pos = {  r, -r, -r } };
		CheckRes(EndVertices());

		U32* indices;
		CheckRes(BeginIndices(6 * 2 * 3).To(indices));
		CheckRes(EndIndices());

		return Ok();
	}

	Res<Mesh> InitIcosahedron() {
		return Ok();
	}

	Res<Mesh> InitSphere() {
		return Ok();
	}

	Res<Mesh> InitTorus() {
		return Ok();
	}

	Res<Mesh> InitMeshes() {
		CheckRes(InitCube());
		CheckRes(InitSphere());
		CheckRes(InitIcosahedron());
		CheckRes(InitTorus());
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	Res<> Init(const Window::State* windowState) {
		CheckRes(Gpu::CreateImage(windowState->width, windowState->height, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::DepthAttachment).To(depthImage));
		CheckRes(Gpu::CreateBuffer(VertexBufferSize, Gpu::BufferUsage::Storage | Gpu::BufferUsage::CpuWrite).To(vertexBuffer));
		CheckRes(Gpu::CreateBuffer(IndexBufferSize, Gpu::BufferUsage::Storage | Gpu::BufferUsage::CpuWrite).To(indexBuffer));

		Gpu::SetName(depthImage, "depth");
		Gpu::SetName(vertexBuffer, "vertices");
		Gpu::SetName(indexBuffer, "indices");

		CheckRes(InitMeshes());

		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	void Shutdown() {
		Gpu::DestroyImage(depthImage);
		Gpu::DestroyBuffer(vertexBuffer);
		Gpu::DestroyBuffer(indexBuffer);
	}

	//----------------------------------------------------------------------------------------------

	Res<> Events(Span<Event::Event> events) {
		for (U64 i = 0; i < events.len; i++) {
			const Event::Event* const ev = &events[i];
			switch (ev->type) {
				case Event::Type::Exit:
					Exit();
					break;
			}
		}
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	Res<> Update(double secs) {
		secs;
		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	Res<> Draw(Gpu::Cmd cmd, Gpu::Image swapchainImage) {
		cmd;swapchainImage;
		// setup scene buffer
		// map transform buffer
		// map indirect draw buffer
		for (U64 i = 0; i < entities.len; i++) {
//			const Entity* const entity = &entities[i];
			// write transform buffer
			// write ind draw cmd
		}
		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static App_3D app;
App* g_app = &app;

//--------------------------------------------------------------------------------------------------

}	// namespace JC