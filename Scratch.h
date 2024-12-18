#include "JC/Common.h"

#include "JC/Array.h"
#include "JC/Math.h"
#include "JC/RenderVk.h"

namespace JC {

//--------------------------------------------------------------------------------------------------


perFrameData
	m4 view
	m4 proj
	frustum planes
	camera pos
	max draws
	lod
	hsb

vkWaitForFences
vkResetFences
vkAcquireNextImageKHR
vkResetCommandBuffer
vkBeginCommandBuffer

imageBarrier: swapchain presentsrc/0/topofpipe -> colorattach/colorattachwrite/colorattachoutput
fill drawCountBuffer = 0
bufferBarrier drawCmdBuffer indirect_read/draw_indirect -> shader_write/compute_shader
draw computer shader pass
bufferBarrier drawCountBuffer shader_write/computer_shader ->  indirect_read/draw_indirect
bufferBarrier drawCmdsBuffer  shader_write/computer_shader ->  indirect_read/draw_indirect
geometry pass
	pipeline
		shaders = vert, frag
		attachmentLayout
			colorAttachments { swapchain.format, blend=true }
			depthStencil = depthTexture.format
		rasterization
		depthstencil
	viewport
	scissor
	color attach w/ clear color
	d/s attach w/clear color
	bindings
		0 vertex buffer
		1 draw buffer
		2 drawcmd buffer
	push constants
		perFrameData

	vkCmdBindIndexBuffer
	vkCmdDrawIndexedIndirectCount
vkEndCommandBuffer
vkQueueSubmit
vkQueuePresentKHR

exec pass
	vkCmdBeginRenderingKHR
	vkCmdSetViewport
	vkCmdSetScissor
	vkCmdPushConstants
	vkCmdPushDescriptorSetWithTemplateKHR
	vkCmdBindPipeline
		_callback
	vkCmdEndRenderingKHR



			.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//--------------------------------------------------------------------------------------------------

}	// namespace JC