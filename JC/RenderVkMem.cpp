#include "JC/RenderVk.h"

#include "JC/Array.h"
#include "JC/Bit.h"
#include "JC/Mem.h"
#include <atomic>

namespace JC {

//--------------------------------------------------------------------------------------------------

struct MemBlockVector {
	u32 memoryType;
	VkDeviceSize preferredBlockSize;
	VkDeviceSize bufferImageGranularity;
	VkDeviceSize m_MinAllocationAlignment;
	Array<MemBlock*> blockArray;

	~MemBlockVector() {
		for (u64 i = blockArray.len; i--; ) {
			blockArray[i]->Destroy();
			delete blockArray[i];
		}
	}

	VkResult Allocate(
		VkDeviceSize size,
		VkDeviceSize alignment,
		const VmaAllocationCreateInfo& createInfo,
		SuballocationType suballocType,
		u64 allocationCount,
		Allocation** pAllocations
	)  {
		u64 allocIndex;
		VkResult res = VK_SUCCESS;

		alignment = Max(alignment, m_MinAllocationAlignment);
		for (allocIndex = 0; allocIndex < allocationCount; ++allocIndex) {
			res = AllocatePage(
				size,
				alignment,
				createInfo,
				suballocType,
				pAllocations + allocIndex);
			if (res != VK_SUCCESS) {
				break;
			}
		}

		if (res != VK_SUCCESS) {
			// Free all already created allocations.
			while (allocIndex--) {
				Free(pAllocations[allocIndex]);
			}
			memset(pAllocations, 0, sizeof(Allocation*) * allocationCount);
		}

		return res;
	}


	void Free(const Allocation* hAllocation) {
		MemBlock* pBlockToDelete = 0;

		bool budgetExceeded = false;
		const u32 heapIndex = MemoryTypeToHeapIndex(memoryType);
		VmaBudget heapBudget = {};
		GetHeapBudgets(&heapBudget, heapIndex, 1);
		budgetExceeded = heapBudget.usage >= heapBudget.budget;

		MemBlock* pBlock = hAllocation->blockAllocation.block;

		if (hAllocation->flags & AllocationFlags::PersistentMap) {
			pBlock->Unmap(1);
		}

		const bool hadEmptyBlockBeforeFree = HasEmptyBlock();
		pBlock->meta->Free(hAllocation->blockAllocation.allocHandle);
		pBlock->PostFree();

		// pBlock became empty after this deallocation.
		if (pBlock->meta->IsEmpty()) {
			// Already had empty block. We don't want to have two, so delete this one.
			if (hadEmptyBlockBeforeFree || budgetExceeded) {
				pBlockToDelete = pBlock;
				Remove(pBlock);
			}
			// else: We now have one empty block - leave it. A hysteresis to avoid allocating whole block back and forth.

		// pBlock didn't become empty, but we have another empty block - find and free that one.
		// (This is optional, heuristics.)
		} else if (hadEmptyBlockBeforeFree) {
			MemBlock* pLastBlock = blockArray.back();
			if (pLastBlock->meta->IsEmpty())
			{
				pBlockToDelete = pLastBlock;
				blockArray.pop_back();
			}
		}

		IncrementallySortBlocks();

		budget.RemoveAllocation(MemoryTypeToHeapIndex(memoryType), hAllocation->size);
		hAllocation->Destroy();
		mallocationObjectAllocator.Free(hAllocation);

		// Destruction of a free block. Deferred until this point, outside of mutex
		// lock, for performance reason.
		if (pBlockToDelete != 0) {
			pBlockToDelete->Destroy();
			vma_delete(pBlockToDelete);
		}
	}



	VkDeviceSize CalcMaxBlockSize() const {
		VkDeviceSize result = 0;
		for (u64 i = blockArray.size(); i--; ) {
			result = Max(result, blockArray[i]->meta->size);
			if (result >= preferredBlockSize) {
				break;
			}
		}
		return result;
	}

	// Finds and removes given block from vector.
	void Remove(MemBlock* pBlock) {
		for (u32 blockIndex = 0; blockIndex < blockArray.size(); ++blockIndex) {
			if (blockArray[blockIndex] == pBlock) {
				VmaVectorRemove(blockArray, blockIndex);
				return;
			}
		}
		Assert(0);
	}

	// Performs single step in sorting blockArray. They may not be fully sorted
	// after this call.
	void IncrementallySortBlocks() {
		// Bubble sort only until first swap.
		for (u64 i = 1; i < blockArray.size(); ++i) {
			if (blockArray[i - 1]->meta->GetSumFreeSize() > blockArray[i]->meta->GetSumFreeSize()) {
				std::swap(blockArray[i - 1], blockArray[i]);
				return;
			}
		}
	}

	void SortByFreeSize() {
		std::sort(
			blockArray.begin(),
			blockArray.end(),
			[](MemBlock* b1, MemBlock* b2) -> bool {
				return b1->meta->GetSumFreeSize() < b2->meta->GetSumFreeSize();
			}
		);
	}


	VkResult AllocatePage(
		VkDeviceSize size,
		VkDeviceSize alignment,
		const VmaAllocationCreateInfo& createInfo,
		SuballocationType suballocType,
		Allocation** pAllocation
	)  {
		VkDeviceSize freeMemory;
		const u32 heapIndex = MemoryTypeToHeapIndex(memoryType);
		VmaBudget heapBudget = {};
		GetHeapBudgets(&heapBudget, heapIndex, 1);
		freeMemory = (heapBudget.usage < heapBudget.budget) ? (heapBudget.budget - heapBudget.usage) : 0;

		const bool canFallbackToDedicated = (createInfo.flags & VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT) == 0;
		const bool canCreateNewBlock = ((createInfo.flags & VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT) == 0) && (freeMemory >= size || !canFallbackToDedicated);
		u32 strategy = createInfo.flags & VMA_ALLOCATION_CREATE_STRATEGY_MASK;

		// Early reject: requested allocation size is larger that maximum block size for this block vector.
		if (size + 0 > preferredBlockSize)
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}

		// 1. Search existing allocations. Try to allocate.
		if (strategy != VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT) // MIN_MEMORY or default
		{
			const bool isHostVisible =
				(vkPhysicalDeviceMemoryProperties.memoryTypes[memoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
			if(isHostVisible)
			{
				const bool isMappingAllowed = (createInfo.flags &
					(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0;
				/*
				For non-mappable allocations, check blocks that are not mapped first.
				For mappable allocations, check blocks that are already mapped first.
				This way, having many blocks, we will separate mappable and non-mappable allocations,
				hopefully limiting the number of blocks that are mapped, which will help tools like RenderDoc.
				*/
				for(u64 mappingI = 0; mappingI < 2; ++mappingI)
				{
					// Forward order in blockArray - prefer blocks with smallest amount of free space.
					for (u64 blockIndex = 0; blockIndex < blockArray.size(); ++blockIndex)
					{
						MemBlock* const pCurrBlock = blockArray[blockIndex];
						Assert(pCurrBlock);
						const bool isBlockMapped = pCurrBlock->mappedData != 0;
						if((mappingI == 0) == (isMappingAllowed == isBlockMapped))
						{
							VkResult res = AllocateFromBlock(
								pCurrBlock, size, alignment, createInfo.flags, suballocType, strategy, pAllocation);
							if (res == VK_SUCCESS)
							{
								IncrementallySortBlocks();
								return VK_SUCCESS;
							}
						}
					}
				}
			}
			else
			{
				// Forward order in blockArray - prefer blocks with smallest amount of free space.
				for (u64 blockIndex = 0; blockIndex < blockArray.size(); ++blockIndex)
				{
					MemBlock* const pCurrBlock = blockArray[blockIndex];
					Assert(pCurrBlock);
					VkResult res = AllocateFromBlock(pCurrBlock, size, alignment, createInfo.flags, suballocType, strategy, pAllocation);
					if (res == VK_SUCCESS)
					{
						IncrementallySortBlocks();
						return VK_SUCCESS;
					}
				}
			}
		}
		else // VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT
		{
			// Backward order in blockArray - prefer blocks with largest amount of free space.
			for (u64 blockIndex = blockArray.size(); blockIndex--; )
			{
				MemBlock* const pCurrBlock = blockArray[blockIndex];
				Assert(pCurrBlock);
				VkResult res = AllocateFromBlock(pCurrBlock, size, alignment, createInfo.flags, suballocType, strategy, pAllocation);
				if (res == VK_SUCCESS)
				{
					IncrementallySortBlocks();
					return VK_SUCCESS;
				}
			}
		}

		// 2. Try to create new block.
		if (canCreateNewBlock)
		{
			// Calculate optimal size for new block.
			VkDeviceSize newBlockSize = preferredBlockSize;
			u32 newBlockSizeShift = 0;
			const u32 NEW_BLOCK_SIZE_SHIFT_MAX = 3;
			// Allocate 1/8, 1/4, 1/2 as first blocks.
			const VkDeviceSize maxExistingBlockSize = CalcMaxBlockSize();
			for (u32 i = 0; i < NEW_BLOCK_SIZE_SHIFT_MAX; ++i)
			{
				const VkDeviceSize smallerNewBlockSize = newBlockSize / 2;
				if (smallerNewBlockSize > maxExistingBlockSize && smallerNewBlockSize >= size * 2)
				{
					newBlockSize = smallerNewBlockSize;
					++newBlockSizeShift;
				}
				else
				{
					break;
				}
			}

			u64 newBlockIndex = 0;
			VkResult res = (newBlockSize <= freeMemory || !canFallbackToDedicated) ?
				CreateBlock(newBlockSize, &newBlockIndex) : VK_ERROR_OUT_OF_DEVICE_MEMORY;
			// Allocation of this size failed? Try 1/2, 1/4, 1/8 of preferredBlockSize.
			while (res < 0 && newBlockSizeShift < NEW_BLOCK_SIZE_SHIFT_MAX)
			{
				const VkDeviceSize smallerNewBlockSize = newBlockSize / 2;
				if (smallerNewBlockSize >= size)
				{
					newBlockSize = smallerNewBlockSize;
					++newBlockSizeShift;
					res = (newBlockSize <= freeMemory || !canFallbackToDedicated) ?
						CreateBlock(newBlockSize, &newBlockIndex) : VK_ERROR_OUT_OF_DEVICE_MEMORY;
				}
				else
				{
					break;
				}
			}

			if (res == VK_SUCCESS)
			{
				MemBlock* const pBlock = blockArray[newBlockIndex];
				Assert(pBlock->meta->size >= size);

				res = AllocateFromBlock(
					pBlock, size, alignment, createInfo.flags, suballocType, strategy, pAllocation);
				if (res == VK_SUCCESS)
				{
					IncrementallySortBlocks();
					return VK_SUCCESS;
				}
				else
				{
					// Allocation from new block failed, possibly due to 0 or alignment.
					return VK_ERROR_OUT_OF_DEVICE_MEMORY;
				}
			}
		}

		return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	}


	VkResult AllocateFromBlock(
		MemBlock* pBlock,
		VkDeviceSize size,
		VkDeviceSize alignment,
		VmaAllocationCreateFlags allocFlags,
		SuballocationType suballocType,
		u32 strategy,
		Allocation** pAllocation
	) {
		VmaAllocationRequest currRequest = {};
		if (pBlock->meta->CreateAllocationRequest(
			size,
			alignment,
			suballocType,
			strategy,
			&currRequest)
		) {
			return CommitAllocationRequest(currRequest, pBlock, alignment, allocFlags, suballocType, pAllocation);
		}
		return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	}

	VkResult CommitAllocationRequest(
		VmaAllocationRequest& allocRequest,
		MemBlock* pBlock,
		VkDeviceSize alignment,
		VmaAllocationCreateFlags allocFlags,
		SuballocationType suballocType,
		Allocation** pAllocation
	) {
		const bool mapped = (allocFlags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0;
		const bool isMappingAllowed = (allocFlags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0;

		pBlock->PostAlloc();
		// Allocate from pCurrBlock.
		if (mapped) {
			VkResult res = pBlock->Map(1, 0);
			if (res != VK_SUCCESS) {
				return res;
			}
		}

		*pAllocation = new Allocation(isMappingAllowed);
		pBlock->meta->Alloc(allocRequest, suballocType, *pAllocation);
		(*pAllocation)->InitBlockAllocation(
			pBlock,
			allocRequest.allocHandle,
			alignment,
			allocRequest.size, // Not size, as actual allocation size may be larger than requested!
			memoryType,
			suballocType,
			mapped
		);
		budget.AddAllocation(MemoryTypeToHeapIndex(memoryType), allocRequest.size);
		return VK_SUCCESS;
	}


	VkResult CreateBlock(VkDeviceSize blockSize, u64* pNewBlockIndex) {
		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.pNext = 0;
		allocInfo.memoryType = memoryType;
		allocInfo.allocationSize = blockSize;

		// Every standalone block can potentially contain a buffer with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT - always enable the feature.
		VkMemoryAllocateFlagsInfoKHR allocFlagsInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR };
		VkMemoryPriorityAllocateInfoEXT priorityInfo = { VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };

		VkDeviceMemory mem = VK_NULL_HANDLE;
		VkResult res = AllocateVulkanMemory(&allocInfo, &mem);
		if (res < 0) {
			return res;
		}

		// New VkDeviceMemory successfully created.

		// Create new Allocation for it.
		MemBlock* const pBlock = vma_new(MemBlock)();
		pBlock->Init(
			memoryType,
			mem,
			allocInfo.allocationSize,
			bufferImageGranularity);

		blockArray.push_back(pBlock);
		if (pNewBlockIndex != 0) {
			*pNewBlockIndex = blockArray.size() - 1;
		}

		return VK_SUCCESS;
	}

	bool HasEmptyBlock() {
		for (u64 index = 0, count = blockArray.size(); index < count; ++index) {
			MemBlock* const pBlock = blockArray[index];
			if (pBlock->meta->IsEmpty()) {
				return true;
			}
		}
		return false;
	}
};

//--------------------------------------------------------------------------------------------------

struct VmaCurrentBudgetData {
	std::atomic<u32> m_BlockCount[VK_MAX_MEMORY_HEAPS];
	std::atomic<u32> m_AllocationCount[VK_MAX_MEMORY_HEAPS];
	std::atomic<u64> m_BlockBytes[VK_MAX_MEMORY_HEAPS];
	std::atomic<u64> m_AllocationBytes[VK_MAX_MEMORY_HEAPS];
	std::atomic<u32> m_OperationsSinceBudgetFetch;
	u64 m_VulkanUsage[VK_MAX_MEMORY_HEAPS];
	u64 m_VulkanBudget[VK_MAX_MEMORY_HEAPS];
	u64 m_BlockBytesAtBudgetFetch[VK_MAX_MEMORY_HEAPS];

	VmaCurrentBudgetData() {
		for (u32 heapIndex = 0; heapIndex < VK_MAX_MEMORY_HEAPS; ++heapIndex)
		{
			m_BlockCount[heapIndex] = 0;
			m_AllocationCount[heapIndex] = 0;
			m_BlockBytes[heapIndex] = 0;
			m_AllocationBytes[heapIndex] = 0;
			m_VulkanUsage[heapIndex] = 0;
			m_VulkanBudget[heapIndex] = 0;
			m_BlockBytesAtBudgetFetch[heapIndex] = 0;
		}
		m_OperationsSinceBudgetFetch = 0;
	}

	void AddAllocation(u32 heapIndex, VkDeviceSize allocationSize) {
		m_AllocationBytes[heapIndex] += allocationSize;
		++m_AllocationCount[heapIndex];
	#if 1
		++m_OperationsSinceBudgetFetch;
	#endif
	}

	void RemoveAllocation(u32 heapIndex, VkDeviceSize allocationSize) {
		Assert(m_AllocationBytes[heapIndex] >= allocationSize);
		m_AllocationBytes[heapIndex] -= allocationSize;
		Assert(m_AllocationCount[heapIndex] > 0);
		--m_AllocationCount[heapIndex];
	#if 1
		++m_OperationsSinceBudgetFetch;
	#endif
	}

};

//--------------------------------------------------------------------------------------------------

static constexpr u64 SmallHeapMaxSize   = (u64)1024 * 1024 * 1024;
static constexpr u64 LargeHeapBlockSize = (u64)256 * 1024 * 1024;

static VkAllocationCallbacks*           vkAllocationCallbacks = 0;
static VkPhysicalDevice                 vkPhysicalDevice;
static VkDevice                         vkDevice;
static VkInstance                       vkInstance;
static VkPhysicalDeviceProperties       vkPhysicalDeviceProperties;
static VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;
static MemBlockVector*                  blockVectors[VK_MAX_MEMORY_TYPES];
static VmaDedicatedAllocationList       dedicatedAllocations[VK_MAX_MEMORY_TYPES];
static VmaCurrentBudgetData             budget;
static u32                              deviceMemoryCount; // Total number of VkDeviceMemory objects.
static u32                              frame;
static u32                              allowedMemoryTypes;

static u32 MemoryTypeToHeapIndex(u32 memoryType) {
	return vkPhysicalDeviceMemoryProperties.memoryTypes[memoryType].heapIndex;
}

static bool IsMemoryTypeNonCoherent(u32 memoryType) {
	return (vkPhysicalDeviceMemoryProperties.memoryTypes[memoryType].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
}

static VkDeviceSize GetMemoryTypeMinAlignment(u32 memoryType) {
	return IsMemoryTypeNonCoherent(memoryType)
		? Max((VkDeviceSize)1, vkPhysicalDeviceProperties.limits.nonCoherentAtomSize)
		: (VkDeviceSize)1;
}

static void UpdateVulkanBudget() {
	VkPhysicalDeviceMemoryProperties2KHR memProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR };
	VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
	memProps.pNext = &budgetProps;

	vkGetPhysicalDeviceMemoryProperties2(vkPhysicalDevice, &memProps);

	for(u32 heapIndex = 0; heapIndex < vkPhysicalDeviceMemoryProperties.memoryHeapCount; ++heapIndex) {
		budget.m_VulkanUsage[heapIndex] = budgetProps.heapUsage[heapIndex];
		budget.m_VulkanBudget[heapIndex] = budgetProps.heapBudget[heapIndex];
		budget.m_BlockBytesAtBudgetFetch[heapIndex] = budget.m_BlockBytes[heapIndex].load();
		// Some bugged drivers return the budget incorrectly, e.g. 0 or much bigger than heap size.
		if(budget.m_VulkanBudget[heapIndex] == 0) {
			budget.m_VulkanBudget[heapIndex] = vkPhysicalDeviceMemoryProperties.memoryHeaps[heapIndex].size * 8 / 10; // 80% heuristics.
		} else if(budget.m_VulkanBudget[heapIndex] > vkPhysicalDeviceMemoryProperties.memoryHeaps[heapIndex].size) {
			budget.m_VulkanBudget[heapIndex] = vkPhysicalDeviceMemoryProperties.memoryHeaps[heapIndex].size;
		}
		if(budget.m_VulkanUsage[heapIndex] == 0 && budget.m_BlockBytesAtBudgetFetch[heapIndex] > 0) {
			budget.m_VulkanUsage[heapIndex] = budget.m_BlockBytesAtBudgetFetch[heapIndex];
		}
	}
	budget.m_OperationsSinceBudgetFetch = 0;
}

void RenderMem_Init() {
	allowedMemoryTypes = UINT32_MAX;
	for(u32 i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i) {
		if(vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) {
			allowedMemoryTypes &= ~(1u << i);
		}
	}

	const VkDeviceSize bufferImageGranularity = Max((VkDeviceSize)1, vkPhysicalDeviceProperties.limits.bufferImageGranularity);

	for(u32 i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i) {
		if((allowedMemoryTypes & (1u << i)) != 0) {
			const VkDeviceSize heapSize = vkPhysicalDeviceMemoryProperties.memoryHeaps[MemoryTypeToHeapIndex(i)].size;
			const bool isSmallHeap = heapSize <= SmallHeapMaxSize;

			blockVectors[i] = new MemBlockVector();
			blockVectors[i]->memoryType = i;
			blockVectors[i]->preferredBlockSize = AlignUp(isSmallHeap ? (heapSize / 8) : LargeHeapBlockSize, (VkDeviceSize)32);
			blockVectors[i]->bufferImageGranularity = bufferImageGranularity;
			blockVectors[i]->m_MinAllocationAlignment = GetMemoryTypeMinAlignment(i); // minAllocationAlignment
		}
	}

	UpdateVulkanBudget();
}

//--------------------------------------------------------------------------------------------------

void RenderMem_Shutdown() {
	for(u64 i = vkPhysicalDeviceMemoryProperties.memoryTypeCount; i--; ) {
		delete blockVectors[i];
	}
}

//--------------------------------------------------------------------------------------------------

void RenderMem_SetFrame(u32 inFrame) {
	frame = inFrame;
}

//--------------------------------------------------------------------------------------------------

struct AllocationInfo {
	u32            memoryType;
	VkDeviceMemory vkDeviceMemory;
	VkDeviceSize   offset;
	VkDeviceSize   size;
	void*          mappedData;
};

struct RenderMem_CreateBufferInfo {
	VkBuffer       vkBuffer;
	Allocation     allocation;
	AllocationInfo allocationInfo;
};

typedef struct VmaAllocationCreateInfo
{
	VmaAllocationCreateFlags flags;
	VmaMemoryUsage usage;
	VkMemoryPropertyFlags requiredFlags;
	VkMemoryPropertyFlags preferredFlags;
	u32 memoryTypeBits;
	float priority;
} VmaAllocationCreateInfo;


//--------------------------------------------------------------------------------------------------

struct BufferMemoryRequirements {
	VkMemoryRequirements  vkMemoryRequirements        = {};
	bool                  requiresDedicatedAllocation = false;
	bool                  prefersDedicatedAllocation  = false;
};

BufferMemoryRequirements GetBufferMemoryRequirements(VkBuffer vkBuffer) {
	VkBufferMemoryRequirementsInfo2 vkMemoryRequirementsInfo2 = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR };
	vkMemoryRequirementsInfo2.buffer = vkBuffer;
	VkMemoryDedicatedRequirements vkMemoryDedicatedRequirements = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };
	vkMemoryRequirementsInfo2.pNext = &vkMemoryDedicatedRequirements;
	VkMemoryRequirements2 vkMemoryRequirements2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
	vkGetBufferMemoryRequirements2(vkDevice, &vkMemoryRequirementsInfo2, &vkMemoryRequirements2);
	return {
		.vkMemoryRequirements = vkMemoryRequirements2.memoryRequirements,
		.requiresDedicatedAllocation = (vkMemoryDedicatedRequirements.requiresDedicatedAllocation != VK_FALSE),
		.prefersDedicatedAllocation  = (vkMemoryDedicatedRequirements.prefersDedicatedAllocation  != VK_FALSE),
	};
}

Res<VkBuffer> RenderMem_CreateBuffer(
	VkBufferCreateInfo* vkBufferCreateInfo
) {
	VkBuffer vkBuffer = VK_NULL_HANDLE;
	CheckVk(vkCreateBuffer(vkDevice, vkBufferCreateInfo, vkAllocationCallbacks, &vkBuffer))

	BufferMemoryRequirements bufferMemoryRequirements = GetBufferMemoryRequirements(vkBuffer);

	AllocateMemory(&bufferMemoryRequirements, vkBuffer, 0, 
	// HERE: 
	// currently deep inside VmaCreateBuffer()
	// next is AllocateMemory and the core flow
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

}	// namespace JC