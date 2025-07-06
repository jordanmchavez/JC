#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cinttypes>
#include <utility>
#include <type_traits>
#include <intrin.h> // For functions like __popcnt, _BitScanForward etc.
#include <bit>
#include <algorithm> // for min, max, swap

#pragma warning (disable: 4324)

template<typename T> static T* VmaAllocate() { return (T*)_aligned_malloc(sizeof(T), alignof(T)); }
template<typename T> static T* VmaAllocateArray(size_t count) { return (T*)_aligned_malloc(sizeof(T) * count, alignof(T)); }
#define                          vma_new(type)              new(VmaAllocate<type>())(type)
#define                          vma_new_array(type, count) new(VmaAllocateArray<type>((count)))(type)
template<typename T> static void vma_delete(T* ptr) { ptr->~T(); _aligned_free(ptr); }
template<typename T> static void vma_delete_array(T* ptr, size_t count)
{
	if (ptr != nullptr)
	{
		for (size_t i = count; i--; )
		{
			ptr[i].~T();
		}
		_aligned_free(ptr);
	}
}

VK_DEFINE_NON_DISPATCHABLE_HANDLE(VmaAllocHandle);

enum VmaMemoryUsage {
	VMA_MEMORY_USAGE_AUTO,
	VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
	VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
};

enum VmaAllocationCreateFlagBits {
	VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT = 0x00000001,
	VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT = 0x00000100,
	VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT = 0x00000400,
	VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT = 0x00000800,
	VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT = 0x00001000,
};
typedef VkFlags VmaAllocationCreateFlags;

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

struct VmaVulkanFunctions {
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
	PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
	PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
	PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
	PFN_vkAllocateMemory vkAllocateMemory;
	PFN_vkFreeMemory vkFreeMemory;
	PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;
};

struct VmaAllocatorCreateInfo {
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
};

struct VmaBudget {
	VkDeviceSize usage;
	VkDeviceSize budget;
};

struct VmaAllocationCreateInfo {
	VmaAllocationCreateFlags flags;
	VmaMemoryUsage usage;
	VkMemoryPropertyFlags requiredFlags;
	VkMemoryPropertyFlags preferredFlags;
};

struct VmaAllocationInfo {
	uint32_t memoryType;
	VkDeviceMemory deviceMemory;
	VkDeviceSize offset;
	VkDeviceSize size;
};

enum ALLOCATION_TYPE {
	ALLOCATION_TYPE_NONE,
	ALLOCATION_TYPE_BLOCK,
	ALLOCATION_TYPE_DEDICATED,
};

enum VmaSuballocationType {
	VMA_SUBALLOCATION_TYPE_FREE,
	VMA_SUBALLOCATION_TYPE_UNKNOWN,
	VMA_SUBALLOCATION_TYPE_BUFFER,
	VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN,
	VMA_SUBALLOCATION_TYPE_IMAGE_LINEAR,
	VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL,
};

struct VmaSuballocation {
	VkDeviceSize         offset;
	VkDeviceSize         size;
	void*                userData;
	VmaSuballocationType type;
};

struct VmaAllocationRequest
{
	VkDeviceSize size;
	VkDeviceSize offset;
};

static const uint32_t VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY = 0x00000040;
static const uint32_t VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY = 0x00000080;

template <typename T> static inline T VmaAlignUp(T val, T alignment) { return (val + alignment - 1) & ~(alignment - 1); }
template <typename T> static inline T VmaAlignDown(T val, T alignment) { return val & ~(alignment - 1); }

static inline bool VmaIsBufferImageGranularityConflict(VmaSuballocationType suballocType1, VmaSuballocationType suballocType2) {
	if (suballocType1 > suballocType2)
	{
		std::swap(suballocType1, suballocType2);
	}

	switch (suballocType1)
	{
	case VMA_SUBALLOCATION_TYPE_FREE:
		return false;
	case VMA_SUBALLOCATION_TYPE_UNKNOWN:
		return true;
	case VMA_SUBALLOCATION_TYPE_BUFFER:
		return
			suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN ||
			suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;
	case VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN:
		return
			suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN ||
			suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_LINEAR ||
			suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;
	case VMA_SUBALLOCATION_TYPE_IMAGE_LINEAR:
		return
			suballocType2 == VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL;
	case VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL:
		return false;
	default:
		return true;
	}
}

template<typename MainT, typename NewT>
static inline void VmaPnextChainPushFront(MainT* mainStruct, NewT* newStruct)
{
	newStruct->pNext = mainStruct->pNext;
	mainStruct->pNext = newStruct;
}
// Finds structure with s->sType == sType in mainStruct->pNext chain.
// Returns pointer to it. If not found, returns null.
template<typename FindT, typename MainT>
static inline const FindT* VmaPnextChainFind(const MainT* mainStruct, VkStructureType sType)
{
	for(const VkBaseInStructure* s = (const VkBaseInStructure*)mainStruct->pNext;
		s != nullptr; s = s->pNext)
	{
		if(s->sType == sType)
		{
			return (const FindT*)s;
		}
	}
	return nullptr;
}

struct VmaDeviceMemoryBlock {
	VkDeviceSize                  m_Size = 0;
	VkDeviceSize                  m_BufferImageGranularity;
	std::vector<VmaSuballocation> m_Suballocations;
	VkDeviceMemory                m_hMemory = VK_NULL_HANDLE;

	void Init(
		VkDeviceMemory newMemory,
		VkDeviceSize newSize,
		VkDeviceSize bufferImageGranularity
	) {
		m_hMemory = newMemory;
		m_BufferImageGranularity = bufferImageGranularity;
		m_Size = newSize;
	}
};

struct VmaAllocation_T {
	struct BlockAllocation {
		VmaDeviceMemoryBlock* m_Block;
	};

	struct DedicatedAllocation{
		VkDeviceMemory m_hMemory;
		VmaAllocation_T* m_Prev;
		VmaAllocation_T* m_Next;
	};

	union {
		BlockAllocation m_BlockAllocation;
		DedicatedAllocation m_DedicatedAllocation;
	};

	VkDeviceSize m_Alignment         = 1;
	VkDeviceSize m_Size              = 0;
	uint32_t     m_MemoryTypeIndex   = 0;
	uint8_t      m_Type              = ALLOCATION_TYPE_NONE;
	uint8_t      m_SuballocationType = VMA_SUBALLOCATION_TYPE_UNKNOWN;

	void InitDedicatedAllocation(uint32_t memoryTypeIndex, VkDeviceMemory hMemory, VmaSuballocationType suballocationType, VkDeviceSize size) {
		m_Type = (uint8_t)ALLOCATION_TYPE_DEDICATED;
		m_Alignment = 0;
		m_Size = size;
		m_MemoryTypeIndex = memoryTypeIndex;
		m_SuballocationType = (uint8_t)suballocationType;
		m_DedicatedAllocation.m_hMemory = hMemory;
		m_DedicatedAllocation.m_Prev = nullptr;
		m_DedicatedAllocation.m_Next = nullptr;
	}
};

struct VmaBlockVector {
	VmaAllocator                       m_hAllocator;
	uint32_t                           m_MemoryTypeIndex;
	VkDeviceSize                       m_PreferredBlockSize;
	VkDeviceSize                       m_BufferImageGranularity;
	std::vector<VmaDeviceMemoryBlock*> m_Blocks;

	VmaBlockVector(
		VmaAllocator hAllocator,
		uint32_t memoryTypeIndex,
		VkDeviceSize preferredBlockSize,
		VkDeviceSize bufferImageGranularity
	) :
		m_hAllocator(hAllocator),
		m_MemoryTypeIndex(memoryTypeIndex),
		m_PreferredBlockSize(preferredBlockSize),
		m_BufferImageGranularity(bufferImageGranularity)
	{}

	VkResult AllocatePage(VkDeviceSize size, VkDeviceSize alignment, VmaSuballocationType suballocType, VmaAllocation* pAllocation);

	VkResult AllocateFromBlock(
		VmaDeviceMemoryBlock* pBlock,
		VkDeviceSize size,
		VkDeviceSize alignment,
		VmaSuballocationType suballocType,
		VmaAllocation* pAllocation
	);

	VkResult CreateBlock(VkDeviceSize blockSize, size_t* pNewBlockIndex);
};

struct VmaCurrentBudgetData {
	uint64_t m_BlockBytes[VK_MAX_MEMORY_HEAPS];
	uint32_t m_OperationsSinceBudgetFetch;
	uint64_t m_VulkanUsage[VK_MAX_MEMORY_HEAPS];
	uint64_t m_VulkanBudget[VK_MAX_MEMORY_HEAPS];
	uint64_t m_BlockBytesAtBudgetFetch[VK_MAX_MEMORY_HEAPS];

	VmaCurrentBudgetData() {
		for (uint32_t heapIndex = 0; heapIndex < VK_MAX_MEMORY_HEAPS; ++heapIndex)
		{
			m_BlockBytes[heapIndex] = 0;
			m_VulkanUsage[heapIndex] = 0;
			m_VulkanBudget[heapIndex] = 0;
			m_BlockBytesAtBudgetFetch[heapIndex] = 0;
		}
		m_OperationsSinceBudgetFetch = 0;
	}
};

struct VmaAllocator_T {
	VkDevice                         m_hDevice;
	VkInstance                       m_hInstance;
	VkPhysicalDeviceProperties       m_PhysicalDeviceProperties;
	VkPhysicalDeviceMemoryProperties m_MemProps;
	VmaBlockVector*                  m_pBlockVectors[VK_MAX_MEMORY_TYPES];
	VmaAllocation_T*                 m_DedicatedAllocations[VK_MAX_MEMORY_TYPES];
	VmaCurrentBudgetData             m_Budget;
	uint32_t                         m_DeviceMemoryCount;
	VkPhysicalDevice                 m_PhysicalDevice;
	uint32_t                         m_CurrentFrameIndex;
	VmaVulkanFunctions               m_VulkanFunctions;

	explicit VmaAllocator_T() {
		m_DeviceMemoryCount = 0;
		(*m_VulkanFunctions.vkGetPhysicalDeviceProperties)(m_PhysicalDevice, &m_PhysicalDeviceProperties);
		(*m_VulkanFunctions.vkGetPhysicalDeviceMemoryProperties)(m_PhysicalDevice, &m_MemProps);
		for(uint32_t memTypeIndex = 0; memTypeIndex < m_MemProps.memoryTypeCount; ++memTypeIndex) {
			const uint32_t heapIndex = m_MemProps.memoryTypes[memTypeIndex].heapIndex;
			const VkDeviceSize heapSize = m_MemProps.memoryHeaps[heapIndex].size;
			#define VMA_SMALL_HEAP_MAX_SIZE (1024ULL * 1024 * 1024)
			const VkDeviceSize preferredBlockSize = VmaAlignUp((heapSize <= VMA_SMALL_HEAP_MAX_SIZE)? (heapSize / 8) : 256ULL * 1024 * 1024, 32ull);
			m_pBlockVectors[memTypeIndex] = vma_new(VmaBlockVector)(
				this,
				memTypeIndex,
				preferredBlockSize,
				m_PhysicalDeviceProperties.limits.bufferImageGranularity
			);
		}
		UpdateVulkanBudget();
	}

	void GetHeapBudgets(VmaBudget* outBudgets, uint32_t firstHeap, uint32_t heapCount) {
		if(m_Budget.m_OperationsSinceBudgetFetch < 30) {
			for(uint32_t i = 0; i < heapCount; ++i, ++outBudgets) {
				const uint32_t heapIndex = firstHeap + i;
				if(m_Budget.m_VulkanUsage[heapIndex] + m_Budget.m_BlockBytes[heapIndex] > m_Budget.m_BlockBytesAtBudgetFetch[heapIndex]) {
					outBudgets->usage = m_Budget.m_VulkanUsage[heapIndex] + m_Budget.m_BlockBytes[heapIndex] - m_Budget.m_BlockBytesAtBudgetFetch[heapIndex];
				} else {
					outBudgets->usage = 0;
				}
				// Have to take MIN with heap size because explicit HeapSizeLimit is included in it.
				outBudgets->budget = std::min(m_Budget.m_VulkanBudget[heapIndex], m_MemProps.memoryHeaps[heapIndex].size);
			}
		} else {
			UpdateVulkanBudget();
			GetHeapBudgets(outBudgets, firstHeap, heapCount);
		}
	}

	// This is the main algorithm that guides the selection of a memory type best for an allocation -
	// converts usage to required/preferred/not preferred flags.
	void FindMemoryPreferences(
		const VmaAllocationCreateInfo& allocCreateInfo,
		uint64_t bufferImageUsage,
		VkMemoryPropertyFlags& outRequiredFlags,
		VkMemoryPropertyFlags& outPreferredFlags,
		VkMemoryPropertyFlags& outNotPreferredFlags
	) const {
		const bool isIntegratedGPU = m_PhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

		outRequiredFlags = allocCreateInfo.requiredFlags;
		outPreferredFlags = allocCreateInfo.preferredFlags;
		outNotPreferredFlags = 0;

		// This relies on values of VK_IMAGE_USAGE_TRANSFER* being the same as VK_BUFFER_IMAGE_TRANSFER*.
		const bool deviceAccess                   = (bufferImageUsage & ~(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
		const bool hostAccessSequentialWrite      = (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) != 0;
		const bool hostAccessRandom               = (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT) != 0;
		const bool hostAccessAllowTransferInstead = (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT) != 0;
		const bool preferDevice                   = allocCreateInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		const bool preferHost                     = allocCreateInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

		// CPU random access - e.g. a buffer written to or transferred from GPU to read back on CPU.
		if(hostAccessRandom) {
			// Prefer cached. Cannot require it, because some platforms don't have it (e.g. Raspberry Pi - see #362)!
			outPreferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
			if (!isIntegratedGPU && deviceAccess && hostAccessAllowTransferInstead && !preferHost) {
				// Nice if it will end up in HOST_VISIBLE, but more importantly prefer DEVICE_LOCAL.
				// Omitting HOST_VISIBLE here is intentional.
				// In case there is DEVICE_LOCAL | HOST_VISIBLE | HOST_CACHED, it will pick that one.
				// Otherwise, this will give same weight to DEVICE_LOCAL as HOST_VISIBLE | HOST_CACHED and select the former if occurs first on the list.
				outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			} else {
				// Always CPU memory.
				outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			}
		// CPU sequential write - may be CPU or host-visible GPU memory, uncached and write-combined.
		} else if(hostAccessSequentialWrite) {
			// Want uncached and write-combined.
			outNotPreferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

			if(!isIntegratedGPU && deviceAccess && hostAccessAllowTransferInstead && !preferHost) {
				outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			} else {
				outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
				// Direct GPU access, CPU sequential write (e.g. a dynamic uniform buffer updated every frame)
				if(deviceAccess) {
					// Could go to CPU memory or GPU BAR/unified. Up to the user to decide. If no preference, choose GPU memory.
					if(preferHost)
						outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
					else
						outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

				// GPU no direct access, CPU sequential write (e.g. an upload buffer to be transferred to the GPU)
				} else {
					// Could go to CPU memory or GPU BAR/unified. Up to the user to decide. If no preference, choose CPU memory.
					if(preferDevice)
						outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
					else
						outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
				}
			}
		// No CPU access
		} else {
			// if(deviceAccess)
			//
			// GPU access, no CPU access (e.g. a color attachment image) - prefer GPU memory,
			// unless there is a clear preference from the user not to do so.
			//
			// else:
			//
			// No direct GPU access, no CPU access, just transfers.
			// It may be staging copy intended for e.g. preserving image for next frame (then better GPU memory) or
			// a "swap file" copy to free some GPU memory (then better CPU memory).
			// Up to the user to decide. If no preferece, assume the former and choose GPU memory.

			if(preferHost)
				outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			else
				outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		}

		// Avoid DEVICE_COHERENT unless explicitly requested.
		if(((allocCreateInfo.requiredFlags | allocCreateInfo.preferredFlags) &
			(VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY)) == 0)
		{
			outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY;
		}
	}

	VkResult FindMemoryTypeIndex(uint32_t memoryTypeBits, const VmaAllocationCreateInfo* pAllocationCreateInfo, uint64_t bufferImageUsage, uint32_t* pMemoryTypeIndex) const {
		VkMemoryPropertyFlags requiredFlags = 0;
		VkMemoryPropertyFlags preferredFlags = 0;
		VkMemoryPropertyFlags notPreferredFlags = 0;
		FindMemoryPreferences(*pAllocationCreateInfo, bufferImageUsage, requiredFlags, preferredFlags, notPreferredFlags);

		*pMemoryTypeIndex = UINT32_MAX;
		uint32_t minCost = UINT32_MAX;
		for(
			uint32_t memTypeIndex = 0, memTypeBit = 1;
			memTypeIndex < m_MemProps.memoryTypeCount;
			++memTypeIndex, memTypeBit <<= 1
		) {
			// This memory type is acceptable according to memoryTypeBits bitmask.
			if(!(memTypeBit & memoryTypeBits)) {
				continue;
			}
			const VkMemoryPropertyFlags currFlags = m_MemProps.memoryTypes[memTypeIndex].propertyFlags;
			// This memory type contains requiredFlags.
			if(requiredFlags & ~currFlags) {
				continue;
			}
			// Calculate cost as number of bits from preferredFlags not present in this memory type.
			uint32_t currCost = std::popcount(preferredFlags & ~currFlags) + std::popcount(currFlags & notPreferredFlags);
			// Remember memory type with lowest cost.
			if(currCost < minCost)
			{
				*pMemoryTypeIndex = memTypeIndex;
				if(currCost == 0)
				{
					return VK_SUCCESS;
				}
				minCost = currCost;
			}
		}
		return (*pMemoryTypeIndex != UINT32_MAX) ? VK_SUCCESS : VK_ERROR_FEATURE_NOT_PRESENT;
	}

	VkResult AllocateMemory(
		const VkMemoryRequirements& vkMemReq,
		bool requiresDedicatedAllocation,
		bool prefersDedicatedAllocation,
		uint64_t bufferImageUsage,
		const VmaAllocationCreateInfo& createInfo,
		VmaSuballocationType suballocType,
		VmaAllocation* pAllocation
	) {
		uint32_t memoryTypeBits = vkMemReq.memoryTypeBits;
		uint32_t memTypeIndex = UINT32_MAX;
		for (;;) {
			if (FindMemoryTypeIndex(memoryTypeBits, &createInfo, bufferImageUsage, &memTypeIndex) != VK_SUCCESS) {
				return VK_ERROR_OUT_OF_DEVICE_MEMORY;
			}

			if (AllocateMemoryOfType(
				vkMemReq.size,
				vkMemReq.alignment,
				requiresDedicatedAllocation || prefersDedicatedAllocation,
				createInfo,
				memTypeIndex,
				suballocType,
				&m_DedicatedAllocations[memTypeIndex],
				*m_pBlockVectors[memTypeIndex],
				pAllocation
			) == VK_SUCCESS) {
				return VK_SUCCESS;
			}
			memoryTypeBits &= ~(1U << memTypeIndex);
		}
	}

	VkResult AllocateVulkanMemory(const VkMemoryAllocateInfo* pAllocateInfo, VkDeviceMemory* pMemory) {
		const uint32_t heapIndex = m_MemProps.memoryTypes[pAllocateInfo->memoryTypeIndex].heapIndex;
		if (pAllocateInfo->allocationSize > m_MemProps.memoryHeaps[heapIndex].size)
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}

		uint32_t deviceMemoryCountIncrement = 0;
		const uint64_t prevDeviceMemoryCount = deviceMemoryCountIncrement + m_DeviceMemoryCount;
		if(prevDeviceMemoryCount >= m_PhysicalDeviceProperties.limits.maxMemoryAllocationCount)
		{
			return VK_ERROR_TOO_MANY_OBJECTS;
		}

		m_Budget.m_BlockBytes[heapIndex] += pAllocateInfo->allocationSize;

		// VULKAN CALL vkAllocateMemory.
		VkResult res = (*m_VulkanFunctions.vkAllocateMemory)(m_hDevice, pAllocateInfo, nullptr, pMemory);

		if(res == VK_SUCCESS)
		{
			++m_Budget.m_OperationsSinceBudgetFetch;
		}
		else
		{
			m_Budget.m_BlockBytes[heapIndex] -= pAllocateInfo->allocationSize;
		}

		return res;
	}

	VkResult AllocateMemoryOfType(
		VkDeviceSize size,
		VkDeviceSize alignment,
		bool dedicatedPreferred,
		const VmaAllocationCreateInfo& createInfo,
		uint32_t memTypeIndex,
		VmaSuballocationType suballocType,
		VmaAllocation_T** dedicatedAllocations,
		VmaBlockVector& blockVector,
		VmaAllocation* pAllocation
	) {
		VmaAllocationCreateInfo finalCreateInfo = createInfo;
		VkResult res = CalcMemTypeParams(finalCreateInfo, memTypeIndex, size);
		if(res != VK_SUCCESS)
			return res;

		if((finalCreateInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) != 0)
		{
			return AllocateDedicatedMemory(
				size,
				suballocType,
				dedicatedAllocations,
				memTypeIndex,
				pAllocation
			);
		}

		// Heuristics: Allocate dedicated memory if requested size if greater than half of preferred block size.
		if(size > blockVector.m_PreferredBlockSize / 2)
		{
			dedicatedPreferred = true;
		}
		// Protection against creating each allocation as dedicated when we reach or exceed heap size/budget,
		// which can quickly deplete maxMemoryAllocationCount: Don't prefer dedicated allocations when above
		// 3/4 of the maximum allocation count.
		if(m_PhysicalDeviceProperties.limits.maxMemoryAllocationCount < UINT32_MAX / 4 &&
			m_DeviceMemoryCount > m_PhysicalDeviceProperties.limits.maxMemoryAllocationCount * 3 / 4)
		{
			dedicatedPreferred = false;
		}

		if(dedicatedPreferred)
		{
			res = AllocateDedicatedMemory(
				size,
				suballocType,
				dedicatedAllocations,
				memTypeIndex,
				pAllocation
			);
			if(res == VK_SUCCESS)
			{
				return VK_SUCCESS;
			}
		}

		res = blockVector.AllocatePage(size, alignment, suballocType, pAllocation);
		if(res == VK_SUCCESS)
			return VK_SUCCESS;

		// Try dedicated memory.
		if(!dedicatedPreferred)
		{
			res = AllocateDedicatedMemory(
				size,
				suballocType,
				dedicatedAllocations,
				memTypeIndex,
				pAllocation
			);
			if(res == VK_SUCCESS)
			{
				return VK_SUCCESS;
			}
		}
		// Everything failed: Return error code.
		return res;
	}

	VkResult AllocateDedicatedMemory(
		VkDeviceSize size,
		VmaSuballocationType suballocType,
		VmaAllocation_T** dedicatedAllocations,
		uint32_t memTypeIndex,
		VmaAllocation* pAllocation
	) {
		VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.memoryTypeIndex = memTypeIndex;
		allocInfo.allocationSize = size;
		allocInfo.pNext = nullptr;

		VkMemoryAllocateFlagsInfoKHR allocFlagsInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR };

		VkDeviceMemory hMemory = VK_NULL_HANDLE;
		VkResult res = AllocateVulkanMemory(&allocInfo, &hMemory);
		if(res < 0)
		{
			return res;
		}

		*pAllocation = new VmaAllocation_T;
		(*pAllocation)->InitDedicatedAllocation(memTypeIndex, hMemory, suballocType, size);
		m_Budget.m_OperationsSinceBudgetFetch++;
		// add to VmaAllocation_T**
		dedicatedAllocations;
		return VK_SUCCESS;
	}

	VkResult CalcMemTypeParams(VmaAllocationCreateInfo& inoutCreateInfo, uint32_t memTypeIndex, VkDeviceSize size) {
		if((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) != 0 &&
			(inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT) != 0)
		{
			const uint32_t heapIndex = m_MemProps.memoryTypes[memTypeIndex].heapIndex;
			VmaBudget heapBudget = {};
			GetHeapBudgets(&heapBudget, heapIndex, 1);
			if(heapBudget.usage + size > heapBudget.budget)
			{
				return VK_ERROR_OUT_OF_DEVICE_MEMORY;
			}
		}
		return VK_SUCCESS;
	}

	void UpdateVulkanBudget() {
		VkPhysicalDeviceMemoryProperties2KHR memProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR };
		VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
		VmaPnextChainPushFront(&memProps, &budgetProps);

		m_VulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR(m_PhysicalDevice, &memProps);

		for(uint32_t heapIndex = 0; heapIndex < m_MemProps.memoryHeapCount; ++heapIndex)
		{
			m_Budget.m_VulkanUsage[heapIndex] = budgetProps.heapUsage[heapIndex];
			m_Budget.m_VulkanBudget[heapIndex] = budgetProps.heapBudget[heapIndex];
			m_Budget.m_BlockBytesAtBudgetFetch[heapIndex] = m_Budget.m_BlockBytes[heapIndex];

			// Some bugged drivers return the budget incorrectly, e.g. 0 or much bigger than heap size.
			if(m_Budget.m_VulkanBudget[heapIndex] == 0)
			{
				m_Budget.m_VulkanBudget[heapIndex] = m_MemProps.memoryHeaps[heapIndex].size * 8 / 10; // 80% heuristics.
			}
			else if(m_Budget.m_VulkanBudget[heapIndex] > m_MemProps.memoryHeaps[heapIndex].size)
			{
				m_Budget.m_VulkanBudget[heapIndex] = m_MemProps.memoryHeaps[heapIndex].size;
			}
			if(m_Budget.m_VulkanUsage[heapIndex] == 0 && m_Budget.m_BlockBytesAtBudgetFetch[heapIndex] > 0)
			{
				m_Budget.m_VulkanUsage[heapIndex] = m_Budget.m_BlockBytesAtBudgetFetch[heapIndex];
			}
		}
		m_Budget.m_OperationsSinceBudgetFetch = 0;
	}

	void SetCurrentFrameIndex(uint32_t frameIndex) {
		m_CurrentFrameIndex = frameIndex;
		UpdateVulkanBudget();
	}
};

VkResult VmaBlockVector::AllocatePage(VkDeviceSize size, VkDeviceSize alignment, VmaSuballocationType suballocType, VmaAllocation* pAllocation) {
	VkDeviceSize freeMemory = 0;
	{
		const uint32_t heapIndex = m_hAllocator->m_MemProps.memoryTypes[m_MemoryTypeIndex].heapIndex;
		VmaBudget heapBudget = {};
		m_hAllocator->GetHeapBudgets(&heapBudget, heapIndex, 1);
		freeMemory = (heapBudget.usage < heapBudget.budget) ? (heapBudget.budget - heapBudget.usage) : 0;
	}

	const bool canCreateNewBlock = freeMemory >= size;

	// 1. Search existing allocations. Try to allocate.
	// Use only last block.
	if (!m_Blocks.empty())
	{
		VmaDeviceMemoryBlock* const pCurrBlock = m_Blocks.back();
		VkResult res = AllocateFromBlock(pCurrBlock, size, alignment, suballocType, pAllocation);
		if (res == VK_SUCCESS)
		{
			return VK_SUCCESS;
		}
	}

	// 2. Try to create new block.
	if (canCreateNewBlock) {
		VkDeviceSize newBlockSize = m_PreferredBlockSize;
		uint32_t newBlockSizeShift = 0;
		const uint32_t NEW_BLOCK_SIZE_SHIFT_MAX = 3;
		VkDeviceSize maxExistingBlockSize = 0;//std::max(blocks[i].size, ...);
		// Allocate 1/8, 1/4, 1/2 as first blocks.
		for (uint32_t i = 0; i < NEW_BLOCK_SIZE_SHIFT_MAX; ++i) {
			const VkDeviceSize smallerNewBlockSize = newBlockSize / 2;
			if (smallerNewBlockSize <= maxExistingBlockSize || smallerNewBlockSize < size * 2) {
				break;
			}
			newBlockSize = smallerNewBlockSize;
			++newBlockSizeShift;
		}

		size_t newBlockIndex = 0;
		VkResult res = newBlockSize <= freeMemory ? CreateBlock(newBlockSize, &newBlockIndex) : VK_ERROR_OUT_OF_DEVICE_MEMORY;
		// Allocation of this size failed? Try 1/2, 1/4, 1/8 of m_PreferredBlockSize.
		while (res < 0 && newBlockSizeShift < NEW_BLOCK_SIZE_SHIFT_MAX) {
			const VkDeviceSize smallerNewBlockSize = newBlockSize / 2;
			if (smallerNewBlockSize < size) {
				break;
			}
			newBlockSize = smallerNewBlockSize;
			++newBlockSizeShift;
			res = (newBlockSize <= freeMemory) ? CreateBlock(newBlockSize, &newBlockIndex) : VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}
		if (res != VK_SUCCESS) {
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}

		res = AllocateFromBlock(m_Blocks[newBlockIndex], size, alignment, suballocType, pAllocation);
		if (res == VK_SUCCESS)
		{
			return VK_SUCCESS;
		}
	}

	return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

VkResult VmaBlockVector::CreateBlock(VkDeviceSize blockSize, size_t* pNewBlockIndex) {
	VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocInfo.pNext = nullptr;
	allocInfo.memoryTypeIndex = m_MemoryTypeIndex;
	allocInfo.allocationSize = blockSize;

	// Every standalone block can potentially contain a buffer with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT - always enable the feature.
	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR };
	allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	VmaPnextChainPushFront(&allocInfo, &allocFlagsInfo);

	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkResult res = m_hAllocator->AllocateVulkanMemory(&allocInfo, &mem);
	if (res < 0)
	{
		return res;
	}

	// New VkDeviceMemory successfully created.

	// Create new Allocation for it.
	VmaDeviceMemoryBlock* const pBlock = vma_new(VmaDeviceMemoryBlock)();
	pBlock->Init(
		mem,
		allocInfo.allocationSize,
		m_BufferImageGranularity
	);

	m_Blocks.push_back(pBlock);
	if (pNewBlockIndex != nullptr)
	{
		*pNewBlockIndex = m_Blocks.size() - 1;
	}

	return VK_SUCCESS;
}

VkResult VmaBlockVector::AllocateFromBlock(
	VmaDeviceMemoryBlock* pBlock,
	VkDeviceSize size,
	VkDeviceSize alignment,
	VmaSuballocationType suballocType,
	VmaAllocation* pAllocation
) {
	VkDeviceSize resultOffset = 0;
	// Start from offset equal to beginning of free space.
	if (!pBlock->m_Suballocations.empty()) {
		const VmaSuballocation& lastSuballoc = pBlock->m_Suballocations.back();
		resultOffset = lastSuballoc.offset + lastSuballoc.size;
	}
	// Apply alignment.
	resultOffset = VmaAlignUp(resultOffset, alignment);

	const VkDeviceSize blockSize = pBlock->m_Size;
	if (m_BufferImageGranularity > 1 && m_BufferImageGranularity != alignment && !pBlock->m_Suballocations.empty()) {
		bool bufferImageGranularityConflict = false;
		for (size_t prevSuballocIndex = pBlock->m_Suballocations.size(); prevSuballocIndex--; ) {
			const VmaSuballocation& prevSuballoc = pBlock->m_Suballocations[prevSuballocIndex];

			VkDeviceSize aEnd       = prevSuballoc.offset + prevSuballoc.size - 1;
			VkDeviceSize aEndPage   = aEnd & ~(m_BufferImageGranularity - 1);
			VkDeviceSize bStart     = 0;	// resultOffset
			VkDeviceSize bStartPage = bStart & ~(m_BufferImageGranularity - 1);
			if (aEndPage != bStartPage) {
				break;
			}
			if (VmaIsBufferImageGranularityConflict(prevSuballoc.type, suballocType)) {
				bufferImageGranularityConflict = true;
				break;
			}
		}
		if (bufferImageGranularityConflict) {
			resultOffset = VmaAlignUp(resultOffset, m_BufferImageGranularity);
		}
	}

	if (resultOffset + size > blockSize) {
		return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	};

	*pAllocation = new VmaAllocation_T;
	pBlock->m_Suballocations.push_back(VmaSuballocation{ resultOffset, size, *pAllocation, suballocType });
	(*pAllocation)->m_Type                          = ALLOCATION_TYPE_BLOCK;
	(*pAllocation)->m_Alignment                     = alignment;
	(*pAllocation)->m_Size                          = size;
	(*pAllocation)->m_MemoryTypeIndex               = m_MemoryTypeIndex;
	(*pAllocation)->m_SuballocationType             = (uint8_t)suballocType;
	(*pAllocation)->m_BlockAllocation.m_Block       = pBlock;
	m_hAllocator->m_Budget.m_OperationsSinceBudgetFetch++;

	return VK_SUCCESS;
}