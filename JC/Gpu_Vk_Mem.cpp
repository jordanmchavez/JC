#include "JC/Core.h"
#include "JC/Gpu_Vk.h"
#include "JC/Bit.h"

namespace JC::Gpu {

//--------------------------------------------------------------------------------------------------

DefErr(Gpu, NoMemType);
DefErr(Gpu, NoMem);

//--------------------------------------------------------------------------------------------------

namespace AllocFlags {
	using Type = U64;
	constexpr Type CpuRead  = 1 << 0;
	constexpr Type CpuWrite = 1 << 1;
	constexpr Type Gpu      = 1 << 2;
};

struct AllocRequest {
	U32                   size;
	VkMemoryRequirements  vkMemoryRequirements;
	VkMemoryAllocateFlags vkMemoryAllocateFlags;
	Bool                  needDedicated;
	Bool                  wantDedicated;
	AllocFlags::Type      allocFlags;
	VkFlags               vkBufferImageUsageFlags;
	Bool                  linear;
};

struct Allocation {
	VkDeviceMemory vkDeviceMemory;
	U64            size;
	U64            offset;
};

enum struct MemChunkType { Linear, Dedicated };

struct MemChunk {
	MemChunkType   type;
	VkDeviceMemory vkDeviceMemory;
	U64            size;
	U64            used;
	Bool           curPageLinear;
};

static constexpr U32 MaxChunks = 256;	// 196 dedicated (arbitrary) + 64 linear (each at least 1/64 heap size)

struct MemType {
	U32       idx;
	MemChunk  chunks[MaxChunks];
	U32       chunksLen;
	MemChunk* curChunk;
	U64       minChunkSize;
	U64       maxChunkSize;
	U64       biggestChunkSize;
};

//--------------------------------------------------------------------------------------------------

static VkDevice                         vkDevice;
static VkPhysicalDevice                 vkPhysicalDevice;
static VkAllocationCallbacks*           vkAllocationCallbacks;
static VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;
static VkPhysicalDeviceProperties       vkPhysicalDeviceProperties;
static MemType                          memTypes[VK_MAX_MEMORY_TYPES];
static U64                              totalAllocCount;
static Bool                             discreteGpu;

//--------------------------------------------------------------------------------------------------

static void InitMem() {
	for (U32 i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
		memTypes[i].idx = i;
		const U64 heapSize = vkPhysicalDeviceMemoryProperties.memoryHeaps[vkPhysicalDeviceMemoryProperties.memoryTypes[i].heapIndex].size;
		memTypes[i].minChunkSize     = Min(heapSize / 64,  32*MB);
		memTypes[i].maxChunkSize     = Min(heapSize /  8, 256*MB);
		memTypes[i].biggestChunkSize = memTypes[i].maxChunkSize;
	}
	discreteGpu = vkPhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

//--------------------------------------------------------------------------------------------------

// From gpuinfo.org as of 7/2025:
//                                                           |  Win |  Lin |  Mac
// ----------------------------------------------------------+------+------+-----
// DEVICE_LOCAL |              |               |             | 89.4 | 81.5 | 99.0
//              | HOST_VISIBLE | HOST_COHERENT | HOST_CACHED | 82.6 | 76.2 | 62.5
//              | HOST_VISIBLE | HOST_COHERENT |             | 82.5 | 73.3 |  3.1
// DEVICE_LOCAL | HOST_VISIBLE | HOST_COHERENT |             | 82.5 | 73.3 |  2.1
// DEVICE_LOCAL | HOST_VISIBLE | HOST_COHERENT | HOST_CACHED | 17.1 | 21.2 | 39.6
// DEVICE_LOCAL | HOST_VISIBLE |               | HOST_CACHED |  0.0 |  0.0 | 95.8

static Res<MemType*> SelectMemType(AllocFlags::Type allocFlags, U32 memoryTypeBits) {
	VkMemoryPropertyFlags needFlags  = 0;
	VkMemoryPropertyFlags wantFlags  = 0;
	VkMemoryPropertyFlags avoidFlags = 0;
	if (allocFlags | AllocFlags::CpuRead) {
		needFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		wantFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	} else if (allocFlags | AllocFlags::CpuWrite) {
		needFlags  |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		avoidFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		avoidFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	} else if (allocFlags | AllocFlags::Gpu) {
		wantFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}

	U32 minCost = U32Max;
	for (U32 i = 0, typeBit = 1; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++, typeBit <<= 1) {
		if (!(memoryTypeBits & typeBit)) {
			continue;
		}
		const VkMemoryPropertyFlags typeFlags = vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags;
		if ((typeFlags & needFlags) != needFlags) {
			continue;
		}
		const U32 cost = Bit::PopCount32(wantFlags & ~typeFlags) + Bit::PopCount32(avoidFlags & ~typeFlags);
		if (cost < minCost) {
			if (cost == 0) {
				return &memTypes[i];
			}
			minCost = cost;
		}
	}

	return Err_NoMemType("allocFlags", allocFlags, "typeBits", memoryTypeBits);
}

//--------------------------------------------------------------------------------------------------

static Res<VkDeviceMemory> AllocVkDeviceMemory(U32 memTypeIdx, U64 size) {
	const VkMemoryAllocateInfo vkMemoryAllocateInfo = {
		.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext           = 0,
		.allocationSize  = size,
		.memoryTypeIndex = memTypeIdx,
	};
	VkDeviceMemory vkDeviceMemory = VK_NULL_HANDLE;
	if (VkResult vkResult = vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, vkAllocationCallbacks, &vkDeviceMemory); vkResult != VK_SUCCESS) {
		return Err_Vk(vkResult, "vkAllocateMemory", "memTypeIdx", memTypeIdx, "size", size);
	}
	totalAllocCount++;
	return vkDeviceMemory;
}

//--------------------------------------------------------------------------------------------------

static Res<Allocation> AllocDedicatedChunk(MemType* memType, U64 size) {
	Assert(memType->chunksLen < MaxChunks);

	const VkMemoryAllocateInfo vkMemoryAllocateInfo = {
		.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext           = 0,
		.allocationSize  = size,
		.memoryTypeIndex = memType->idx,
	};
	VkDeviceMemory vkDeviceMemory = VK_NULL_HANDLE;
	if (Res<> r = AllocVkDeviceMemory(memType->idx, size).To(vkDeviceMemory); !r) {
		return r.err;
	}

	memType->chunks[memType->chunksLen++] = {
		.type           = MemChunkType::Dedicated,
		.vkDeviceMemory = vkDeviceMemory,
		.size           = size,
		.used           = 0,
	};

	return Allocation {
		.vkDeviceMemory = vkDeviceMemory,
		.size           = size,
		.offset         = 0,
	};
}

//--------------------------------------------------------------------------------------------------

static Res<Allocation> AllocFromChunk(MemType* memType, U64 size, U64 align, Bool linear) {
	if (!memType->curChunk || memType->curChunk->used + size > memType->curChunk->size) {
		Assert(memType->chunksLen < MaxChunks);
		U64 newChunkSize = Max(memType->biggestChunkSize, size * 2);
		VkDeviceMemory vkDeviceMemory = VK_NULL_HANDLE;
		for (;;) {
			if (Res<> r = AllocVkDeviceMemory(memType->idx, size).To(vkDeviceMemory)) {
				break;
			}
			newChunkSize /= 2;
			if (newChunkSize < size) {
				return Err_NoMem("memTypeIdx", memType->idx, "size", size);
			}
		}

		// TODO: record waste from old chunk, if any
		memType->curChunk = &memType->chunks[memType->chunksLen++];
		*memType->curChunk = {
			.type           = MemChunkType::Linear,
			.vkDeviceMemory = vkDeviceMemory,
			.size           = size,
			.used           = 0,
			.curPageLinear  = linear,	// Default to current request
		};
	}

	U64 offset = Bit::AlignUp(memType->curChunk->used, align);

	// Handle linear/nonlinear granularity
	if (align >= vkPhysicalDeviceProperties.limits.bufferImageGranularity) {
		// Automatically aligned
		memType->curChunk->curPageLinear = linear;
	} else if (memType->curChunk->curPageLinear != linear) {
		// Need to align to linear/nonlinear page size
		offset = Bit::AlignUp(offset, vkPhysicalDeviceProperties.limits.bufferImageGranularity);
		// TODO: record waste
		memType->curChunk->curPageLinear = linear;
	}

	return Allocation {
		.vkDeviceMemory = memType->curChunk->vkDeviceMemory,
		.size           = size,
		.offset         = offset,
	};
}

//--------------------------------------------------------------------------------------------------

// TODO: add tracking here: file/line associated with each block
static Res<Allocation> Alloc(const AllocRequest* req) {
	U32 memoryTypeBits = req->vkMemoryRequirements.memoryTypeBits;
	for (;;) {
		MemType* memType;
		if (Res<> r = SelectMemType(req->allocFlags, memoryTypeBits).To(memType); !r) {
			return r.err;
		}

		if (req->needDedicated) {
			return AllocDedicatedChunk(memType, req->size);
		}
		Bool wantDedicated =
			(req->size > memType->maxChunkSize / 2) &&
			(totalAllocCount<= (U64)vkPhysicalDeviceProperties.limits.maxMemoryAllocationCount * 3 / 4)
		;
		Allocation allocation;
		if (wantDedicated) {
			if (Res<> r = AllocDedicatedChunk(memType, req->size).To(allocation)) {
				return allocation;
			}
		}
	// https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#glossary-linear-resource
	// Linear     == VkBuffer | VkImage+VK_IMAGE_TILING_LINEAR
	// Non-linear == VkImage+VK_IMAGE_TILING_OPTIMAL
		if (Res<> r = AllocFromChunk(memType, req->size, req->vkMemoryRequirements.alignment, req->linear).To(allocation)) {
			return allocation;
		}
		if (!wantDedicated) {
			if (Res<> r = AllocDedicatedChunk(memType, req->size).To(allocation)) {
				return allocation;
			}
		}

		memoryTypeBits &= ~(1u << memType->idx);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC::Gpu