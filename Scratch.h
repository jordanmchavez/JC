#include <windows.h>
#include <math.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES			// do not declared prototypes, so I can load dynamically!
#include "vulkan/vulkan_core.h"


struct shader_uniform {

	VkBuffer buffer;
	VkDeviceMemory memory;

	float *projectionMatrix;
	float *viewMatrix;
	float *modelMatrix;

};

struct vulkan_context {
								
	uint32_t width;
	uint32_t height;

	float cameraZ;
	int32_t cameraZDir;

	uint32_t presentQueueIdx;
	VkQueue presentQueue;

	VkInstance instance;

	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;

	VkImage *presentImages;
	VkImage depthImage;
	VkImageView depthImageView;
	VkFramebuffer *frameBuffers;

	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties physicalDeviceProperties;
	VkPhysicalDeviceMemoryProperties memoryProperties;

	VkDevice device;

	VkCommandBuffer setupCmdBuffer;
	VkCommandBuffer drawCmdBuffer;

	VkRenderPass renderPass;

	VkBuffer vertexInputBuffer;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;

	shader_uniform uniforms;

	VkDebugUtilsMessengerEXT callback;
};

vulkan_context context;


void render( ) {

	if( context.cameraZ <= 1 ) {
		context.cameraZ = 1;
		context.cameraZDir = 1;
	} else if( context.cameraZ >= 10 ) {
		context.cameraZ = 10;
		context.cameraZDir = -1;
	}

	context.cameraZ += context.cameraZDir * 0.01f;
	context.uniforms.viewMatrix[11] = context.cameraZ;

	// update shader uniforms:
	void *matrixMapped;
	vkMapMemory( context.device, context.uniforms.memory, 0, VK_WHOLE_SIZE, 0, &matrixMapped );
	
	memcpy( matrixMapped, context.uniforms.projectionMatrix, sizeof(float) * 16 );
	memcpy( ((float *)matrixMapped + 16), context.uniforms.viewMatrix, sizeof(float) * 16 );
	memcpy( ((float *)matrixMapped + 32), context.uniforms.modelMatrix, sizeof(float) * 16 );

	VkMappedMemoryRange memoryRange = {};
	memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	memoryRange.memory = context.uniforms.memory;
	memoryRange.offset = 0;
	memoryRange.size = VK_WHOLE_SIZE;
	
	vkFlushMappedMemoryRanges( context.device, 1, &memoryRange );
 
	vkUnmapMemory( context.device, context.uniforms.memory );


	VkSemaphore presentCompleteSemaphore, renderingCompleteSemaphore;
	VkSemaphoreCreateInfo semaphoreCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, 0, 0 };
	vkCreateSemaphore( context.device, &semaphoreCreateInfo, NULL, &presentCompleteSemaphore );
	vkCreateSemaphore( context.device, &semaphoreCreateInfo, NULL, &renderingCompleteSemaphore );

	uint32_t nextImageIdx;
	vkAcquireNextImageKHR(  context.device, context.swapChain, UINT64_MAX,
							presentCompleteSemaphore, VK_NULL_HANDLE, &nextImageIdx );

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer( context.drawCmdBuffer, &beginInfo );

	// barrier for reading from uniform buffer after all writing is done:
	VkMemoryBarrier uniformMemoryBarrier = {};
	uniformMemoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	uniformMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	uniformMemoryBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;

	vkCmdPipelineBarrier(   context.drawCmdBuffer, 
							VK_PIPELINE_STAGE_HOST_BIT, 
							VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
							0,
							1, &uniformMemoryBarrier,
							0, NULL, 
							0, NULL );

	// change image layout from VK_IMAGE_LAYOUT_PRESENT_SRC_KHR to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	VkImageMemoryBarrier layoutTransitionBarrier = {};
	layoutTransitionBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	layoutTransitionBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	layoutTransitionBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	layoutTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	layoutTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	layoutTransitionBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	layoutTransitionBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	layoutTransitionBarrier.image = context.presentImages[ nextImageIdx ];
	VkImageSubresourceRange resourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	layoutTransitionBarrier.subresourceRange = resourceRange;

	vkCmdPipelineBarrier(   context.drawCmdBuffer, 
							VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
							VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							0,
							0, NULL,
							0, NULL, 
							1, &layoutTransitionBarrier );

	// activate render pass:
	VkClearValue clearValue[] = { { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0, 0.0 } };
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = context.renderPass;
	renderPassBeginInfo.framebuffer = context.frameBuffers[ nextImageIdx ];
	renderPassBeginInfo.renderArea = { 0, 0, context.width, context.height };
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValue;
	vkCmdBeginRenderPass( context.drawCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE );

	// bind the graphics pipeline to the command buffer. Any vkDraw command afterwards is affeted by this pipeline!
	vkCmdBindPipeline( context.drawCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipeline );	

	// take care of dynamic state:
	VkViewport viewport = { 0, 0, (float)context.width, (float)context.height, 0, 1 };
	vkCmdSetViewport( context.drawCmdBuffer, 0, 1, &viewport );

	VkRect2D scissor = { 0, 0, context.width, context.height };
	vkCmdSetScissor( context.drawCmdBuffer, 0, 1, &scissor);

	// bind our shader parameters:
	vkCmdBindDescriptorSets(	context.drawCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
								context.pipelineLayout, 0, 1, &context.descriptorSet, 0, NULL );

	// render the triangle:
	VkDeviceSize offsets = { };
	vkCmdBindVertexBuffers( context.drawCmdBuffer, 0, 1, &context.vertexInputBuffer, &offsets );

	vkCmdDraw( context.drawCmdBuffer, 3, 1, 0, 0 );

	vkCmdEndRenderPass( context.drawCmdBuffer );

	// change layout back to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	VkImageMemoryBarrier prePresentBarrier = {};
	prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	prePresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	prePresentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	prePresentBarrier.image = context.presentImages[ nextImageIdx ];
	vkCmdPipelineBarrier( context.drawCmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &prePresentBarrier );

	vkEndCommandBuffer( context.drawCmdBuffer );

	// present:
	VkFence renderFence;
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	vkCreateFence( context.device, &fenceCreateInfo, NULL, &renderFence );

	VkPipelineStageFlags waitStageMash = { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &presentCompleteSemaphore;
	submitInfo.pWaitDstStageMask = &waitStageMash;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &context.drawCmdBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderingCompleteSemaphore;
	vkQueueSubmit( context.presentQueue, 1, &submitInfo, renderFence );

	vkWaitForFences( context.device, 1, &renderFence, VK_TRUE, UINT64_MAX );
	vkDestroyFence( context.device, renderFence, NULL );

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderingCompleteSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &context.swapChain;
	presentInfo.pImageIndices = &nextImageIdx;
	presentInfo.pResults = NULL;
	vkQueuePresentKHR( context.presentQueue, &presentInfo );

	vkDestroySemaphore( context.device, presentCompleteSemaphore, NULL );
	vkDestroySemaphore( context.device, renderingCompleteSemaphore, NULL );
}
						
int CALLBACK WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	const double PI = 3.14159265359f;
	const double TORAD = PI/180.0f;

	// perspective projection parameters:
	float fov = 45.0f;
	float nearZ = 0.1f;
	float farZ = 1000.0f;
	float aspectRatio = context.width / (float)context.height;
	float t = 1.0f / tan( fov * TORAD * 0.5 );
	float nf = nearZ - farZ;

	float projectionMatrix[16] = { t / aspectRatio, 0, 0, 0,
									0, t, 0, 0,
									0, 0, (-nearZ-farZ) / nf, (2*nearZ*farZ) / nf,
									0, 0, 1, 0 };

	float viewMatrix[16] = {	1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								0, 0, 0, 1 };

	float modelMatrix[16] = {   1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								0, 0, 0, 1 };

	// animate camera:
	context.cameraZ = 10.0f;
	context.cameraZDir = -1.0f;
	viewMatrix[11] = context.cameraZ;

	// store matrices in our uniforms
	context.uniforms.projectionMatrix = projectionMatrix;
	context.uniforms.viewMatrix = viewMatrix;
	context.uniforms.modelMatrix = modelMatrix;
	
	// create our uniforms buffers:
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = sizeof(float) * 16 * 3; // size in bytes
	bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	result = vkCreateBuffer( context.device, &bufferCreateInfo, NULL, &context.uniforms.buffer );  
	checkVulkanResult( result, "Failed to create uniforms buffer." );

	// allocate memory for buffer:
	VkMemoryRequirements bufferMemoryRequirements = {};
	vkGetBufferMemoryRequirements( context.device, context.uniforms.buffer, &bufferMemoryRequirements );

	VkMemoryAllocateInfo matrixAllocateInfo = {};
	matrixAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	matrixAllocateInfo.allocationSize = bufferMemoryRequirements.size;

	uint32_t uniformMemoryTypeBits = bufferMemoryRequirements.memoryTypeBits;
	VkMemoryPropertyFlags uniformDesiredMemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	for( uint32_t i = 0; i < 32; ++i ) {
		VkMemoryType memoryType = context.memoryProperties.memoryTypes[i];
		if( uniformMemoryTypeBits & 1 ) {
			if( ( memoryType.propertyFlags & uniformDesiredMemoryFlags ) == uniformDesiredMemoryFlags ) {
				matrixAllocateInfo.memoryTypeIndex = i;
				break;
			}
		}
		uniformMemoryTypeBits = uniformMemoryTypeBits >> 1;
	}

	//VkDeviceMemory bufferMemory;
	result = vkAllocateMemory( context.device, &matrixAllocateInfo, NULL, &context.uniforms.memory );
	checkVulkanResult( result, "Failed to allocate uniforms buffer memory." );

	result = vkBindBufferMemory( context.device, context.uniforms.buffer, context.uniforms.memory, 0 );
	checkVulkanResult( result, "Failed to bind uniforms buffer memory." );

	// set buffer content:
	void *matrixMapped;
	result = vkMapMemory( context.device, context.uniforms.memory, 0, VK_WHOLE_SIZE, 0, &matrixMapped );
	checkVulkanResult( result, "Failed to mapp uniform buffer memory." );
	
	memcpy( matrixMapped, &projectionMatrix, sizeof( projectionMatrix ) );
	memcpy( ((float *)matrixMapped + 16), &viewMatrix, sizeof( viewMatrix ) );
	memcpy( ((float *)matrixMapped + 32), &modelMatrix, sizeof( modelMatrix ) );
	
	vkUnmapMemory( context.device, context.uniforms.memory );


	// create our texture:
	// sampler:
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.minLod = 0;
	samplerCreateInfo.maxLod = 5;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

	VkSampler sampler;
	result = vkCreateSampler( context.device, &samplerCreateInfo, NULL, &sampler );
	checkVulkanResult( result, "Failed to create sampler." );

	// image & image view:
	struct loaded_image {
		int width;
		int height;
		void *data;
	};

	loaded_image testImage;
	testImage.width = 800;
	testImage.height = 600;
	testImage.data = (void *) new float[ testImage.width * testImage.height * 4 ];

	for( uint32_t x = 0; x < testImage.width; ++x ) {
		for( uint32_t y = 0; y < testImage.height; ++y ) {
			float g = 0.3;
			if( x % 40 < 20 && y % 40 < 20 ) {
				g = 1;
			}
			if( x % 40 >= 20 && y % 40 >= 20) {
				g = 1;
			}

			float *pixel = ((float *) testImage.data) + ( x * testImage.height * 4 ) + (y * 4);
			pixel[0] = g * 0.4;
			pixel[1] = g * 0.5;
			pixel[2] = g * 0.7;
			pixel[3] = 1.0;
		}
	}

	// // load our bmp using win32:
	// HBITMAP bmpHandle = (HBITMAP) LoadImage(	 NULL,   // Loading a resource from file.
	//						 "..\\texture.bmp",
	//						 IMAGE_BITMAP,
	//						 0, 0,	  // use real image size
	//						 LR_LOADFROMFILE );

	// assert( bmpHandle, "Failed to load bmp file." );

	// BITMAP bitmap;
	// GetObject( bmpHandle, sizeof(bitmap), (void*)&bitmap );

	// uint32_t buffSize = bitmap.bmWidth * bitmap.bmHeight * 3;
	// void *bitmapBits = new DWORD[ buffSize ];
	// uint32_t bitsRead = GetBitmapBits( bmpHandle, buffSize, bitmapBits );
	// assert( bitsRead == buffSize, "Could not read bitmap bits." );

	VkImageCreateInfo textureCreateInfo = {};
	textureCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	textureCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	textureCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT; //VK_FORMAT_R32G32B32A32_SFLOAT; // VK_FORMAT_R32G32B32_SFLOAT;
	//textureCreateInfo.extent = { bitmap.bmWidth, bitmap.bmHeight, 1 };
	textureCreateInfo.extent = { (uint32_t) testImage.width, (uint32_t) testImage.height, 1 };
	textureCreateInfo.mipLevels = 1;
	textureCreateInfo.arrayLayers = 1;
	textureCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	textureCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
	textureCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	textureCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

	VkImage textureImage;
	result = vkCreateImage( context.device, &textureCreateInfo, NULL, &textureImage );
	checkVulkanResult( result, "Failed to create texture image." );

	// allocate and bind image memory:
	VkMemoryRequirements textureMemoryRequirements = {};
	vkGetImageMemoryRequirements( context.device, textureImage, &textureMemoryRequirements );

	VkMemoryAllocateInfo textureImageAllocateInfo = {};
	textureImageAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	textureImageAllocateInfo.allocationSize = textureMemoryRequirements.size;

	uint32_t textureMemoryTypeBits = textureMemoryRequirements.memoryTypeBits;
	VkMemoryPropertyFlags tDesiredMemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	for( uint32_t i = 0; i < 32; ++i ) {
		VkMemoryType memoryType = context.memoryProperties.memoryTypes[i];
		if( textureMemoryTypeBits & 1 ) {
			if( ( memoryType.propertyFlags & tDesiredMemoryFlags ) == tDesiredMemoryFlags ) {
				textureImageAllocateInfo.memoryTypeIndex = i;
				break;
			}
		}
		textureMemoryTypeBits = textureMemoryTypeBits >> 1;
	}

	VkDeviceMemory textureImageMemory = {};
	result = vkAllocateMemory( context.device, &textureImageAllocateInfo, NULL, &textureImageMemory );
	checkVulkanResult( result, "Failed to allocate device memory." );

	result = vkBindImageMemory( context.device, textureImage, textureImageMemory, 0 );
	checkVulkanResult( result, "Failed to bind image memory." );

	// copy our image content:
	void *imageMapped;
	result = vkMapMemory( context.device, textureImageMemory, 0, VK_WHOLE_SIZE, 0, &imageMapped );
	checkVulkanResult( result, "Failed to map uniform buffer memory." );
	
	memcpy( imageMapped, testImage.data, sizeof(float) * testImage.width * testImage.height * 4 );

	VkMappedMemoryRange memoryRange = {};
	memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	memoryRange.memory = textureImageMemory;
	memoryRange.offset = 0;
	memoryRange.size = VK_WHOLE_SIZE;
	vkFlushMappedMemoryRanges( context.device, 1, &memoryRange );
	
	vkUnmapMemory( context.device, textureImageMemory );

	//delete[] bitmapBits;
	delete[] testImage.data;


	// before using the texture image must change to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL layout
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer( context.setupCmdBuffer, &beginInfo );

		VkImageMemoryBarrier layoutTransitionBarrier = {};
		layoutTransitionBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		layoutTransitionBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		layoutTransitionBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		layoutTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		layoutTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		layoutTransitionBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		layoutTransitionBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		layoutTransitionBarrier.image = textureImage;
		VkImageSubresourceRange resourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		layoutTransitionBarrier.subresourceRange = resourceRange;

		vkCmdPipelineBarrier(   context.setupCmdBuffer, 
								VK_PIPELINE_STAGE_HOST_BIT,
								VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								0,
								0, NULL,
								0, NULL, 
								1, &layoutTransitionBarrier );

		vkEndCommandBuffer( context.setupCmdBuffer );

		VkPipelineStageFlags waitStageMash[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = NULL;
		submitInfo.pWaitDstStageMask = waitStageMash;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &context.setupCmdBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = NULL;
		result = vkQueueSubmit( context.presentQueue, 1, &submitInfo, submitFence );

		vkWaitForFences( context.device, 1, &submitFence, VK_TRUE, UINT64_MAX );
		vkResetFences( context.device, 1, &submitFence );
		vkResetCommandBuffer( context.setupCmdBuffer, 0 );
	}


	// TODO(jhenriuqes): create the texture view! it is being mapped below!
	VkImageViewCreateInfo textureImageViewCreateInfo = {};
	textureImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	textureImageViewCreateInfo.image = textureImage;
	textureImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	textureImageViewCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	textureImageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	textureImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	textureImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	textureImageViewCreateInfo.subresourceRange.levelCount = 1;
	textureImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	textureImageViewCreateInfo.subresourceRange.layerCount = 1;

	VkImageView textureView;
	result = vkCreateImageView( context.device, &textureImageViewCreateInfo, NULL, &textureView );
	checkVulkanResult( result, "Failed to create image view." );

	// TUTORIAL_2_002: 
	// Define and create our descriptors:
	VkDescriptorSetLayoutBinding bindings[2];
	
	// uniform buffer for our matrices:
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	bindings[0].pImmutableSamplers = NULL;

	// our example texture sampler:
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].pImmutableSamplers = NULL;

	VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {};
	setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCreateInfo.bindingCount = 2;
	setLayoutCreateInfo.pBindings = bindings;

	VkDescriptorSetLayout setLayout;
	result = vkCreateDescriptorSetLayout( context.device, &setLayoutCreateInfo, NULL, &setLayout );
	checkVulkanResult( result, "Failed to create DescriptorSetLayout." );

	// decriptor pool creation:
	VkDescriptorPoolSize uniformBufferPoolSize[2];
	uniformBufferPoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uniformBufferPoolSize[0].descriptorCount = 1;
	uniformBufferPoolSize[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uniformBufferPoolSize[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolCreateInfo = {}; 
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = 1;
	poolCreateInfo.poolSizeCount = 2;
	poolCreateInfo.pPoolSizes = uniformBufferPoolSize;
	
	VkDescriptorPool descriptorPool;
	result = vkCreateDescriptorPool( context.device, &poolCreateInfo, NULL, &descriptorPool );
	checkVulkanResult( result, "Failed to create descriptor pool." );

	// allocate our descriptor from the pool:
	VkDescriptorSetAllocateInfo descriptorAllocateInfo = {};
	descriptorAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorAllocateInfo.descriptorPool = descriptorPool;
	descriptorAllocateInfo.descriptorSetCount = 1;
	descriptorAllocateInfo.pSetLayouts = &setLayout;

	result = vkAllocateDescriptorSets( context.device, &descriptorAllocateInfo, &context.descriptorSet );
	checkVulkanResult( result, "Failed to allocate descriptor sets." );

	// When a set is allocated all values are undefined and all 
	// descriptors are uninitialized. must init all statically used bindings:
	VkDescriptorBufferInfo descriptorBufferInfo = {};
	descriptorBufferInfo.buffer = context.uniforms.buffer;
	descriptorBufferInfo.offset = 0;
	descriptorBufferInfo.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet writeDescriptor = {};
	writeDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptor.dstSet = context.descriptorSet;
	writeDescriptor.dstBinding = 0;
	writeDescriptor.dstArrayElement = 0;
	writeDescriptor.descriptorCount = 1;
	writeDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptor.pImageInfo = NULL;
	writeDescriptor.pBufferInfo = &descriptorBufferInfo;
	writeDescriptor.pTexelBufferView = NULL;

	vkUpdateDescriptorSets( context.device, 1, &writeDescriptor, 0, NULL );

	// update our combined sampler:
	VkDescriptorImageInfo descriptorImageInfo = {};
	descriptorImageInfo.sampler = sampler;
	descriptorImageInfo.imageView = textureView;
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	writeDescriptor.dstSet = context.descriptorSet;
	writeDescriptor.dstBinding = 1;
	writeDescriptor.dstArrayElement = 0;
	writeDescriptor.descriptorCount = 1;
	writeDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptor.pImageInfo = &descriptorImageInfo;
	writeDescriptor.pBufferInfo = NULL;
	writeDescriptor.pTexelBufferView = NULL;
	vkUpdateDescriptorSets( context.device, 1, &writeDescriptor, 0, NULL );


	// TUTORIAL_015 Graphics Pipeline:
	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = 1;
	layoutCreateInfo.pSetLayouts = &setLayout;
	layoutCreateInfo.pushConstantRangeCount = 0;
	layoutCreateInfo.pPushConstantRanges = NULL;

	result = vkCreatePipelineLayout( context.device, &layoutCreateInfo, NULL, &context.pipelineLayout );
	checkVulkanResult( result, "Failed to create pipeline layout." );

	// setup shader stages:
	VkPipelineShaderStageCreateInfo shaderStageCreateInfo[2] = {};
	shaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageCreateInfo[0].module = vertexShaderModule;
	shaderStageCreateInfo[0].pName = "main";		// shader entry point function name
	shaderStageCreateInfo[0].pSpecializationInfo = NULL;

	shaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageCreateInfo[1].module = fragmentShaderModule;
	shaderStageCreateInfo[1].pName = "main";		// shader entry point function name
	shaderStageCreateInfo[1].pSpecializationInfo = NULL;

	// vertex input configuration:
	VkVertexInputBindingDescription vertexBindingDescription = {};
	vertexBindingDescription.binding = 0;
	vertexBindingDescription.stride = sizeof(vertex);
	vertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertexAttributeDescritpion[3];
	
	// position:
	vertexAttributeDescritpion[0].location = 0;
	vertexAttributeDescritpion[0].binding = 0;
	vertexAttributeDescritpion[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexAttributeDescritpion[0].offset = 0;
	
	// normals:
	vertexAttributeDescritpion[1].location = 1;
	vertexAttributeDescritpion[1].binding = 0;
	vertexAttributeDescritpion[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttributeDescritpion[1].offset = 4 * sizeof(float);

	// texture coordinates:
	vertexAttributeDescritpion[2].location = 2;
	vertexAttributeDescritpion[2].binding = 0;
	vertexAttributeDescritpion[2].format = VK_FORMAT_R32G32_SFLOAT;
	vertexAttributeDescritpion[2].offset = (4 + 3) * sizeof(float);

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;  // attribute indexing is a function of the vertex index
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 3;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescritpion;

	// vertex topology config:
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	// viewport config:
	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = context.width;
	viewport.height = context.height;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;

	VkRect2D scissors = {};
	scissors.offset = { 0, 0 };
	scissors.extent = { context.width, context.height };

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissors;

	// rasterization config:
	VkPipelineRasterizationStateCreateInfo rasterizationState = {};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.cullMode = VK_CULL_MODE_NONE;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthBiasEnable = VK_FALSE;
	rasterizationState.depthBiasConstantFactor = 0;
	rasterizationState.depthBiasClamp = 0;
	rasterizationState.depthBiasSlopeFactor = 0;
	rasterizationState.lineWidth = 1;

	// sampling config:
	VkPipelineMultisampleStateCreateInfo multisampleState = {};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.sampleShadingEnable = VK_FALSE;
	multisampleState.minSampleShading = 0;
	multisampleState.pSampleMask = NULL;
	multisampleState.alphaToCoverageEnable = VK_FALSE;
	multisampleState.alphaToOneEnable = VK_FALSE;

	// depth/stencil config:
	VkStencilOpState noOPStencilState = {};
	noOPStencilState.failOp = VK_STENCIL_OP_KEEP;
	noOPStencilState.passOp = VK_STENCIL_OP_KEEP;
	noOPStencilState.depthFailOp = VK_STENCIL_OP_KEEP;
	noOPStencilState.compareOp = VK_COMPARE_OP_ALWAYS;
	noOPStencilState.compareMask = 0;
	noOPStencilState.writeMask = 0;
	noOPStencilState.reference = 0;
	
	VkPipelineDepthStencilStateCreateInfo depthState = {};
	depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthState.depthTestEnable = VK_TRUE;
	depthState.depthWriteEnable = VK_TRUE;
	depthState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthState.depthBoundsTestEnable = VK_FALSE;
	depthState.stencilTestEnable = VK_FALSE;
	depthState.front = noOPStencilState;
	depthState.back = noOPStencilState;
	depthState.minDepthBounds = 0;
	depthState.maxDepthBounds = 0;

	// color blend config: (Actually off for tutorial)
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = 0xf;

	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorBlendAttachmentState;
	colorBlendState.blendConstants[0] = 0.0;
	colorBlendState.blendConstants[1] = 0.0;
	colorBlendState.blendConstants[2] = 0.0;
	colorBlendState.blendConstants[3] = 0.0;

	// configure dynamic state:
	VkDynamicState dynamicState[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = 2;
	dynamicStateCreateInfo.pDynamicStates = dynamicState;

	// and finally, pipeline config and creation:
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStageCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pTessellationState = NULL;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pDepthStencilState = &depthState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.layout = context.pipelineLayout;
	pipelineCreateInfo.renderPass = context.renderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = NULL;
	pipelineCreateInfo.basePipelineIndex = 0;

	result = vkCreateGraphicsPipelines( context.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &context.pipeline );
	checkVulkanResult( result, "Failed to create graphics pipeline." );

}