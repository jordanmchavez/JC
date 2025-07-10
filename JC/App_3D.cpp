#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Camera.h"
#include "JC/Event.h"
#include "JC/FS.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Math.h"
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

	static constexpr U32 MaxMaterials = 1024;
	static constexpr U32 MaxVertices  = 1024 * 1024;
	static constexpr U32 MaxIndices   = 1024 * 1024;
	static constexpr U32 MaxEntities  = 1024 * 1024;

	Mem::Allocator*     allocator     = 0;
	Mem::TempAllocator* tempAllocator = 0;
	Log::Logger*        logger        = 0;

	Gpu::Pool           permPool;
	Gpu::Image          depthImage;
	Gpu::Buffer         vertexBuffer;
	U32                 verticesUsed;
	Gpu::Buffer         indexBuffer;
	U32                 indicesUsed;
	Gpu::Buffer         materialBuffer;
	Gpu::Buffer         transformBuffer;
	Gpu::Buffer         sceneBuffer;
	Gpu::Buffer         drawIndirectBuffer;
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
		Gpu::SetName(depthImage, "depth");

		CheckRes(Gpu::CreateBuffer(permPool, MaxVertices * sizeof(Vertex), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(vertexBuffer));
		Gpu::SetName(vertexBuffer, "vertex");

		CheckRes(Gpu::CreateBuffer(permPool, MaxIndices * sizeof(U32), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(indexBuffer));
		Gpu::SetName(indexBuffer, "index");

		CheckRes(Gpu::CreateBuffer(permPool, MaxMaterials * sizeof(Material), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(materialBuffer));
		Gpu::SetName(materialBuffer, "material");

		CheckRes(Gpu::CreateBuffer(permPool, MaxEntities * sizeof(Mat4), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(transformBuffer));
		Gpu::SetName(transformBuffer, "transform");

		CheckRes(Gpu::CreateBuffer(permPool, sizeof(Scene), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(sceneBuffer));
		Gpu::SetName(sceneBuffer, "scene");

		CheckRes(Gpu::CreateBuffer(permPool, MaxEntities * sizeof(DrawIndirect), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(drawIndirectBuffer));
		Gpu::SetName(drawIndirectBuffer, "drawIndirect");

		CheckRes(InitMeshes());

		return Ok();
	}

	//----------------------------------------------------------------------------------------------

	void Shutdown() {
		Gpu::DestroyImage(depthImage);
		Gpu::DestroyBuffer(materialBuffer);
		Gpu::DestroyBuffer(vertexBuffer);
		Gpu::DestroyBuffer(indexBuffer);
		Gpu::DestroyBuffer(transformBuffer);
		Gpu::DestroyBuffer(sceneBuffer);
		Gpu::DestroyBuffer(drawIndirectBuffer);
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

	PerspectiveCamera camera;

	Res<> Draw(Gpu::Cmd cmd, Gpu::Image swapchainImage) {
		const Mat4 view = Math::Look(camera.pos, camera.x, camera.y, camera.z);
		const Mat4 proj = Math::Perspective(Math::DegToRad(camera.fov), camera.aspect, 0.01f, 100000000.0f);
		const Mat4 projView = Math::Mul(view, proj);	// TODO: order?
		*(Scene*)Gpu::CmdBeginBufferUpload(cmd, sceneBuffer, 0, sizeof(Scene)) = {
			.view     = view,
			.proj     = proj,
			.projView = projView,
		};
		Gpu::CmdEndBufferUpload(cmd, sceneBuffer);

		Mat4* const transforms = (Mat4*)Gpu::CmdBeginBufferUpload(cmd, transformBuffer, 0, entities.len * sizeof(Mat4));
		DrawIndirect* drawIndirects = (DrawIndirect*)Gpu::CmdBeginBufferUpload(cmd, drawIndirectBuffer, 0, entities.len * sizeof(DrawIndirect));
		for (U64 i = 0; i < entities.len; i++) {
//			const Entity* const entity = &entities[i];
			// write transform buffer
			// write ind draw cmd

		}
		Gpu::CmdEndBufferUpload(cmd, transformBuffer);
		Gpu::CmdEndBufferUpload(cmd, drawIndirectBuffer);

		Gpu::
		swapchainImage;
		return Ok();
	}
};

//--------------------------------------------------------------------------------------------------

static App_3D app;
App* g_app = &app;

//--------------------------------------------------------------------------------------------------

}	// namespace JC