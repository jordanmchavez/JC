#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Event.h"
#include "JC/FS.h"
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

	static constexpr U32 MaxVertices = 1024 * 1024;
	static constexpr U32 MaxIndices  = 1024 * 1024;

	Mem::Allocator*     allocator     = 0;
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;

	Gpu::Pool           permPool;
	Gpu::Image          depthImage;
	Gpu::Buffer         vertexBuffer;
	U32                 verticesUsed;
	Gpu::Buffer         indexBuffer;
	U32                 indicesUsed;
	Mesh                cube;
	Mesh                sphere;
	Mesh                icosahedron;
	Mesh                torus;
	Array<Entity>       entities;

	//----------------------------------------------------------------------------------------------

	Res<> PreInit(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn) {
		allocator     = allocatorIn;
		tempAllocator = tempAllocatorIn;
		logger        = loggerIn;

		verticesUsed = 0;
		indicesUsed  = 0;

		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	struct VertexAllocation {
		Gpu::Cmd cmd;
		Vertex*  ptr;
		U32      offset;
	};

	VertexAllocation CmdBeginVertices(Gpu::Cmd cmd, U32 n) {
		Assert(verticesUsed < MaxVertices);
		VertexAllocation va = {
			.cmd    = cmd,
			.ptr    = (Vertex*)Gpu::CmdBeginBufferUpload(cmd, vertexBuffer, verticesUsed * sizeof(Vertex), n * sizeof(Vertex)),
			.offset = verticesUsed,
		};
		verticesUsed += n;
		return va;
	}

	void CmdEndVertices(Gpu::Cmd cmd) {
		Gpu::CmdEndBufferUpload(cmd, vertexBuffer);
	}

	//----------------------------------------------------------------------------------------------
	
	struct IndexAllocation {
		U32* ptr;
		U64  offset;
	};

	IndexAllocation CmdBeginIndices(Gpu::Cmd cmd, U32 n) {
		Assert(indicesUsed < MaxIndices);
		IndexAllocation ia = {
			.ptr    = (U32*)Gpu::CmdBeginBufferUpload(cmd, indexBuffer, indicesUsed * sizeof(U32), n * sizeof(U32)),
			.offset = indicesUsed,
		};
		indicesUsed += n;
		return ia;
	}

	void CmdEndIndices(Gpu::Cmd cmd) {
		Gpu::CmdEndBufferUpload(cmd, indexBuffer);
	}

	//----------------------------------------------------------------------------------------------

	Res<Mesh> CreateCube(Gpu::Cmd cmd) {
		VertexAllocation va = CmdBeginVertices(cmd, 12);
		constexpr float r = -0.5f;
		*va.ptr++ = { .pos = { -r,  r,  r } };
		*va.ptr++ = { .pos = {  r,  r,  r } };
		*va.ptr++ = { .pos = {  r, -r,  r } };
		*va.ptr++ = { .pos = { -r, -r,  r } };
		*va.ptr++ = { .pos = { -r,  r, -r } };
		*va.ptr++ = { .pos = {  r,  r, -r } };
		*va.ptr++ = { .pos = {  r, -r, -r } };
		*va.ptr++ = { .pos = { -r, -r, -r } };
		CmdEndVertices(cmd);

		IndexAllocation ia = CmdBeginIndices(cmd, 6 * 2 * 3);
		*ia.ptr++ = 0; *ia.ptr++ = 1; *ia.ptr++ = 2;
		*ia.ptr++ = 0; *ia.ptr++ = 2; *ia.ptr++ = 3;
		*ia.ptr++ = 1; *ia.ptr++ = 5; *ia.ptr++ = 6;
		*ia.ptr++ = 1; *ia.ptr++ = 6; *ia.ptr++ = 2;
		*ia.ptr++ = 5; *ia.ptr++ = 4; *ia.ptr++ = 7;
		*ia.ptr++ = 5; *ia.ptr++ = 7; *ia.ptr++ = 6;
		*ia.ptr++ = 4; *ia.ptr++ = 0; *ia.ptr++ = 3;
		*ia.ptr++ = 4; *ia.ptr++ = 3; *ia.ptr++ = 7;
		*ia.ptr++ = 4; *ia.ptr++ = 5; *ia.ptr++ = 1;
		*ia.ptr++ = 4; *ia.ptr++ = 1; *ia.ptr++ = 0;
		*ia.ptr++ = 3; *ia.ptr++ = 2; *ia.ptr++ = 6;
		*ia.ptr++ = 3; *ia.ptr++ = 6; *ia.ptr++ = 7;
		CmdEndIndices(cmd);

		return Mesh {
			.vertexOffset = va.offset,
			.vertexCount  = 8,
			.indexOffset  = ia.offset,
			.indexCount   = 6 * 2 * 3,

		};
	}
	/*
	Res<Mesh> CreateIcosahedron() {
		return Ok();
	}

	Res<Mesh> CreateSphere() {
		return Ok();
	}

	Res<Mesh> CreateTorus() {
		return Ok();
	}
	*/
	Res<> InitMeshes() {
		Gpu::Cmd cmd;
		CheckRes(Gpu::BeginImmediateCmds().To(cmd));
		CheckRes(CreateCube(cmd).To(cube));
		//CheckRes(CreateSphere());
		//CheckRes(CreateIcosahedron());
		//CheckRes(CreateTorus());
		CheckRes(Gpu::EndImmediateCmds(Gpu::Stage::TransferDst));

		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	Res<> Init(const Window::State* windowState) {
		permPool = Gpu::PermPool();
		CheckRes(Gpu::CreateImage(permPool, windowState->width, windowState->height, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::DepthAttachment, Gpu::MemUsage::Gpu).To(depthImage));
		CheckRes(Gpu::CreateBuffer(permPool, MaxVertices * sizeof(Vertex), Gpu::BufferUsage::Storage, Gpu::MemUsage::CpuWrite).To(vertexBuffer));
		CheckRes(Gpu::CreateBuffer(permPool, MaxIndices * sizeof(U32), Gpu::BufferUsage::Storage, Gpu::MemUsage::CpuWrite).To(indexBuffer));

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