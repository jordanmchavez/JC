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
//--------------------------------------------------------------------------------------------------
/*
	Res<DeviceMem> AllocateImage(VkImage vkImage, VkMemoryPropertyFlags vkMemoryPropertyFlags) {
		VkMemoryRequirements vkMemoryRequirements = {};
		vkGetImageMemoryRequirements(vkDevice, vkImage, &vkMemoryRequirements);

		DeviceMem deviceMem = {};
		if (Res<> r = AllocateDeviceMem(vkMemoryRequirements, vkMemoryPropertyFlags, 0).To(deviceMem); !r) {
			return r.err;
		}
		
		if (const VkResult r = vkBindImageMemory(vkDevice, vkImage, deviceMem.vkDeviceMemory, 0); r != VK_SUCCESS) {
			FreeDeviceMem(deviceMem);
			return MakeVkErr(r, vkBindImageMemory);
		}
	
		return deviceMem;
	}

	//-------------------------------------------------------------------------------------------------

	Res<Image> CreateImage(u32 width, u32 height, VkFormat vkFormat, VkImageUsageFlags vkImageUsageFlags) {
		const VkImageCreateInfo vkImageCreateInfo = {
			.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext                 = 0,
			.flags                 = 0,
			.imageType             = VK_IMAGE_TYPE_2D,
			.format                = vkFormat,
			.extent                = { .width = width, .height = height, .depth = 1 },
			.mipLevels             = 1,
			.arrayLayers           = 1,
			.samples               = VK_SAMPLE_COUNT_1_BIT,
			.tiling                = VK_IMAGE_TILING_OPTIMAL,
			.usage                 = vkImageUsageFlags,
			.sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices   = 0,
			.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		Image image;
		CheckVk(vkCreateImage(vkDevice, &vkImageCreateInfo, vkAllocationCallbacks, &image.vkImage));
		image.width    = width;
		image.height   = height;
		image.vkFormat = vkFormat;

		if (Res<> r = AllocateImageDeviceMem(image.vkImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).To(image.deviceMem); !r) {
			DestroyImage(image);
			return r.err;
		}

		const VkImageViewCreateInfo vkImageViewCreateInfo = {
			.sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext              = 0,
			.flags              = 0,
			.image              = image.vkImage,
			.viewType           = VK_IMAGE_VIEW_TYPE_2D,
			.format             = image.vkFormat,
			.components         = {
				.r              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b              = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a              = VK_COMPONENT_SWIZZLE_IDENTITY,
			},
			.subresourceRange   = {
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel   = 0,
				.levelCount     = 1,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
		};
		if (VkResult r = vkCreateImageView(vkDevice, &vkImageViewCreateInfo, vkAllocationCallbacks, &image.vkImageView); r != VK_SUCCESS) {
			DestroyImage(image);
			return MakeVkErr(r, vkCreateImageView);
		}

		return image;
	}

	//-------------------------------------------------------------------------------------------------

	void DestroyImage(Image image) {
		if (image.vkImageView) {
			vkDestroyImageView(vkDevice, image.vkImageView, vkAllocationCallbacks);
		}
		FreeDeviceMem(image.deviceMem);
		if (image.vkImage) {
			vkDestroyImage(vkDevice, image.vkImage, vkAllocationCallbacks);
		}
	}

	//-------------------------------------------------------------------------------------------------


	Res<> UploadImage(Image image, const void* data, u64 size) {
		Buffer stagingBuffer;
		if (Res<> r = CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT).To(stagingBuffer); !r) {
			return r;
		}
		Defer { DestroyBuffer(stagingBuffer); };

		u8* ptr = 0;
		CheckVk(vkMapMemory(vkDevice, stagingBuffer.deviceMem.vkDeviceMemory, stagingBuffer.deviceMem.offset, size, 0, (void**)&ptr));
		MemCpy(ptr + stagingBuffer.deviceMem.offset, data, size);
		vkUnmapMemory(vkDevice, stagingBuffer.deviceMem.vkDeviceMemory);

		constexpr VkCommandBufferBeginInfo vkCommandBufferBeginInfo = {
			.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext            = 0,
			.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = 0,
		};
		CheckVk(vkBeginCommandBuffer(vkResourceCommandBuffer, &vkCommandBufferBeginInfo));

		TransitionImage(vkResourceCommandBuffer, image.vkImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		const VkBufferImageCopy2 vkBufferImageCopy2 = {
			.sType              = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
			.pNext              = 0,
			.bufferOffset       = 0,
			.bufferRowLength    = 0,
			.bufferImageHeight  = 0,
			.imageSubresource   = {
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel       = 0,
				.baseArrayLayer = 0,
				.layerCount     = 1,
			},
			.imageOffset        = { 0, 0, 0 },
			.imageExtent        = { image.width, image.height, 1 },
		};
		const VkCopyBufferToImageInfo2 vkCopyBufferToImageInfo2 = {
			.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
			.pNext          = 0,
			.srcBuffer      = stagingBuffer.vkBuffer,
			.dstImage       = image.vkImage,
			.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.regionCount    = 1,
			.pRegions       = &vkBufferImageCopy2,
		};
		vkCmdCopyBufferToImage2(vkResourceCommandBuffer, &vkCopyBufferToImageInfo2);

		TransitionImage(vkResourceCommandBuffer, image.vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkEndCommandBuffer(vkResourceCommandBuffer);

		const VkCommandBufferSubmitInfo vkCommandBufferSubmitInfo = {
			.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.pNext         = 0,
			.commandBuffer = vkResourceCommandBuffer,
			.deviceMask    = 0,
		};
		const VkSubmitInfo2 vkSubmitInfo2 = {
			.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.pNext                    = 0,
			.flags                    = 0,
			.waitSemaphoreInfoCount   = 0,
			.pWaitSemaphoreInfos      = 0,
			.commandBufferInfoCount   = 1,
			.pCommandBufferInfos      = &vkCommandBufferSubmitInfo,
			.signalSemaphoreInfoCount = 0,
			.pSignalSemaphoreInfos    = 0,
		};
		CheckVk(vkResetFences(vkDevice, 1, &vkResourceFence));
		CheckVk(vkQueueSubmit2(vkQueue, 1, &vkSubmitInfo2, vkResourceFence));
		CheckVk(vkWaitForFences(vkDevice, 1, &vkResourceFence, VK_TRUE, U64Max));
	
		return Ok();
	}

	//-------------------------------------------------------------------------------------------------
	*/

//--------------------------------------------------------------------------------------------------

}	// namespace JC