#include "JC/Common.h"
#include "JC/Math.h"
#include "JC/RenderVk.h"

namespace JC {

//--------------------------------------------------------------------------------------------------

void foo() {
vkBeginCommandBuffer(...);

vkCmdCopyBuffer2(...);

// queue the memory barrier: transfer/write -> vs/read
const VkBufferMemoryBarrier2 VkBufferMemoryBarrier2 = {
	.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
	.pNext               = 0,
	.srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
	.srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
	.dstStageMask        = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
	.dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
	.srcQueueFamilyIndex = 0,
	.dstQueueFamilyIndex = 0,
	.buffer              = stagingBuffer.vkBuffer,
	.offset              = 0,
	.size                = stagingBuffer.size,
};
const VkDependencyInfo vkDependencyInfo = {
	.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
	.pNext                    = 0,
	.dependencyFlags          = 0,
	.memoryBarrierCount       = 0,
	.pMemoryBarriers          = 0,
	.bufferMemoryBarrierCount = 1,
	.pBufferMemoryBarriers    = &vkBufferMemoryBarrier2,
	.imageMemoryBarrierCount  = 0,
	.pImageMemoryBarriers     = 0,
};
vkCmdPipelineBarrier2(vkCommandBuffer, &vkDependencyInfo);

// various binds and draw calls

}


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