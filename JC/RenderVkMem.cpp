#include "JC/RenderVk.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Mem.h"
#include <atomic>

namespace JC {

//--------------------------------------------------------------------------------------------------

Res<> RenderMem_CreateBuffer(
    VmaAllocator allocator,
    const VkBufferCreateInfo* pBufferCreateInfo,
    const VmaAllocationCreateInfo* pAllocationCreateInfo,
    VkBuffer* pBuffer,
    VmaAllocation* pAllocation,
    VmaAllocationInfo* pAllocationInfo
) {
    if((pBufferCreateInfo->usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_COPY) != 0 && !allocator->m_UseKhrBufferDeviceAddress)
    {
        VMA_ASSERT(0 && "Creating a buffer with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT is not valid if VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT was not used.");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *pBuffer = VK_NULL_HANDLE;
    *pAllocation = VK_NULL_HANDLE;

    // 1. Create VkBuffer.
    VkResult res = (*allocator->GetVulkanFunctions().vkCreateBuffer)(
        allocator->m_hDevice,
        pBufferCreateInfo,
        allocator->GetAllocationCallbacks(),
        pBuffer);
    if(res >= 0)
    {
        // 2. vkGetBufferMemoryRequirements.
        VkMemoryRequirements vkMemReq = {};
        bool requiresDedicatedAllocation = false;
        bool prefersDedicatedAllocation  = false;
        allocator->GetBufferMemoryRequirements(*pBuffer, vkMemReq,
            requiresDedicatedAllocation, prefersDedicatedAllocation);

        // 3. Allocate memory using allocator.
        res = allocator->AllocateMemory(
            vkMemReq,
            requiresDedicatedAllocation,
            prefersDedicatedAllocation,
            *pBuffer, // dedicatedBuffer
            VK_NULL_HANDLE, // dedicatedImage
            VmaBufferImageUsage(*pBufferCreateInfo, allocator->m_UseKhrMaintenance5), // dedicatedBufferImageUsage
            *pAllocationCreateInfo,
            VMA_SUBALLOCATION_TYPE_BUFFER,
            1, // allocationCount
            pAllocation);

        if(res >= 0)
        {
            // 3. Bind buffer with memory.
            if((pAllocationCreateInfo->flags & VMA_ALLOCATION_CREATE_DONT_BIND_BIT) == 0)
            {
                res = allocator->BindBufferMemory(*pAllocation, 0, *pBuffer, VMA_NULL);
            }
            if(res >= 0)
            {
                // All steps succeeded.
                #if VMA_STATS_STRING_ENABLED
                    (*pAllocation)->InitBufferUsage(*pBufferCreateInfo, allocator->m_UseKhrMaintenance5);
                #endif
                if(pAllocationInfo != VMA_NULL)
                {
                    allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
                }

                return VK_SUCCESS;
            }
            allocator->FreeMemory(
                1, // allocationCount
                pAllocation);
            *pAllocation = VK_NULL_HANDLE;
            (*allocator->GetVulkanFunctions().vkDestroyBuffer)(allocator->m_hDevice, *pBuffer, allocator->GetAllocationCallbacks());
            *pBuffer = VK_NULL_HANDLE;
            return res;
        }
        (*allocator->GetVulkanFunctions().vkDestroyBuffer)(allocator->m_hDevice, *pBuffer, allocator->GetAllocationCallbacks());
        *pBuffer = VK_NULL_HANDLE;
        return res;
    }
    return res;
}


//--------------------------------------------------------------------------------------------------

}	// namespace JC