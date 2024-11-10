#include "RenderMem_Vulkan.h"

#include "Array.h"
#include "Bit.h"
#include "Error.h"
#include "Log.h"
#include "Mem.h"

namespace JC {

//-------------------------------------------------------------------------------------------------

#define VK_CHECK(expr) JC_CHECK_FATAL((expr) == VK_SUCCESS)

namespace RenderMem {
	struct SharedDeviceAllocation {
		VkDeviceMemory          deviceMemory;
		u64                     size;
		u64                     free1;
		u64                     free2[64];
		SharedDeviceAllocation* next;
	};

	struct SharedAllocator {
		u64 chunkSize;
		SharedDeviceAllocation* shareddDeviceAllocations;
	};

	struct AllocationData {
		u32 memoryTypeIndex;
		u64 size;
		union {
			struct {
				VkDeviceMemory  deviceMemory;
				AllocationData* next;
				AllocationData* prev;
			} dedicated;
			struct {
				//???
				u64 offset;
			} shared;
		};
	};

	Log::Scope                       logScope;

	Mem::Allocator                   allocator;

	VkInstance                       instance;
	VkPhysicalDevice                 physicalDevice;
	VkDevice                         device;
	VkAllocationCallbacks*           allocationCallbacks;
	VkPhysicalDeviceProperties       physicalDeviceProperties;
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;

	u32                              deviceTotalAllocs;
	u32                              memoryTypeToHeap[VK_MAX_MEMORY_TYPES];
	BlockAllocator<AllocationData>   allocationDataAllocator;
	AllocationData*                  dedicatedAllocationList;
	u64                              heapBytesAllocated[VK_MAX_HEAPS];
	SharedAllocator                  sharedAllocators[VK_MAX_MEMORY_TYPES];

	JC_DEFINE_LOG_FUNCS(RenderMem, logScope);

	Allocation Alloc(u64 size, u32 memoryTypeIndex);
	Allocation AllocDedicated(u64 size, u32 memoryTypeIndex);
	Allocation AllocShared(u64 size, u32 memoryTypeIndex);
}	// namespace RenderMem

//-------------------------------------------------------------------------------------------------

void RenderMem::Init(VkInstance inInstance, VkPhysicalDevice inPhysicalDevice, VkDevice inDevice, VkAllocationCallbacks* allocationCallbacks) {
	logScope = Log::CreateScope("RenderMem");

	allocator = Mem::CreateChildAllocator("RenderMem", Mem::RootAllocator());

	instance = inInstance;
	physicalDevice = inPhysicalDevice;
	device = inDevice;
	allocationCallbacks = allocationCallbacks;
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

	allocationDataAllocator.Init(allocator, 32);

	// Sentinel head node
	dedicatedAllocationList = allocationDataAllocator.Alloc();
	dedicatedAllocationList->memoryTypeIndex        = VK_MAX_MEMORY_TYPES;
	dedicatedAllocationList->size                   = 0;
	dedicatedAllocationList->dedicated.deviceMemory = VK_NULL_HANDLE;
	dedicatedAllocationList->dedicated.next         = dedicatedAllocationList;
	dedicatedAllocationList->dedicated.prev         = dedicatedAllocationList;

	for (u32 i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
		u32 heapIndex = physicalDeviceMemoryProperties.memoryTypes[i].heapIndex;
		memoryTypeToHeap[i] = heapIndex;
		if (physicalDeviceMemoryProperties.memoryHeaps[i].size >= 1024 * 1024 * 1024) {
			sharedAllocators[i].chunkSize = 256 * 1024 * 1024;
		} else {
			sharedAllocators[i].chunkSize = physicalDeviceMemoryProperties.memoryHeaps[heapIndex].size / 8;
		}
	}

}

//-------------------------------------------------------------------------------------------------

RenderMem::Allocation RenderMem::AllocBuffer(Render::Usage usage, u64 size, VkBuffer buffer) {
	JC_ASSERT(size > 0);
	JC_ASSERT(usage > Render::Usage::Null);
	JC_ASSERT(usage < Render::Usage::Max);
	JC_ASSERT(buffer != VK_NULL_HANDLE);

	VkMemoryPropertyFlags must = 0;
	VkMemoryPropertyFlags want = 0;
	VkMemoryPropertyFlags dontWant = 0;

	if (usage == Render::Usage::Static) {
		want     |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	} else if (usage == Render::Usage::Dynamic) {
		must     |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		want     |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		dontWant |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	} else if (usage == Render::Usage::Staging) {
		must     |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		dontWant |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
	u32 memoryTypeBits = memoryRequirements.memoryTypeBits;

	while (true) {
		u32 memoryTypeIndex = 0xffffffff;
		i32 maxScore = 0;
		for (u32 i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
			if ((memoryTypeBits & (1 << i)) == 0) {
				continue;
			}
			u32 memFlags = physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags;
			if ((memFlags & must) != must) {
				continue;
			}
			// +1 for each matching preferred bit, -1 for each matching not-preferred bit
			i32 score = CountBits32(memFlags & want) - CountBits32(memFlags & dontWant);
			if (score > maxScore) {
				memoryTypeIndex = i;
				score = maxScore;
			}
		}

		JC_CHECK_FATALF(memoryTypeIndex != 0xffffffff, "Couldn't allocate %d bytes for buffer with usage %s", size, Render::UsageStr(usage));

		Allocation allocation = Alloc(size, memoryTypeIndex);
		if (allocation.handle != 0) {
			VK_CHECK(vkBindBufferMemory(device, buffer, mem, offset));

			return allocation;
		}
	}
}

//-------------------------------------------------------------------------------------------------

RenderMem::Allocation RenderMem::Alloc(u64 size, u32 memoryTypeIndex) {
	bool preferDedicated = false;

	if (size > sharedAllocators[memoryTypeIndex].chunkSize / 2) {
		preferDedicated = true;
	}

	if (deviceTotalAllocs > physicalDeviceProperties.limits.maxMemoryAllocationCount * 3 / 4) {
		preferDedicated = false;
	}

	Allocation allocation;
	if (preferDedicated) {
		allocation = AllocDedicated(size, memoryTypeIndex);
		if (allocation.handle != 0) {
			return allocation;
		}
	}

	allocation = AllocShared(size, memoryTypeIndex);
	if (allocation.handle != 0) {
		return allocation;
	}

	if (!preferDedicated) {
		allocation = AllocDedicated(size, memoryTypeIndex);
		if (allocation.handle != 0) {
			return allocation;
		}
	}

	return { .handle = 0 };
}

//-------------------------------------------------------------------------------------------------


RenderMem::Allocation RenderMem::AllocDedicated(u64 size, u32 memoryTypeIndex) {
	VkMemoryAllocateInfo memoryAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = size,
		.memoryTypeIndex = memoryTypeIndex,
	};
	VkDeviceMemory deviceMemory;
	VkResult result = vkAllocateMemory(device, &memoryAllocateInfo, allocationCallbacks, &deviceMemory);
	if (result != VK_SUCCESS) {
		Errorf("vkAllocateMemory failed: size=%d, memoryTypeIndex=%d", size, memoryTypeIndex);
		return { .handle = 0 };
	}

	u32 heapIndex = memoryTypeToHeap[memoryTypeIndex];
	heapBytesAllocated[heapIndex] += size;
	deviceTotalAllocs++;

	AllocationData* allocationData = allocationDataAllocator.Alloc();

	allocationData->memoryTypeIndex = memoryTypeIndex;
	allocationData->size            = size;
	allocationData->dedicated.next  = dedicatedAllocationList;
	allocationData->dedicated.prev  = dedicatedAllocationList->dedicated.prev;
	dedicatedAllocationList->dedicated.prev->dedicated.next = allocationData;
	dedicatedAllocationList->dedicated.prev;
}

//-------------------------------------------------------------------------------------------------

// blocks are sorted according to smallest amount of free space first
void AllocFromBlocks() {
	ASSERT(size < blockSize);
	u32 heapBudget = heap[index].size * 0.8;
	u32 heapAvailable = heapBudget - heapUsed;

	if (dynamic || staging) {	// equivalent to host visible, we always want mapped
		// prefer already-mapped blocks
		for (u32 i = 0; i < blocks.count; i++) {
			if (blocks[i].mapped) {
				if (AllocateFromBlock(block[i])) {
					IncrementallySortBlocks();
					return true;
				}
			}
		}
		// fall back to not-mapped blocks
		for (u32 i = 0; i < blocks.count; i++) {
			if (!blocks[i].mapped && AllocateFromBlock(block[i])) {
				IncrementallySortBlocks();
				return true;
			}
		}
	} else {
		for (u32 i = 0; i < blocks.count; i++) {
			if (AllocateFromBlock(block[i])) {
				IncrementallySortBlocks();
				return true;
			}
		}
	}

	// try create new block
	if (size > available) {
		return false;
	}

	u64 maxBlockSize = ..find largest size across all blocks..;	// why would this ever not just be our preferred size?
	u64 blockSize = preferredBlockSize;


	// Since we initially have no blocks, our maxBlockSize will be 0 at first
	// This algo allocs the first 3 blocks as 1/8, 1/4, and 1/2
	for (frac in 1/8, 1/4, 1/2) {
		u64 smallerSize = blockSize * frac;
		if (smallerSize > maxBlockSize && smallerSize > size * 2) {
			break;
		}
	}

	u32 newBlockIndex;
	bool res = false;
	if (blockSize <= available) {
		res = CreateBlock(blockSiz, &newBlockIndexe);
	}
	while (!res && frac > 1/8) {
		blockSize /= 2;
		frac /= 2;
		if (blockSize < size) {
			break;
		}
		if (blockSize <= available) {
			res = CreateBlock(blockSize, &newBlockIndex);
		}
	}

	if (!res) {
		return false;
	}

	if (AllocFromBlock(blocks[newBlockIndex])) {
		IncrementallySortBlocks();
		return true;
	}

	return false;
}

bool AllocFromBlock(Block* block, u64 size) {
	// apply TLSF algorithm to this block
}

void IncrementallySortBlocks(Block* block) {
	if (!sort) {
		return;
	}

	// bubble down this block until its correct place
	// real algo only does one swap
}

//-------------------------------------------------------------------------------------------------
} // namespace JC
