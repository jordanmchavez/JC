#include "JC/Common.h"
#include "JC/Math.h"
#include "JC/RenderVk.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

Frame
	CommandPool
	CommandBuffer

Renderer
	CreateBuffer()
	CreateTexture()
	CreateSampler()
	CreateShader()
	CreatePipeline()

	u32 AddBindlessTexture(Texture)
	u632AddBindlessSampler(Sampler)

	BindPipeline()
	BindIndexBuffer()

	BeginFrame
		wait frame fence
		begin command buffer

	EndFrame
		acquireImage
		reset frame fence
		transition swapchain image -> layout_general
		clear swapchain image
		possibly copy draw image into swap chain via blit
		possible render imgui
		transition swapchain image -> present optimal
		queuesubmit(wait=acquiresem, signal=rendersem)
		vkQueuePresentKHR
		


	cmdBuffer = renderApi->BeginFrame();
		vkWaitForFence(fences[frame])
		vkAcquireImage(acquireSem)
		reset command buffer
		cmdbegin
		return cmdBuffer

	void* const ptr = renderApi->MapBuffer(sceneStagingBuffer);
	MemCpy(ptr, &sceneStagingData, sizeof(sceneStagingData));
	renderApi->UnmapBuffer(sceneStagingBuffer);
	renderApi->CopyBuffer(sceneStagingBuffer, sceneBuffer);
	renderApi->Barrier(sceneBuffer, transfer/write -> vs+fs/read);

	renderApi->Clear()
		transitionimage(swapchainImages[i] -> VK_IMAGE_LAYOUT_GENERAL)
		clear

	renderApi->BeginRendering()
		transition image -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		set viewport
		bind texture descriptors
	bind pipeline

	for each mesh
		bind index buffer
		push constants
		draw

	end rendering
	renderApi->EndFrame()
		transition image -> VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		end command buffer
		queue submit(wait acquireSem[frame], signal renderSem[frame], signal renderFences[frame])
		present(wait render sem)

struct MeshDraw {
	Mesh mesh;
	Mat4 transform;
	Material material;
};

struct Renderer {
	Mesh CreateMesh(vertices, indices);
	Shader CreateShader(code);
	Pipeline CreatePipeline(Shader vs, Shader fs);
	Entity CreateEntity(Mesh, Material);
};


//--------------------------------------------------------------------------------------------------

}	// namespace JC