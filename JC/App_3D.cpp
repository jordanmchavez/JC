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

static constexpr U64 StagingBufferSize = 256 * MB;	// Per frame

struct Vertex {
	Vec3 pos;
	U32  pad;
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

static Res<U32> UploadVertices(Span<const Vertex> vertices) {
	JC_ASSERT(verticesUsed + vertices.len <= MaxVertices);
	JC_CHECK_RES(Gpu::ImmediateCopyToBuffer(vertices.data, vertices.len * sizeof(Vertex), vertexBuffer, verticesUsed * sizeof(Vertex)));
	const U32 offset = verticesUsed;
	verticesUsed += (U32)vertices.len;
	return offset;
}

//--------------------------------------------------------------------------------------------------

static Res<U32> UploadIndices(Span<const U32> indices) {
	JC_ASSERT(indicesUsed + indices.len <= MaxIndices);
	JC_CHECK_RES(Gpu::ImmediateCopyToBuffer(indices.data, indices.len * sizeof(U32), indexBuffer, indicesUsed * sizeof(U32)));
	const U32 offset = indicesUsed;
	indicesUsed += (U32)indices.len;
	return offset;
}

//--------------------------------------------------------------------------------------------------

static Res<Mesh> CreateCube() {
	constexpr float r = -0.5f;
	constexpr Vertex vertices[] = {
		{ .pos = { -r,  r,  r } },
		{ .pos = {  r,  r,  r } },
		{ .pos = {  r, -r,  r } },
		{ .pos = { -r, -r,  r } },
		{ .pos = { -r,  r, -r } },
		{ .pos = {  r,  r, -r } },
		{ .pos = {  r, -r, -r } },
		{ .pos = { -r, -r, -r } },
	};
	U32 vertexOffset = 0;
	JC_CHECK_RES(UploadVertices(vertices).To(vertexOffset));

	constexpr U32 indices[] = {
		0, 1, 2,
		0, 2, 3,
		1, 5, 6,
		1, 6, 2,
		5, 4, 7,
		5, 7, 6,
		4, 0, 3,
		4, 3, 7,
		4, 5, 1,
		4, 1, 0,
		3, 2, 6,
		3, 6, 7,
	};
	U32 indexOffset = 0;
	JC_CHECK_RES(UploadIndices(indices).To(indexOffset));

	return Mesh {
		.vertexOffset = vertexOffset,
		.vertexCount  = JC_LENOF(vertices),
		.indexOffset  = indexOffset,
		.indexCount   = JC_LENOF(indices)
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
/*
static Res<Gpu::Shader> LoadShader(Str path) {
	Span<U8> data;
	CheckRes(FS::ReadAll(tempAllocator, path).To(data));
	Gpu::Shader shader;
	CheckRes(Gpu::CreateShader(data.data, data.len).To(shader));
	return shader;
}
*/
//--------------------------------------------------------------------------------------------------

static Res<> Init(const Window::State* windowState) {
	windowState;

	JC_CHECK_RES(Gpu::CreateImage(windowState->width, windowState->height, Gpu::ImageFormat::D32_Float, Gpu::ImageUsage::Depth).To(depthImage));
	JC_GPU_NAME(depthImage);

	JC_CHECK_RES(Gpu::CreateBuffer(MaxVertices * sizeof(Vertex), Gpu::BufferUsage::Storage | Gpu::BufferUsage::Upload | Gpu::BufferUsage::Addr).To(vertexBuffer));
	vertexBufferAddr = Gpu::GetBufferAddr(vertexBuffer);
	JC_GPU_NAME(vertexBuffer);

	JC_CHECK_RES(Gpu::CreateBuffer(MaxIndices * sizeof(U32), Gpu::BufferUsage::Index | Gpu::BufferUsage::Upload | Gpu::BufferUsage::Addr).To(indexBuffer));
	indexBufferAddr = Gpu::GetBufferAddr(indexBuffer);
	JC_GPU_NAME(indexBuffer);

	/*
	CheckRes(Gpu::CreateBuffer(permPool, MaxMaterials * sizeof(Material), Gpu::BufferUsage::Storage | Gpu::BufferUsage::CopySrc, Gpu::MemUsage::CpuWrite).To(materialBuffer));
	materialBufferAddr = Gpu::GetBufferAddr(materialBuffer);
	Gpu_DbgName(materialBuffer);

	CheckRes(Gpu::CreateBuffer(permPool, MaxEntities * sizeof(Mat4), Gpu::BufferUsage::Storage | Gpu::BufferUsage::CopySrc, Gpu::MemUsage::CpuWrite).To(transformBuffer));
	transformBufferAddr = Gpu::GetBufferAddr(transformBuffer);
	Gpu_DbgName(transformBuffer);

	CheckRes(Gpu::CreateBuffer(permPool, sizeof(Scene), Gpu::BufferUsage::Storage | Gpu::BufferUsage::CopySrc, Gpu::MemUsage::CpuWrite).To(sceneBuffer));
	sceneBufferAddr = Gpu::GetBufferAddr(sceneBuffer);
	Gpu_DbgName(sceneBuffer);

	CheckRes(Gpu::CreateBuffer(permPool, MaxEntities * sizeof(DrawIndirect), Gpu::BufferUsage::Indirect | Gpu::BufferUsage::CopySrc, Gpu::MemUsage::CpuWrite).To(drawIndirectBuffer));
	drawIndirectBufferAddr = Gpu::GetBufferAddr(drawIndirectBuffer);
	Gpu_DbgName(drawIndirectBuffer, "drawIndirect");
	*/

	JC_CHECK_RES(CreateCube().To(cube));
	//CheckRes(CreateSphere());
	//CheckRes(CreateIcosahedron());
	//CheckRes(CreateTorus());
	JC_CHECK_RES(Gpu::ImmediateWait());
/*	Gpu::CmdImageBarrier(
		cmd,
		depthImage,
		Gpu::BarrierStage::None,
		Gpu::BarrierStage::EarlyFragmentTests_DepthStencilAttachmentRead | Gpu::BarrierStage::LateFragmentTests_DepthStencilAttachmentRead,
		Gpu::ImageLayout::Depth
	);
	CheckRes(Gpu::EndImmediateCmds());

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
	*/
	return Ok();
}

//--------------------------------------------------------------------------------------------------

static void Shutdown() {
	Gpu::DestroyImage(depthImage);
	Gpu::DestroyBuffer(vertexBuffer);
	Gpu::DestroyBuffer(indexBuffer);
	//Gpu::DestroyPipeline(pipeline);
	//Gpu::DestroyShader(vertShader);
	//Gpu::DestroyShader(fragShader);
	//Gpu::DestroyBuffer(materialBuffer);
	//Gpu::DestroyBuffer(transformBuffer);
	//Gpu::DestroyBuffer(sceneBuffer);
	//Gpu::DestroyBuffer(drawIndirectBuffer);
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

static Res<> Draw(Gpu::Frame frame) {
	frame;
/*
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
	Gpu::CmdBufferBarrier(cmd, transformBuffer, 0, entities.len * sizeof(Mat4), Gpu::Stage::Write_Copy, Gpu::Stage::Read_VertexShaderStorage);

	Gpu::CmdEndBufferUpload(cmd, drawIndirectBuffer);
	Gpu::CmdBufferBarrier(cmd, drawIndirectBuffer, 0, entities.len * sizeof(Mat4), Gpu::Stage::Write_Copy, Gpu::Stage::Read_VertexShaderStorage);

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
	*/
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