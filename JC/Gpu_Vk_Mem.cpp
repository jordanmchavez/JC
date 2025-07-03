
//#include "JC/Bit.h"

//namespace JC::Gpu {
/*
//--------------------------------------------------------------------------------------------------
// Interface

usage pattern for the most basic game: allocate only, never free
everything fits in vram, so just allocate a chunk of sufficient size and linearly allocate

more complex usage pattern
levels
need a level allocator as well as a perm allocator
one simple option is to use the "top" of each chunk
other option is to load all the perm stuff, "lock" those, then reset on top of that

struct Allocation {
	VkDeviceMemory vkDeviceMemory;
	U64            offset;
	U64            size;
};

//--------------------------------------------------------------------------------------------------

static VkDevice               vkDevice;
static VkAllocationCallbacks* vkAllocationCallbacks;

//--------------------------------------------------------------------------------------------------

void Init(VkDevice vkDeviceIn, VkAllocationCallbacks* vkAllocationCallbacksIn) {
	vkDevice              = vkDeviceIn;
	vkAllocationCallbacks = vkAllocationCallbacksIn;
}

//--------------------------------------------------------------------------------------------------

Res<Allocation> Allocate() {
	const VkMemoryAllocateFlagsInfo vkMemoryAllocateFlagsInfo = {
		.sType      = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.pNext      = 0,
		.flags      = vkMemoryAllocateFlags,
		.deviceMask = 0,
	};

	const VkMemoryAllocateInfo vkMemoryAllocateInfo = {
		.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext           = &vkMemoryAllocateFlagsInfo,
		.allocationSize  = vkMemoryRequirements.size,
		.memoryTypeIndex = memType,
	};

	VkDeviceMemory vkDeviceMemory;
	vkAllocateMemory(vkDevice, vkMemoryAllocateInfo, vkAllocationCallbacks, &vkDeviceMemory);
}

//--------------------------------------------------------------------------------------------------

enum struct MemUsage {
};

struct Chunk {
	VkDeviceMemory vkDeviceMemory;
};

static constexpr U32 MaxChunksPerType = 64;	// Min chunk size is 1/64 heap size

struct MemType {
	Chunk chunks[MaxChunksPerType];
	U32   chunksLen;
	U64   maxChunkSize;
	U64   currentMaxChunkSize;
};

static VkInstance                       vkInstance;
static VkPhysicalDevice                 vkPhysicalDevice;
static VkPhysicalDeviceProperties       vkPhysicalDeviceProperties;
static VkPhysicalDeviceMemoryProperties vkPhysicalDeviceMemoryProperties;
static U32                              memTypeBits;
static Type                             types[VK_MAX_MEMORY_TYPES];

Res<Allocation> Allocate() {
	U32 memType = U32Max;
	for (U32 i = 0; i < vkPhysicalDeviceMemoryProperties.memoryTypeCount; i++) {
		if (
			(vkMemoryRequirements.memoryTypeBits & (1 << i)) &&
			(physicalDevice->vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & vkMemoryPropertyFlags)
		) {
			memType = i;
		}
	}
	if (memType == U32Max) {
		return Err_NoMem();
	}
	const VkMemoryAllocateFlagsInfo vkMemoryAllocateFlagsInfo = {
		.sType      = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.pNext      = 0,
		.flags      = vkMemoryAllocateFlags,
		.deviceMask = 0,
	};
	const VkMemoryAllocateInfo vkMemoryAllocateInfo = {
		.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext           = &vkMemoryAllocateFlagsInfo,
		.allocationSize  = vkMemoryRequirements.size,
		.memoryTypeIndex = memType,
	};
	VkDeviceMemory vkDeviceMemory = VK_NULL_HANDLE;
	CheckVk(vkAllocateMemory(vkDevice, &vkMemoryAllocateInfo, vkAllocationCallbacks, &vkDeviceMemory));

	return Mem {
		.vkDeviceMemory        = vkDeviceMemory,
		.offset                = 0,
		.size                  = vkMemoryRequirements.size,
		.type                  = memType,
		.vkMemoryPropertyFlags = vkMemoryPropertyFlags,
		.vkMemoryAllocateFlags = vkMemoryAllocateFlags,
	};
};


Res<> Init() {
/*
General algorithm:
heap.maxChunkSize = Min(256mb, heapSize / 8)
each memtype has a set of chunks
if requested size fits in an existing chunk, use it
else allocate a new chunk for this memtype:
	if not enough budget
		fail
	if requestedSize > heap.maxChunkSize
		dedicated allocation
	for frac in [1/8, 1/4, 1/2, 1]
		newChunkSize = heap.maxChunkSize * frac
		if newChunkSize >= maxExistingChunkSize && newChunkSize > requestedSize * 2
			use newChunkSize
	maxExistingChunkSize = max(newChunkSize, maxExistingChunkSize);
	then use linear/tlsf or whatever for allocations

*//*
	U32 memTypeBits = U32Max;
	// TODO: exclude VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD?
	// vkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY

	for (U32 m = 0; m < vkPhysicalDeviceMemoryProperties.memoryTypeCount; m++) {
		const U32 h        = vkPhysicalDeviceMemoryProperties.memoryTypes[m].heapIndex;
		const U64 heapSize = vkPhysicalDeviceMemoryProperties.memoryHeaps[h].size;
		U64 blockSize;
		if (heapSize < 1024 * 1024 * 1024) {
			blockSize = heapSize / 8;
		} else {
			blockSize = 256 * 1024 * 1024;
		}
		blockSize = Bit::AlignUp(blockSize, 32);

		U64 minAlign = 1;
		const U64 flags = vkPhysicalDeviceMemoryProperties.memoryTypes[memTypeIndex].propertyFlags;
		if (
			(flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
			~(flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
		) {
			minAlign = vkPhysicalDeviceProperties.limits.nonCoherentAtomSize;
		}

		for each mem type
			init suballocator for this type:
			list of chunks
			max chunk size
			current max chunk size

		m_pBlockVectors[m].Init(
			m,
			blockSize,
			vkPhysicalDeviceProperties.limits.bufferImageGranularity,
			minAlignGetMemoryTypeMinAlignment(memTypeIndex)
		);

	}

	UpdateVulkanBudget();
}

struct MemReqs {
	VkMemoryPropertyFlags required;
	VkMemoryPropertyFlags preferred;
	VkMemoryPropertyFlags notPreferred;
};

enum struct MemUsage {
	
};
Res<MemReqs> CalcMemReqs() {
	MemReqs memReqs;

	const Bool discreteGpu  = (vkPhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
	const Bool deviceAccess = bufferUsageFlags & ~(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	const Bool preferDevice = flags & PreferDevice;
	const Bool preferHost   = flags & PreferHost;
	if (flags & HostRandomAccess) {
		// Prefer cached. Cannot require it, because some platforms don't have it (e.g. Raspberry Pi - see #362)!
		memReqs.preferred |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

		if (
			discreteGpu &&
			deviceAccess && 
			(flags & HostAccess_AllowTransferInstead)
		) {
			// Picks DEVICE_LOCAL | HOST_VISIBLE | HOST_CACHED if possible
			// Else whichever comes first between DEVICE_LOCAL and HOST_VISIBLE | HOST_CACHED
			// Doesn't guarantee HOST_VISIBLE
			memReqs.preferred |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			// rely on caller to check for non-HOST_VISIBLE and do the staging deal
		} else {
			// Always CPU
			memReqs.required |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		}
	} else if (flags & HostSequentialWrite) {
		memReqs.notPreferred |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		if (
			discreteGpu &&
			deviceAccess &&
			(flags & HostAccess_AllowTransferInstead)
		) {
			memReqs.preferred |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			// rely on caller to check for non-HOST_VISIBLE and do the staging deal
		} else {
			memReqs.required |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			if (deviceAccess) {
				memReqs.preferred |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			} else {
				memReqs.preferred |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			}
		}
	} else {
		// No CPU
		memReqs.preferred |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}

	return memReqs;
}

void AllocateMemory() {
	
}
/*

large allocations
what are my types?
GPU only, like render targets, depth buffers, gbuffers, etc
These get dedicated DEVICE_LOCAL allocations, but can fall back to HOST_VISIBLE
	prefer DEVICE_LOCAL
	prefer dedicated allocations, but this is user-specified
cpu-loaded resources like geometry and textures
	prefer DEVICE_LOCAL
	use large shared buffers per resource type, but again this is user-specified
	loaded with dedicated staging buffers
staging buffers
	rqeuire HOST_VISIBLE
dynamic resources like sprites and UI
	device_local / coherent

bulk-loading resources:
	have a fixed amount of staging memory (256mb)
	begin an immediate submit batch
	load as much as we can into staging memory
	submit the batch and wait
	repeat
	so essentially we have a queue of load commands, possibly from files, possibly from lambdas (cubes and such)

regarding BAR:
	we need a dedicated code path to take advantage of this.
	user code needs to allocate a "Dynamic" usage buffer and check whether it's DEVICE_LOCAL
	If so, no staging. Otherwise, stage.
	I think this is a TODO item: for now, everything dynamic is staged
*/
/*
void CreateImage() {
	vkCreateImage;
	vkGetImageMemoryRequirements2;
		VkImageMemoryRequirementsInfo2;
		VkMemoryRequirements2 + VkMemoryDedicatedRequirements;
		// gives prefers/requires dedicated
}
/*
Static
	>= 256mb
		Dedicated DEVICE_LOCAL
		Fallback  0
	< 256mb
		TLSF     DEVICE_LOCAL
		Fallback 0

Staging
	HOST_VISIBLE | HOST_COHERENT
	no fallback
	< 64k allocation
	>= 64k allocations do the linear arena thing on the list of blocks managed by the allocator
	freeing a linear block decreases the usage and if zero offset goes to zero (full block reset)
	arena allocator
	256mb, rounded up to next multiple of 256mb for the linear allocator
	64k minimum block size, but double arena (arena within arena, doesn't make any sense)
*/

// TODO: use memory budget and limit our top-end allocations to those values

//--------------------------------------------------------------------------------------------------

//}	// namespace JC::Gpu