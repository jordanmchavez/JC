#include "JC/App.h"
#include "JC/Array.h"
#include "JC/Camera.h"
#include "JC/Event.h"
#include "JC/FS.h"
#include "JC/Gpu.h"
#include "JC/Log.h"
#include "JC/Math.h"
#include "JC/Window.h"

namespace JC::App3D {

//------------------------------------------------------------------------------------------------------

struct Vertex {
	Vec3 pos;
	U32  pad;
};

struct VertexAllocation {
	Gpu::Cmd cmd;
	Vertex*  ptr;
	U32      offset;
};

struct IndexAllocation {
	U32* ptr;
	U32  offset;
};

struct Mesh {
	U32 vertexOffset;
	U32 vertexCount;
	U32 indexOffset;
	U32 indexCount;
};

struct Material {
	Vec4 color;
};

struct Entity {
	const Mesh* mesh;
	U32         materialIdx;
	Vec3        pos;
	Vec3        axis;
	F32         angle;
	F32         angleSpeed;
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

//------------------------------------------------------------------------------------------------------

static constexpr U32 MaxMaterials = 1024;
static constexpr U32 MaxVertices  = 1024 * 1024;
static constexpr U32 MaxIndices   = 1024 * 1024;
static constexpr U32 MaxEntities  = 1024 * 1024;

static Mem::Allocator*     allocator     = 0;
static Mem::TempAllocator* tempAllocator = 0;
static Log::Logger*        logger        = 0;
static Gpu::Pool           permPool;
static Gpu::Image          depthImage;
static Gpu::Buffer         vertexBuffer;
static U64                 vertexBufferAddr;
static U32                 verticesUsed;
static Gpu::Buffer         indexBuffer;
static U64                 indexBufferAddr;
static U32                 indicesUsed;
static Gpu::Buffer         materialBuffer;
static U64                 materialBufferAddr;
static Gpu::Buffer         transformBuffer;
static U64                 transformBufferAddr;
static Gpu::Buffer         sceneBuffer;
static U64                 sceneBufferAddr;
static Gpu::Buffer         drawIndirectBuffer;
static U64                 drawIndirectBufferAddr;
static Mesh                cube;
static Mesh                sphere;
static Mesh                icosahedron;
static Mesh                torus;
static Array<Entity>       entities;
static Gpu::Shader         vertShader;
static Gpu::Shader         fragShader;
static Gpu::Pipeline       pipeline;
static PerspectiveCamera   camera;

//--------------------------------------------------------------------------------------------------

static Res<> PreInit(Mem::Allocator* allocatorIn, Mem::TempAllocator* tempAllocatorIn, Log::Logger* loggerIn) {
	allocator     = allocatorIn;
	tempAllocator = tempAllocatorIn;
	logger        = loggerIn;

	verticesUsed = 0;
	indicesUsed  = 0;

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static VertexAllocation CmdBeginVertices(Gpu::Cmd cmd, U32 n) {
	Assert(verticesUsed < MaxVertices);
	VertexAllocation va = {
		.cmd    = cmd,
		.ptr    = (Vertex*)Gpu::CmdBeginBufferUpload(cmd, vertexBuffer, verticesUsed * sizeof(Vertex), n * sizeof(Vertex)),
		.offset = verticesUsed,
	};
	verticesUsed += n;
	return va;
}

static void CmdEndVertices(Gpu::Cmd cmd) {
	Gpu::CmdEndBufferUpload(cmd, vertexBuffer);
}

//--------------------------------------------------------------------------------------------------
	
static IndexAllocation CmdBeginIndices(Gpu::Cmd cmd, U32 n) {
	Assert(indicesUsed < MaxIndices);
	IndexAllocation ia = {
		.ptr    = (U32*)Gpu::CmdBeginBufferUpload(cmd, indexBuffer, indicesUsed * sizeof(U32), n * sizeof(U32)),
		.offset = indicesUsed,
	};
	indicesUsed += n;
	return ia;
}

static void CmdEndIndices(Gpu::Cmd cmd) {
	Gpu::CmdEndBufferUpload(cmd, indexBuffer);
}

//--------------------------------------------------------------------------------------------------

static Res<Mesh> CreateCube(Gpu::Cmd cmd) {
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
//--------------------------------------------------------------------------------------------------

static Res<Mesh> CreateIcosahedron() {
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<Mesh> CreateSphere() {
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<Mesh> CreateTorus() {
	return Ok();
}
*/

//--------------------------------------------------------------------------------------------------

static Res<> InitMeshes() {
	Gpu::Cmd cmd;
	CheckRes(Gpu::BeginImmediateCmds().To(cmd));
	CheckRes(CreateCube(cmd).To(cube));
	//CheckRes(CreateSphere());
	//CheckRes(CreateIcosahedron());
	//CheckRes(CreateTorus());
	CheckRes(Gpu::EndImmediateCmds(Gpu::Stage::TransferDst));

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<Gpu::Shader> LoadShader(Str path) {
	Span<U8> data;
	CheckRes(FS::ReadAll(tempAllocator, path).To(data));
	Gpu::Shader shader;
	CheckRes(Gpu::CreateShader(data.data, data.len).To(shader));
	return shader;
}

//--------------------------------------------------------------------------------------------------

static Res<> Init(const Window::State* windowState) {
	permPool = Gpu::PermPool();

	CheckRes(Gpu::CreateImage(permPool, windowState->width, windowState->height, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::DepthAttachment, Gpu::MemUsage::Gpu).To(depthImage));
	Gpu::SetName(depthImage, "depth");

	CheckRes(Gpu::CreateBuffer(permPool, MaxVertices * sizeof(Vertex), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(vertexBuffer));
	vertexBufferAddr = Gpu::GetBufferAddr(vertexBuffer);
	Gpu::SetName(vertexBuffer, "vertex");

	CheckRes(Gpu::CreateBuffer(permPool, MaxIndices * sizeof(U32), Gpu::BufferUsage::Index | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(indexBuffer));
	indexBufferAddr = Gpu::GetBufferAddr(indexBuffer);
	Gpu::SetName(indexBuffer, "index");

	CheckRes(Gpu::CreateBuffer(permPool, MaxMaterials * sizeof(Material), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(materialBuffer));
	materialBufferAddr = Gpu::GetBufferAddr(materialBuffer);
	Gpu::SetName(materialBuffer, "material");

	CheckRes(Gpu::CreateBuffer(permPool, MaxEntities * sizeof(Mat4), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(transformBuffer));
	transformBufferAddr = Gpu::GetBufferAddr(transformBuffer);
	Gpu::SetName(transformBuffer, "transform");

	CheckRes(Gpu::CreateBuffer(permPool, sizeof(Scene), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(sceneBuffer));
	sceneBufferAddr = Gpu::GetBufferAddr(sceneBuffer);
	Gpu::SetName(sceneBuffer, "scene");

	CheckRes(Gpu::CreateBuffer(permPool, MaxEntities * sizeof(DrawIndirect), Gpu::BufferUsage::Indirect | Gpu::BufferUsage::Upload, Gpu::MemUsage::CpuWrite).To(drawIndirectBuffer));
	drawIndirectBufferAddr = Gpu::GetBufferAddr(drawIndirectBuffer);
	Gpu::SetName(drawIndirectBuffer, "drawIndirect");

	CheckRes(InitMeshes());

	CheckRes(LoadShader("Shaders/MeshVert.spv").To(vertShader));
	CheckRes(LoadShader("Shaders/MeshFrag.spv").To(fragShader));

	CheckRes(Gpu::CreateGraphicsPipeline(
		{ vertShader, fragShader },
		{ Gpu::GetSwapchainImageFormat() },
		Gpu::ImageFormat::D32_Float
	).To(pipeline));

	entities.Init(allocator);
	entities.Add(Entity {
		.mesh        = &cube,
		.materialIdx = 0,
		.pos         = { 0.0f, 0.0f, 0.0f },
		.axis        = { 0.0f, 0.0f, 0.0f },
		.angle       = 0.0f,
		.angleSpeed  = 0.001f,

	});

	return Ok();
}

//--------------------------------------------------------------------------------------------------

static void Shutdown() {
	Gpu::DestroyPipeline(pipeline);
	Gpu::DestroyShader(vertShader);
	Gpu::DestroyShader(fragShader);
	Gpu::DestroyImage(depthImage);
	Gpu::DestroyBuffer(materialBuffer);
	Gpu::DestroyBuffer(vertexBuffer);
	Gpu::DestroyBuffer(indexBuffer);
	Gpu::DestroyBuffer(transformBuffer);
	Gpu::DestroyBuffer(sceneBuffer);
	Gpu::DestroyBuffer(drawIndirectBuffer);
}

//--------------------------------------------------------------------------------------------------

static Res<> Events(Span<Event::Event> events) {
	for (U64 i = 0; i < events.len; i++) {
		const Event::Event* const ev = &events[i];
		switch (ev->type) {
			case Event::Type::Exit:
				App::Exit();
				break;
		}
	}
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<> Update(double secs) {
	secs;
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static Res<> Draw(Gpu::Cmd cmd, Gpu::Image swapchainImage) {
	const Mat4 view = Math::Look(camera.pos, camera.x, camera.y, camera.z);
	const Mat4 proj = Math::Perspective(Math::DegToRad(camera.fov), camera.aspect, 0.01f, 100000000.0f);
	const Mat4 projView = Math::Mul(view, proj);	// TODO: order?
	*(Scene*)Gpu::CmdBeginBufferUpload(cmd, sceneBuffer, 0, sizeof(Scene)) = {
		.view     = view,
		.proj     = proj,
		.projView = projView,
	};
	Gpu::CmdEndBufferUpload(cmd, sceneBuffer);

	Mat4* transforms = (Mat4*)Gpu::CmdBeginBufferUpload(cmd, transformBuffer, 0, entities.len * sizeof(Mat4));
	DrawIndirect* drawIndirects = (DrawIndirect*)Gpu::CmdBeginBufferUpload(cmd, drawIndirectBuffer, 0, entities.len * sizeof(DrawIndirect));
	for (U64 i = 0; i < entities.len; i++) {
		const Entity* const entity = &entities[i];
		*transforms++ = Math::AxisAngleMat4(entity->axis, entity->angle);
		*drawIndirects++ = {
			.indexCount     = entity->mesh->indexCount,
			.instanceCount  = 1,
			.firstIndex     = entity->mesh->indexOffset,
			.vertexOffset   = entity->mesh->vertexOffset,
			.firstInstance  = 0,
		};

	}
	Gpu::CmdEndBufferUpload(cmd, transformBuffer);
	Gpu::CmdEndBufferUpload(cmd, drawIndirectBuffer);

	Gpu::CmdBufferBarrier(cmd, transformBuffer, 0, entities.len * sizeof(Mat4), Gpu::Stage::TransferSrc, Gpu::Stage::

	const U32 swapchainImageWidth  = Gpu::GetImageWidth(swapchainImage);
	const U32 swapchainImageHeight = Gpu::GetImageHeight(swapchainImage);
	const Gpu::Pass pass = {
		.pipeline         = pipeline,
		.colorAttachments = { swapchainImage },
		.depthAttachment  = depthImage,
		.viewport         = { .x = 0.0f, .y = 0.0f, .w = (F32)swapchainImageWidth, .h = (F32)swapchainImageHeight },
		.scissor          = { .x = 0,    .y = 0,    .w = swapchainImageWidth,      .h = swapchainImageHeight },
		.clear            = true,
	};
	Gpu::CmdBeginPass(cmd, &pass);
	const PushConstants pushConstants = {
		.vertexBufferAddr    = vertexBufferAddr,
		.transformBufferAddr = transformBufferAddr,
		.materialBufferAddr  = materialBufferAddr,
		.sceneBufferAddr     = sceneBufferAddr,
	};
	Gpu::CmdPushConstants(cmd, pipeline, &pushConstants, sizeof(pushConstants));
	Gpu::CmdBindIndexBuffer(cmd, indexBuffer);
	Gpu::CmdDrawIndexedIndirect(cmd, drawIndirectBuffer, (U32)entities.len);
	Gpu::CmdEndPass(cmd);

	return Ok();
}

//------------------------------------------------------------------------------------------------------

App::Fns appFns = {
	.PreInit  = PreInit,
	.Init     = Init,
	.Shutdown = Shutdown,
	.Events   = Events,
	.Update   = Update,
	.Draw     = Draw,
};

}	// namespace JC::App3D