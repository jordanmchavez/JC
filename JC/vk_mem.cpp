#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <utility>
#include <type_traits>
#include <intrin.h> // For functions like __popcnt, _BitScanForward etc.
#include <bit>
#include <algorithm> // for min, max, swap

#pragma warning (disable: 4324)

VK_DEFINE_NON_DISPATCHABLE_HANDLE(VmaAllocHandle);

template<typename T>
class VmaList;

template<typename ItemTypeTraits>
class VmaIntrusiveLinkedList;

class VmaDeviceMemoryBlock;

struct VmaDedicatedAllocationListItemTraits;
class VmaDedicatedAllocationList;

struct VmaSuballocation;
struct VmaSuballocationOffsetLess;
struct VmaSuballocationOffsetGreater;
struct VmaSuballocationItemSizeLess;

typedef VmaList<VmaSuballocation> VmaSuballocationList;

struct VmaAllocationRequest;

class VmaBlockMetadata;
class VmaBlockMetadata_Linear;
class VmaBlockMetadata_TLSF;

struct VmaBlockVector;

struct VmaPoolListItemTraits;

struct VmaCurrentBudgetData;

class VmaAllocationObjectAllocator;


enum VmaMemoryUsage {
	VMA_MEMORY_USAGE_UNKNOWN = 0,
	VMA_MEMORY_USAGE_AUTO = 7,
	VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE = 8,
	VMA_MEMORY_USAGE_AUTO_PREFER_HOST = 9,
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
	PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
	PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
	PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
	PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
	PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;
	PFN_vkGetDeviceBufferMemoryRequirementsKHR vkGetDeviceBufferMemoryRequirements;
	PFN_vkGetDeviceImageMemoryRequirementsKHR vkGetDeviceImageMemoryRequirements;
};

struct VmaAllocatorCreateInfo {
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
};

struct VmaStatistics {
	uint32_t blockCount;
	uint32_t allocationCount;
	VkDeviceSize blockBytes;
	VkDeviceSize allocationBytes;
};

struct VmaBudget {
	VmaStatistics statistics;
	VkDeviceSize usage;
	VkDeviceSize budget;
};

struct VmaAllocationCreateInfo {
	VmaAllocationCreateFlags flags;
	VmaMemoryUsage usage;
	VkMemoryPropertyFlags requiredFlags;
	VkMemoryPropertyFlags preferredFlags;
	uint32_t memoryTypeBits;
	float priority;
};

struct VmaAllocationInfo {
	uint32_t memoryType;
	VkDeviceMemory deviceMemory;
	VkDeviceSize offset;
	VkDeviceSize size;
};

#define VMA_COUNT_BITS_SET(v) VmaCountBitsSet(v)
#define VMA_BITSCAN_LSB(mask) VmaBitScanLSB(mask)
#define VMA_BITSCAN_MSB(mask) VmaBitScanMSB(mask)
#define VMA_MIN(v1, v2)    ((std::min)((v1), (v2)))
#define VMA_MAX(v1, v2)    ((std::max)((v1), (v2)))
#define VMA_SORT(beg, end, cmp)  std::sort(beg, end, cmp)

#define VMA_SMALL_HEAP_MAX_SIZE (1024ULL * 1024 * 1024)
#define VMA_DEFAULT_LARGE_HEAP_BLOCK_SIZE (256ULL * 1024 * 1024)

static const uint32_t VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY = 0x00000040;
static const uint32_t VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY = 0x00000080;

enum VmaSuballocationType
{
	VMA_SUBALLOCATION_TYPE_FREE = 0,
	VMA_SUBALLOCATION_TYPE_UNKNOWN = 1,
	VMA_SUBALLOCATION_TYPE_BUFFER = 2,
	VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN = 3,
	VMA_SUBALLOCATION_TYPE_IMAGE_LINEAR = 4,
	VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL = 5,
	VMA_SUBALLOCATION_TYPE_MAX_ENUM = 0x7FFFFFFF
};

enum class VmaAllocationRequestType
{
	Normal,
	TLSF,
	// Used by "Linear" algorithm.
	UpperAddress,
	EndOf1st,
	EndOf2nd,
};

#ifndef _VMA_FUNCTIONS

static inline uint32_t VmaCountBitsSet(uint32_t v)
{
	return std::popcount(v);
}

static inline uint8_t VmaBitScanLSB(uint64_t mask)
{
	unsigned long pos;
	if (_BitScanForward64(&pos, mask))
		return static_cast<uint8_t>(pos);
	return UINT8_MAX;
}

static inline uint8_t VmaBitScanLSB(uint32_t mask)
{
	unsigned long pos;
	if (_BitScanForward(&pos, mask))
		return static_cast<uint8_t>(pos);
	return UINT8_MAX;
}

static inline uint8_t VmaBitScanMSB(uint64_t mask)
{
	unsigned long pos;
	if (_BitScanReverse64(&pos, mask))
		return static_cast<uint8_t>(pos);
	return UINT8_MAX;
}

static inline uint8_t VmaBitScanMSB(uint32_t mask)
{
	unsigned long pos;
	if (_BitScanReverse(&pos, mask))
		return static_cast<uint8_t>(pos);
	return UINT8_MAX;
}

/*
Returns true if given number is a power of two.
T must be unsigned integer number or signed integer but always nonnegative.
For 0 returns true.
*/
template <typename T>
inline bool VmaIsPow2(T x)
{
	return (x & (x - 1)) == 0;
}

// Aligns given value up to nearest multiply of align value. For example: VmaAlignUp(11, 8) = 16.
// Use types like uint32_t, uint64_t as T.
template <typename T>
static inline T VmaAlignUp(T val, T alignment)
{
	return (val + alignment - 1) & ~(alignment - 1);
}

// Aligns given value down to nearest multiply of align value. For example: VmaAlignDown(11, 8) = 8.
// Use types like uint32_t, uint64_t as T.
template <typename T>
static inline T VmaAlignDown(T val, T alignment)
{
	return val & ~(alignment - 1);
}

// Division with mathematical rounding to nearest number.
template <typename T>
static inline T VmaRoundDiv(T x, T y)
{
	return (x + (y / (T)2)) / y;
}

// Divide by 'y' and round up to nearest integer.
template <typename T>
static inline T VmaDivideRoundingUp(T x, T y)
{
	return (x + y - (T)1) / y;
}

// Returns smallest power of 2 greater or equal to v.
static inline uint32_t VmaNextPow2(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

static inline uint64_t VmaNextPow2(uint64_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	v++;
	return v;
}

// Returns largest power of 2 less or equal to v.
static inline uint32_t VmaPrevPow2(uint32_t v)
{
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v = v ^ (v >> 1);
	return v;
}

static inline uint64_t VmaPrevPow2(uint64_t v)
{
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v |= v >> 32;
	v = v ^ (v >> 1);
	return v;
}

static inline bool VmaStrIsEmpty(const char* pStr)
{
	return pStr == nullptr || *pStr == '\0';
}

/*
Returns true if two memory blocks occupy overlapping pages.
ResourceA must be in less memory offset than ResourceB.

Algorithm is based on "Vulkan 1.0.39 - A Specification (with all registered Vulkan extensions)"
chapter 11.6 "Resource Memory Association", paragraph "Buffer-Image Granularity".
*/
static inline bool VmaBlocksOnSamePage(
	VkDeviceSize resourceAOffset,
	VkDeviceSize resourceASize,
	VkDeviceSize resourceBOffset,
	VkDeviceSize pageSize)
{
	VkDeviceSize resourceAEnd = resourceAOffset + resourceASize - 1;
	VkDeviceSize resourceAEndPage = resourceAEnd & ~(pageSize - 1);
	VkDeviceSize resourceBStart = resourceBOffset;
	VkDeviceSize resourceBStartPage = resourceBStart & ~(pageSize - 1);
	return resourceAEndPage == resourceBStartPage;
}

/*
Returns true if given suballocation types could conflict and must respect
VkPhysicalDeviceLimits::bufferImageGranularity. They conflict if one is buffer
or linear image and another one is optimal image. If type is unknown, behave
conservatively.
*/
static inline bool VmaIsBufferImageGranularityConflict(
	VmaSuballocationType suballocType1,
	VmaSuballocationType suballocType2)
{
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

template <typename CmpLess, typename IterT, typename KeyT>
static IterT VmaBinaryFindFirstNotLess(IterT beg, IterT end, const KeyT& key, const CmpLess& cmp)
{
	size_t down = 0;
	size_t up = size_t(end - beg);
	while (down < up)
	{
		const size_t mid = down + (up - down) / 2;  // Overflow-safe midpoint calculation
		if (cmp(*(beg + mid), key))
		{
			down = mid + 1;
		}
		else
		{
			up = mid;
		}
	}
	return beg + down;
}

template<typename CmpLess, typename IterT, typename KeyT>
IterT VmaBinaryFindSorted(const IterT& beg, const IterT& end, const KeyT& value, const CmpLess& cmp)
{
	IterT it = VmaBinaryFindFirstNotLess<CmpLess, IterT, KeyT>(
		beg, end, value, cmp);
	if (it == end ||
		(!cmp(*it, value) && !cmp(value, *it)))
	{
		return it;
	}
	return end;
}

template<typename T>
static bool VmaValidatePointerArray(uint32_t count, const T* arr)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		const T iPtr = arr[i];
		if (iPtr == nullptr)
		{
			return false;
		}
		for (uint32_t j = i + 1; j < count; ++j)
		{
			if (iPtr == arr[j])
			{
				return false;
			}
		}
	}
	return true;
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

// An abstraction over buffer or image `usage` flags, depending on available extensions.
struct VmaBufferImageUsage
{
	typedef uint64_t BaseType; // VkFlags64

	static const VmaBufferImageUsage UNKNOWN;

	BaseType Value;

	VmaBufferImageUsage() { *this = UNKNOWN; }
	explicit VmaBufferImageUsage(BaseType usage) : Value(usage) { }
	VmaBufferImageUsage(const VkBufferCreateInfo &createInfo);
	explicit VmaBufferImageUsage(const VkImageCreateInfo &createInfo);

	bool operator==(const VmaBufferImageUsage& rhs) const { return Value == rhs.Value; }
	bool operator!=(const VmaBufferImageUsage& rhs) const { return Value != rhs.Value; }

	bool Contains(BaseType flag) const { return (Value & flag) != 0; }
	bool ContainsDeviceAccess() const
	{
		// This relies on values of VK_IMAGE_USAGE_TRANSFER* being the same as VK_BUFFER_IMAGE_TRANSFER*.
		return (Value & ~BaseType(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) != 0;
	}
};

const VmaBufferImageUsage VmaBufferImageUsage::UNKNOWN = VmaBufferImageUsage(0);

VmaBufferImageUsage::VmaBufferImageUsage(const VkBufferCreateInfo &createInfo)
{
	// If VkBufferCreateInfo::pNext chain contains VkBufferUsageFlags2CreateInfoKHR,
	// take usage from it and ignore VkBufferCreateInfo::usage, per specification
	// of the VK_KHR_maintenance5 extension.
	const VkBufferUsageFlags2CreateInfoKHR* const usageFlags2 = VmaPnextChainFind<VkBufferUsageFlags2CreateInfoKHR>(&createInfo, VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO_KHR);
	if(usageFlags2 != nullptr)
	{
		this->Value = usageFlags2->usage;
		return;
	}

	this->Value = (BaseType)createInfo.usage;
}

VmaBufferImageUsage::VmaBufferImageUsage(const VkImageCreateInfo &createInfo)
	: Value((BaseType)createInfo.usage)
{
	// Maybe in the future there will be VK_KHR_maintenanceN extension with structure
	// VkImageUsageFlags2CreateInfoKHR, like the one for buffers...
}

// This is the main algorithm that guides the selection of a memory type best for an allocation -
// converts usage to required/preferred/not preferred flags.
static bool FindMemoryPreferences(
	bool isIntegratedGPU,
	const VmaAllocationCreateInfo& allocCreateInfo,
	VmaBufferImageUsage bufImgUsage,
	VkMemoryPropertyFlags& outRequiredFlags,
	VkMemoryPropertyFlags& outPreferredFlags,
	VkMemoryPropertyFlags& outNotPreferredFlags)
{
	outRequiredFlags = allocCreateInfo.requiredFlags;
	outPreferredFlags = allocCreateInfo.preferredFlags;
	outNotPreferredFlags = 0;

	switch(allocCreateInfo.usage)
	{
	case VMA_MEMORY_USAGE_UNKNOWN:
		break;
	case VMA_MEMORY_USAGE_AUTO:
	case VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE:
	case VMA_MEMORY_USAGE_AUTO_PREFER_HOST:
	{
		const bool deviceAccess = bufImgUsage.ContainsDeviceAccess();
		const bool hostAccessSequentialWrite = (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) != 0;
		const bool hostAccessRandom = (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT) != 0;
		const bool hostAccessAllowTransferInstead = (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT) != 0;
		const bool preferDevice = allocCreateInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		const bool preferHost = allocCreateInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

		// CPU random access - e.g. a buffer written to or transferred from GPU to read back on CPU.
		if(hostAccessRandom)
		{
			// Prefer cached. Cannot require it, because some platforms don't have it (e.g. Raspberry Pi - see #362)!
			outPreferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

			if (!isIntegratedGPU && deviceAccess && hostAccessAllowTransferInstead && !preferHost)
			{
				// Nice if it will end up in HOST_VISIBLE, but more importantly prefer DEVICE_LOCAL.
				// Omitting HOST_VISIBLE here is intentional.
				// In case there is DEVICE_LOCAL | HOST_VISIBLE | HOST_CACHED, it will pick that one.
				// Otherwise, this will give same weight to DEVICE_LOCAL as HOST_VISIBLE | HOST_CACHED and select the former if occurs first on the list.
				outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			}
			else
			{
				// Always CPU memory.
				outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			}
		}
		// CPU sequential write - may be CPU or host-visible GPU memory, uncached and write-combined.
		else if(hostAccessSequentialWrite)
		{
			// Want uncached and write-combined.
			outNotPreferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

			if(!isIntegratedGPU && deviceAccess && hostAccessAllowTransferInstead && !preferHost)
			{
				outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			}
			else
			{
				outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
				// Direct GPU access, CPU sequential write (e.g. a dynamic uniform buffer updated every frame)
				if(deviceAccess)
				{
					// Could go to CPU memory or GPU BAR/unified. Up to the user to decide. If no preference, choose GPU memory.
					if(preferHost)
						outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
					else
						outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
				}
				// GPU no direct access, CPU sequential write (e.g. an upload buffer to be transferred to the GPU)
				else
				{
					// Could go to CPU memory or GPU BAR/unified. Up to the user to decide. If no preference, choose CPU memory.
					if(preferDevice)
						outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
					else
						outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
				}
			}
		}
		// No CPU access
		else
		{
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
		break;
	}
	}

	// Avoid DEVICE_COHERENT unless explicitly requested.
	if(((allocCreateInfo.requiredFlags | allocCreateInfo.preferredFlags) &
		(VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY)) == 0)
	{
		outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD_COPY;
	}

	return true;
}


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

template<typename CmpLess, typename VectorT>
size_t VmaVectorInsertSorted(VectorT& vector, const typename VectorT::value_type& value)
{
	const size_t indexToInsert = VmaBinaryFindFirstNotLess(
		vector.data(),
		vector.data() + vector.size(),
		value,
		CmpLess()) - vector.data();
	VmaVectorInsert(vector, indexToInsert, value);
	return indexToInsert;
}

template<typename CmpLess, typename VectorT>
bool VmaVectorRemoveSorted(VectorT& vector, const typename VectorT::value_type& value)
{
	CmpLess comparator;
	typename VectorT::iterator it = VmaBinaryFindFirstNotLess(
		vector.begin(),
		vector.end(),
		value,
		comparator);
	if ((it != vector.end()) && !comparator(*it, value) && !comparator(value, *it))
	{
		size_t indexToRemove = it - vector.begin();
		VmaVectorRemove(vector, indexToRemove);
		return true;
	}
	return false;
}
#endif // _VMA_FUNCTIONS

#ifndef _VMA_POOL_ALLOCATOR
/*
Allocator for objects of type T using a list of arrays (pools) to speed up
allocation. Number of elements that can be allocated is not bounded because
allocator can create multiple blocks.
*/
template<typename T>
class VmaPoolAllocator
{
public:
	VmaPoolAllocator(uint32_t firstBlockCapacity);
	~VmaPoolAllocator();
	template<typename... Types> T* Alloc(Types&&... args);
	void Free(T* ptr);

private:
	union Item
	{
		uint32_t NextFreeIndex;
		alignas(T) char Value[sizeof(T)];
	};
	struct ItemBlock
	{
		Item* pItems;
		uint32_t Capacity;
		uint32_t FirstFreeIndex;
	};

	const uint32_t m_FirstBlockCapacity;
	std::vector<ItemBlock> m_ItemBlocks;

	ItemBlock& CreateNewBlock();
};

#ifndef _VMA_POOL_ALLOCATOR_FUNCTIONS
template<typename T>
VmaPoolAllocator<T>::VmaPoolAllocator(uint32_t firstBlockCapacity) : 
	m_FirstBlockCapacity(firstBlockCapacity),
	m_ItemBlocks()
{
}

template<typename T>
VmaPoolAllocator<T>::~VmaPoolAllocator()
{
	for (size_t i = m_ItemBlocks.size(); i--;)
		vma_delete_array(m_ItemBlocks[i].pItems, m_ItemBlocks[i].Capacity);
	m_ItemBlocks.clear();
}

template<typename T>
template<typename... Types> T* VmaPoolAllocator<T>::Alloc(Types&&... args)
{
	for (size_t i = m_ItemBlocks.size(); i--; )
	{
		ItemBlock& block = m_ItemBlocks[i];
		// This block has some free items: Use first one.
		if (block.FirstFreeIndex != UINT32_MAX)
		{
			Item* const pItem = &block.pItems[block.FirstFreeIndex];
			block.FirstFreeIndex = pItem->NextFreeIndex;
			T* result = (T*)&pItem->Value;
			new(result)T(std::forward<Types>(args)...); // Explicit constructor call.
			return result;
		}
	}

	// No block has free item: Create new one and use it.
	ItemBlock& newBlock = CreateNewBlock();
	Item* const pItem = &newBlock.pItems[0];
	newBlock.FirstFreeIndex = pItem->NextFreeIndex;
	T* result = (T*)&pItem->Value;
	new(result) T(std::forward<Types>(args)...); // Explicit constructor call.
	return result;
}

template<typename T>
void VmaPoolAllocator<T>::Free(T* ptr)
{
	// Search all memory blocks to find ptr.
	for (size_t i = m_ItemBlocks.size(); i--; )
	{
		ItemBlock& block = m_ItemBlocks[i];

		// Casting to union.
		Item* pItemPtr = nullptr;
		memcpy(&pItemPtr, &ptr, sizeof(pItemPtr));

		// Check if pItemPtr is in address range of this block.
		if ((pItemPtr >= block.pItems) && (pItemPtr < block.pItems + block.Capacity))
		{
			ptr->~T(); // Explicit destructor call.
			const uint32_t index = static_cast<uint32_t>(pItemPtr - block.pItems);
			pItemPtr->NextFreeIndex = block.FirstFreeIndex;
			block.FirstFreeIndex = index;
			return;
		}
	}
}

template<typename T>
typename VmaPoolAllocator<T>::ItemBlock& VmaPoolAllocator<T>::CreateNewBlock()
{
	const uint32_t newBlockCapacity = m_ItemBlocks.empty() ?
		m_FirstBlockCapacity : m_ItemBlocks.back().Capacity * 3 / 2;

	const ItemBlock newBlock =
	{
		vma_new_array(Item, newBlockCapacity),
		newBlockCapacity,
		0
	};

	m_ItemBlocks.push_back(newBlock);

	// Setup singly-linked list of all free items in this block.
	for (uint32_t i = 0; i < newBlockCapacity - 1; ++i)
		newBlock.pItems[i].NextFreeIndex = i + 1;
	newBlock.pItems[newBlockCapacity - 1].NextFreeIndex = UINT32_MAX;
	return m_ItemBlocks.back();
}
#endif // _VMA_POOL_ALLOCATOR_FUNCTIONS
#endif // _VMA_POOL_ALLOCATOR

#ifndef _VMA_RAW_LIST
template<typename T>
struct VmaListItem
{
	VmaListItem* pPrev;
	VmaListItem* pNext;
	T Value;
};

// Doubly linked list.
template<typename T>
class VmaRawList
{
public:
	typedef VmaListItem<T> ItemType;

	explicit VmaRawList();
	// Intentionally not calling Clear, because that would be unnecessary
	// computations to return all items to m_ItemAllocator as free.
	~VmaRawList() = default;

	size_t GetCount() const { return m_Count; }
	bool IsEmpty() const { return m_Count == 0; }

	ItemType* Front() { return m_pFront; }
	ItemType* Back() { return m_pBack; }
	const ItemType* Front() const { return m_pFront; }
	const ItemType* Back() const { return m_pBack; }

	ItemType* PushFront();
	ItemType* PushBack();
	ItemType* PushFront(const T& value);
	ItemType* PushBack(const T& value);
	void PopFront();
	void PopBack();

	// Item can be null - it means PushBack.
	ItemType* InsertBefore(ItemType* pItem);
	// Item can be null - it means PushFront.
	ItemType* InsertAfter(ItemType* pItem);
	ItemType* InsertBefore(ItemType* pItem, const T& value);
	ItemType* InsertAfter(ItemType* pItem, const T& value);

	void Clear();
	void Remove(ItemType* pItem);

private:
	VmaPoolAllocator<ItemType> m_ItemAllocator;
	ItemType* m_pFront;
	ItemType* m_pBack;
	size_t m_Count;
};

#ifndef _VMA_RAW_LIST_FUNCTIONS
template<typename T>
VmaRawList<T>::VmaRawList() : 
	m_ItemAllocator(128),
	m_pFront(nullptr),
	m_pBack(nullptr),
	m_Count(0) {}

template<typename T>
VmaListItem<T>* VmaRawList<T>::PushFront()
{
	ItemType* const pNewItem = m_ItemAllocator.Alloc();
	pNewItem->pPrev = nullptr;
	if (IsEmpty())
	{
		pNewItem->pNext = nullptr;
		m_pFront = pNewItem;
		m_pBack = pNewItem;
		m_Count = 1;
	}
	else
	{
		pNewItem->pNext = m_pFront;
		m_pFront->pPrev = pNewItem;
		m_pFront = pNewItem;
		++m_Count;
	}
	return pNewItem;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::PushBack()
{
	ItemType* const pNewItem = m_ItemAllocator.Alloc();
	pNewItem->pNext = nullptr;
	if(IsEmpty())
	{
		pNewItem->pPrev = nullptr;
		m_pFront = pNewItem;
		m_pBack = pNewItem;
		m_Count = 1;
	}
	else
	{
		pNewItem->pPrev = m_pBack;
		m_pBack->pNext = pNewItem;
		m_pBack = pNewItem;
		++m_Count;
	}
	return pNewItem;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::PushFront(const T& value)
{
	ItemType* const pNewItem = PushFront();
	pNewItem->Value = value;
	return pNewItem;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::PushBack(const T& value)
{
	ItemType* const pNewItem = PushBack();
	pNewItem->Value = value;
	return pNewItem;
}

template<typename T>
void VmaRawList<T>::PopFront()
{
	ItemType* const pFrontItem = m_pFront;
	ItemType* const pNextItem = pFrontItem->pNext;
	if (pNextItem != nullptr)
	{
		pNextItem->pPrev = nullptr;
	}
	m_pFront = pNextItem;
	m_ItemAllocator.Free(pFrontItem);
	--m_Count;
}

template<typename T>
void VmaRawList<T>::PopBack()
{
	ItemType* const pBackItem = m_pBack;
	ItemType* const pPrevItem = pBackItem->pPrev;
	if(pPrevItem != nullptr)
	{
		pPrevItem->pNext = nullptr;
	}
	m_pBack = pPrevItem;
	m_ItemAllocator.Free(pBackItem);
	--m_Count;
}

template<typename T>
void VmaRawList<T>::Clear()
{
	if (!IsEmpty())
	{
		ItemType* pItem = m_pBack;
		while (pItem != nullptr)
		{
			ItemType* const pPrevItem = pItem->pPrev;
			m_ItemAllocator.Free(pItem);
			pItem = pPrevItem;
		}
		m_pFront = nullptr;
		m_pBack = nullptr;
		m_Count = 0;
	}
}

template<typename T>
void VmaRawList<T>::Remove(ItemType* pItem)
{
	if(pItem->pPrev != nullptr)
	{
		pItem->pPrev->pNext = pItem->pNext;
	}
	else
	{
		m_pFront = pItem->pNext;
	}

	if(pItem->pNext != nullptr)
	{
		pItem->pNext->pPrev = pItem->pPrev;
	}
	else
	{
		m_pBack = pItem->pPrev;
	}

	m_ItemAllocator.Free(pItem);
	--m_Count;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::InsertBefore(ItemType* pItem)
{
	if(pItem != nullptr)
	{
		ItemType* const prevItem = pItem->pPrev;
		ItemType* const newItem = m_ItemAllocator.Alloc();
		newItem->pPrev = prevItem;
		newItem->pNext = pItem;
		pItem->pPrev = newItem;
		if(prevItem != nullptr)
		{
			prevItem->pNext = newItem;
		}
		else
		{
			m_pFront = newItem;
		}
		++m_Count;
		return newItem;
	}
	return PushBack();
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::InsertAfter(ItemType* pItem)
{
	if(pItem != nullptr)
	{
		ItemType* const nextItem = pItem->pNext;
		ItemType* const newItem = m_ItemAllocator.Alloc();
		newItem->pNext = nextItem;
		newItem->pPrev = pItem;
		pItem->pNext = newItem;
		if(nextItem != nullptr)
		{
			nextItem->pPrev = newItem;
		}
		else
		{
			m_pBack = newItem;
		}
		++m_Count;
		return newItem;
	}
	return PushFront();
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::InsertBefore(ItemType* pItem, const T& value)
{
	ItemType* const newItem = InsertBefore(pItem);
	newItem->Value = value;
	return newItem;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::InsertAfter(ItemType* pItem, const T& value)
{
	ItemType* const newItem = InsertAfter(pItem);
	newItem->Value = value;
	return newItem;
}
#endif // _VMA_RAW_LIST_FUNCTIONS
#endif // _VMA_RAW_LIST

#ifndef _VMA_LIST
template<typename T>
class VmaList
{
public:
	class reverse_iterator;
	class const_iterator;
	class const_reverse_iterator;

	class iterator
	{
		friend class const_iterator;
	public:
		iterator() :  m_pList(nullptr), m_pItem(nullptr) {}
		explicit iterator(const reverse_iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

		T& operator*() const { return m_pItem->Value; }
		T* operator->() const { return &m_pItem->Value; }

		bool operator==(const iterator& rhs) const { return m_pItem == rhs.m_pItem; }
		bool operator!=(const iterator& rhs) const { return m_pItem != rhs.m_pItem; }

		const iterator operator++(int) { iterator result = *this; ++*this; return result; }
		const iterator operator--(int) { iterator result = *this; --*this; return result; }

		iterator& operator++() { m_pItem = m_pItem->pNext; return *this; }
		iterator& operator--();

	private:
		VmaRawList<T>* m_pList;
		VmaListItem<T>* m_pItem;

		iterator(VmaRawList<T>* pList, VmaListItem<T>* pItem) : m_pList(pList),  m_pItem(pItem) {}
	};
	class reverse_iterator
	{
		friend class const_reverse_iterator;
	public:
		reverse_iterator() : m_pList(nullptr), m_pItem(nullptr) {}
		explicit reverse_iterator(const iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

		T& operator*() const { return m_pItem->Value; }
		T* operator->() const { return &m_pItem->Value; }

		bool operator==(const reverse_iterator& rhs) const { return m_pItem == rhs.m_pItem; }
		bool operator!=(const reverse_iterator& rhs) const { return m_pItem != rhs.m_pItem; }

		const reverse_iterator operator++(int) { reverse_iterator result = *this; ++* this; return result; }
		const reverse_iterator operator--(int) { reverse_iterator result = *this; --* this; return result; }

		reverse_iterator& operator++() { m_pItem = m_pItem->pPrev; return *this; }
		reverse_iterator& operator--();

	private:
		VmaRawList<T>* m_pList;
		VmaListItem<T>* m_pItem;

		reverse_iterator(VmaRawList<T>* pList, VmaListItem<T>* pItem) : m_pList(pList),  m_pItem(pItem) {}
	};
	class const_iterator
	{
	public:
		const_iterator() : m_pList(nullptr), m_pItem(nullptr) {}
		explicit const_iterator(const iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}
		explicit const_iterator(const reverse_iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

		iterator drop_const() { return { const_cast<VmaRawList<T>*>(m_pList), const_cast<VmaListItem<T>*>(m_pItem) }; }

		const T& operator*() const { return m_pItem->Value; }
		const T* operator->() const { return &m_pItem->Value; }

		bool operator==(const const_iterator& rhs) const { return m_pItem == rhs.m_pItem; }
		bool operator!=(const const_iterator& rhs) const { return m_pItem != rhs.m_pItem; }

		const const_iterator operator++(int) { const_iterator result = *this; ++* this; return result; }
		const const_iterator operator--(int) { const_iterator result = *this; --* this; return result; }

		const_iterator& operator++() { m_pItem = m_pItem->pNext; return *this; }
		const_iterator& operator--();

	private:
		const VmaRawList<T>* m_pList;
		const VmaListItem<T>* m_pItem;

		const_iterator(const VmaRawList<T>* pList, const VmaListItem<T>* pItem) : m_pList(pList), m_pItem(pItem) {}
	};
	class const_reverse_iterator
	{
		friend class VmaList<T>;
	public:
		const_reverse_iterator() : m_pList(nullptr), m_pItem(nullptr) {}
		explicit const_reverse_iterator(const reverse_iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}
		explicit const_reverse_iterator(const iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

		reverse_iterator drop_const() { return { const_cast<VmaRawList<T>*>(m_pList), const_cast<VmaListItem<T>*>(m_pItem) }; }

		const T& operator*() const { return m_pItem->Value; }
		const T* operator->() const { return &m_pItem->Value; }

		bool operator==(const const_reverse_iterator& rhs) const { return m_pItem == rhs.m_pItem; }
		bool operator!=(const const_reverse_iterator& rhs) const { return m_pItem != rhs.m_pItem; }

		const const_reverse_iterator operator++(int) { const_reverse_iterator result = *this; ++* this; return result; }
		const const_reverse_iterator operator--(int) { const_reverse_iterator result = *this; --* this; return result; }

		const_reverse_iterator& operator++() { m_pItem = m_pItem->pPrev; return *this; }
		const_reverse_iterator& operator--();

	private:
		const VmaRawList<T>* m_pList;
		const VmaListItem<T>* m_pItem;

		const_reverse_iterator(const VmaRawList<T>* pList, const VmaListItem<T>* pItem) : m_pList(pList), m_pItem(pItem) {}
	};

	explicit VmaList() : m_RawList() {}

	bool empty() const { return m_RawList.IsEmpty(); }
	size_t size() const { return m_RawList.GetCount(); }

	iterator begin() { return iterator(&m_RawList, m_RawList.Front()); }
	iterator end() { return iterator(&m_RawList, nullptr); }

	const_iterator cbegin() const { return const_iterator(&m_RawList, m_RawList.Front()); }
	const_iterator cend() const { return const_iterator(&m_RawList, nullptr); }

	const_iterator begin() const { return cbegin(); }
	const_iterator end() const { return cend(); }

	reverse_iterator rbegin() { return reverse_iterator(&m_RawList, m_RawList.Back()); }
	reverse_iterator rend() { return reverse_iterator(&m_RawList, nullptr); }

	const_reverse_iterator crbegin() const { return const_reverse_iterator(&m_RawList, m_RawList.Back()); }
	const_reverse_iterator crend() const { return const_reverse_iterator(&m_RawList, nullptr); }

	const_reverse_iterator rbegin() const { return crbegin(); }
	const_reverse_iterator rend() const { return crend(); }

	void push_back(const T& value) { m_RawList.PushBack(value); }
	iterator insert(iterator it, const T& value) { return iterator(&m_RawList, m_RawList.InsertBefore(it.m_pItem, value)); }

	void clear() { m_RawList.Clear(); }
	void erase(iterator it) { m_RawList.Remove(it.m_pItem); }

private:
	VmaRawList<T> m_RawList;
};

#ifndef _VMA_LIST_FUNCTIONS
template<typename T>
typename VmaList<T>::iterator& VmaList<T>::iterator::operator--()
{
	if (m_pItem != nullptr)
	{
		m_pItem = m_pItem->pPrev;
	}
	else
	{
		m_pItem = m_pList->Back();
	}
	return *this;
}

template<typename T>
typename VmaList<T>::reverse_iterator& VmaList<T>::reverse_iterator::operator--()
{
	if (m_pItem != nullptr)
	{
		m_pItem = m_pItem->pNext;
	}
	else
	{
		m_pItem = m_pList->Front();
	}
	return *this;
}

template<typename T>
typename VmaList<T>::const_iterator& VmaList<T>::const_iterator::operator--()
{
	if (m_pItem != nullptr)
	{
		m_pItem = m_pItem->pPrev;
	}
	else
	{
		m_pItem = m_pList->Back();
	}
	return *this;
}

template<typename T>
typename VmaList<T>::const_reverse_iterator& VmaList<T>::const_reverse_iterator::operator--()
{
	if (m_pItem != nullptr)
	{
		m_pItem = m_pItem->pNext;
	}
	else
	{
		m_pItem = m_pList->Back();
	}
	return *this;
}
#endif // _VMA_LIST_FUNCTIONS
#endif // _VMA_LIST

#ifndef _VMA_INTRUSIVE_LINKED_LIST
/*
Expected interface of ItemTypeTraits:
struct MyItemTypeTraits
{
	typedef MyItem ItemType;
	static ItemType* GetPrev(const ItemType* item) { return item->myPrevPtr; }
	static ItemType* GetNext(const ItemType* item) { return item->myNextPtr; }
	static ItemType*& AccessPrev(ItemType* item) { return item->myPrevPtr; }
	static ItemType*& AccessNext(ItemType* item) { return item->myNextPtr; }
};
*/
template<typename ItemTypeTraits>
class VmaIntrusiveLinkedList
{
public:
	typedef typename ItemTypeTraits::ItemType ItemType;
	static ItemType* GetPrev(const ItemType* item) { return ItemTypeTraits::GetPrev(item); }
	static ItemType* GetNext(const ItemType* item) { return ItemTypeTraits::GetNext(item); }

	// Movable, not copyable.
	VmaIntrusiveLinkedList() = default;
	VmaIntrusiveLinkedList(VmaIntrusiveLinkedList && src) noexcept;
	VmaIntrusiveLinkedList(const VmaIntrusiveLinkedList&) = delete;
	VmaIntrusiveLinkedList& operator=(VmaIntrusiveLinkedList&& src) noexcept;
	VmaIntrusiveLinkedList& operator=(const VmaIntrusiveLinkedList&) = delete;
	~VmaIntrusiveLinkedList() { }

	size_t GetCount() const { return m_Count; }
	bool IsEmpty() const { return m_Count == 0; }
	ItemType* Front() { return m_Front; }
	ItemType* Back() { return m_Back; }
	const ItemType* Front() const { return m_Front; }
	const ItemType* Back() const { return m_Back; }

	void PushBack(ItemType* item);
	void PushFront(ItemType* item);
	ItemType* PopBack();
	ItemType* PopFront();

	// MyItem can be null - it means PushBack.
	void InsertBefore(ItemType* existingItem, ItemType* newItem);
	// MyItem can be null - it means PushFront.
	void InsertAfter(ItemType* existingItem, ItemType* newItem);
	void Remove(ItemType* item);
	void RemoveAll();

private:
	ItemType* m_Front = nullptr;
	ItemType* m_Back = nullptr;
	size_t m_Count = 0;
};

#ifndef _VMA_INTRUSIVE_LINKED_LIST_FUNCTIONS
template<typename ItemTypeTraits>
VmaIntrusiveLinkedList<ItemTypeTraits>::VmaIntrusiveLinkedList(VmaIntrusiveLinkedList&& src) noexcept
	: m_Front(src.m_Front), m_Back(src.m_Back), m_Count(src.m_Count)
{
	src.m_Front = src.m_Back = nullptr;
	src.m_Count = 0;
}

template<typename ItemTypeTraits>
VmaIntrusiveLinkedList<ItemTypeTraits>& VmaIntrusiveLinkedList<ItemTypeTraits>::operator=(VmaIntrusiveLinkedList&& src) noexcept
{
	if (&src != this)
	{
		m_Front = src.m_Front;
		m_Back = src.m_Back;
		m_Count = src.m_Count;
		src.m_Front = src.m_Back = nullptr;
		src.m_Count = 0;
	}
	return *this;
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::PushBack(ItemType* item)
{
	if (IsEmpty())
	{
		m_Front = item;
		m_Back = item;
		m_Count = 1;
	}
	else
	{
		ItemTypeTraits::AccessPrev(item) = m_Back;
		ItemTypeTraits::AccessNext(m_Back) = item;
		m_Back = item;
		++m_Count;
	}
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::PushFront(ItemType* item)
{
	if (IsEmpty())
	{
		m_Front = item;
		m_Back = item;
		m_Count = 1;
	}
	else
	{
		ItemTypeTraits::AccessNext(item) = m_Front;
		ItemTypeTraits::AccessPrev(m_Front) = item;
		m_Front = item;
		++m_Count;
	}
}

template<typename ItemTypeTraits>
typename VmaIntrusiveLinkedList<ItemTypeTraits>::ItemType* VmaIntrusiveLinkedList<ItemTypeTraits>::PopBack()
{
	ItemType* const backItem = m_Back;
	ItemType* const prevItem = ItemTypeTraits::GetPrev(backItem);
	if (prevItem != nullptr)
	{
		ItemTypeTraits::AccessNext(prevItem) = nullptr;
	}
	m_Back = prevItem;
	--m_Count;
	ItemTypeTraits::AccessPrev(backItem) = nullptr;
	ItemTypeTraits::AccessNext(backItem) = nullptr;
	return backItem;
}

template<typename ItemTypeTraits>
typename VmaIntrusiveLinkedList<ItemTypeTraits>::ItemType* VmaIntrusiveLinkedList<ItemTypeTraits>::PopFront()
{
	ItemType* const frontItem = m_Front;
	ItemType* const nextItem = ItemTypeTraits::GetNext(frontItem);
	if (nextItem != nullptr)
	{
		ItemTypeTraits::AccessPrev(nextItem) = nullptr;
	}
	m_Front = nextItem;
	--m_Count;
	ItemTypeTraits::AccessPrev(frontItem) = nullptr;
	ItemTypeTraits::AccessNext(frontItem) = nullptr;
	return frontItem;
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::InsertBefore(ItemType* existingItem, ItemType* newItem)
{
	if (existingItem != nullptr)
	{
		ItemType* const prevItem = ItemTypeTraits::GetPrev(existingItem);
		ItemTypeTraits::AccessPrev(newItem) = prevItem;
		ItemTypeTraits::AccessNext(newItem) = existingItem;
		ItemTypeTraits::AccessPrev(existingItem) = newItem;
		if (prevItem != nullptr)
		{
			ItemTypeTraits::AccessNext(prevItem) = newItem;
		}
		else
		{
			m_Front = newItem;
		}
		++m_Count;
	}
	else
		PushBack(newItem);
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::InsertAfter(ItemType* existingItem, ItemType* newItem)
{
	if (existingItem != nullptr)
	{
		ItemType* const nextItem = ItemTypeTraits::GetNext(existingItem);
		ItemTypeTraits::AccessNext(newItem) = nextItem;
		ItemTypeTraits::AccessPrev(newItem) = existingItem;
		ItemTypeTraits::AccessNext(existingItem) = newItem;
		if (nextItem != nullptr)
		{
			ItemTypeTraits::AccessPrev(nextItem) = newItem;
		}
		else
		{
			m_Back = newItem;
		}
		++m_Count;
	}
	else
		return PushFront(newItem);
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::Remove(ItemType* item)
{
	if (ItemTypeTraits::GetPrev(item) != nullptr)
	{
		ItemTypeTraits::AccessNext(ItemTypeTraits::AccessPrev(item)) = ItemTypeTraits::GetNext(item);
	}
	else
	{
		m_Front = ItemTypeTraits::GetNext(item);
	}

	if (ItemTypeTraits::GetNext(item) != nullptr)
	{
		ItemTypeTraits::AccessPrev(ItemTypeTraits::AccessNext(item)) = ItemTypeTraits::GetPrev(item);
	}
	else
	{
		m_Back = ItemTypeTraits::GetPrev(item);
	}
	ItemTypeTraits::AccessPrev(item) = nullptr;
	ItemTypeTraits::AccessNext(item) = nullptr;
	--m_Count;
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::RemoveAll()
{
	if (!IsEmpty())
	{
		ItemType* item = m_Back;
		while (item != nullptr)
		{
			ItemType* const prevItem = ItemTypeTraits::AccessPrev(item);
			ItemTypeTraits::AccessPrev(item) = nullptr;
			ItemTypeTraits::AccessNext(item) = nullptr;
			item = prevItem;
		}
		m_Front = nullptr;
		m_Back = nullptr;
		m_Count = 0;
	}
}
#endif // _VMA_INTRUSIVE_LINKED_LIST_FUNCTIONS
#endif // _VMA_INTRUSIVE_LINKED_LIST

#ifndef _VMA_DEVICE_MEMORY_BLOCK

class VmaDeviceMemoryBlock
{
public:
	VmaBlockMetadata* m_pMetadata;

	explicit VmaDeviceMemoryBlock(VmaAllocator hAllocator);
	~VmaDeviceMemoryBlock();

	// Always call after construction.
	void Init(
		uint32_t newMemoryTypeIndex,
		VkDeviceMemory newMemory,
		VkDeviceSize newSize,
		uint32_t id,
		uint32_t algorithm,
		VkDeviceSize bufferImageGranularity);
	// Always call before destruction.
	void Destroy(VmaAllocator allocator);

	VkDeviceMemory GetDeviceMemory() const { return m_hMemory; }
	uint32_t GetMemoryTypeIndex() const { return m_MemoryTypeIndex; }
	uint32_t GetId() const { return m_Id; }

private:
	uint32_t m_MemoryTypeIndex;
	uint32_t m_Id;
	VkDeviceMemory m_hMemory;
};
#endif // _VMA_DEVICE_MEMORY_BLOCK

#ifndef _VMA_ALLOCATION_T
struct VmaAllocation_T
{
	friend struct VmaDedicatedAllocationListItemTraits;

public:
	enum ALLOCATION_TYPE
	{
		ALLOCATION_TYPE_NONE,
		ALLOCATION_TYPE_BLOCK,
		ALLOCATION_TYPE_DEDICATED,
	};

	// This struct is allocated using VmaPoolAllocator.
	explicit VmaAllocation_T();
	~VmaAllocation_T();

	void InitBlockAllocation(
		VmaDeviceMemoryBlock* block,
		VmaAllocHandle allocHandle,
		VkDeviceSize alignment,
		VkDeviceSize size,
		uint32_t memoryTypeIndex,
		VmaSuballocationType suballocationType
	);
	void InitDedicatedAllocation(
		uint32_t memoryTypeIndex,
		VkDeviceMemory hMemory,
		VmaSuballocationType suballocationType,
		VkDeviceSize size);
	void Destroy();

	ALLOCATION_TYPE GetType() const { return (ALLOCATION_TYPE)m_Type; }
	VkDeviceSize GetAlignment() const { return m_Alignment; }
	VkDeviceSize GetSize() const { return m_Size; }
	VmaSuballocationType GetSuballocationType() const { return (VmaSuballocationType)m_SuballocationType; }

	VmaDeviceMemoryBlock* GetBlock() const { return m_BlockAllocation.m_Block; }
	uint32_t GetMemoryTypeIndex() const { return m_MemoryTypeIndex; }

	void SwapBlockAllocation(VmaAllocation allocation);
	VmaAllocHandle GetAllocHandle() const;
	VkDeviceSize GetOffset() const;
	VkDeviceMemory GetMemory() const;

private:
	// Allocation out of VmaDeviceMemoryBlock.
	struct BlockAllocation
	{
		VmaDeviceMemoryBlock* m_Block;
		VmaAllocHandle m_AllocHandle;
	};
	// Allocation for an object that has its own private VkDeviceMemory.
	struct DedicatedAllocation
	{
		VkDeviceMemory m_hMemory;
		VmaAllocation_T* m_Prev;
		VmaAllocation_T* m_Next;
	};
	union
	{
		// Allocation out of VmaDeviceMemoryBlock.
		BlockAllocation m_BlockAllocation;
		// Allocation for an object that has its own private VkDeviceMemory.
		DedicatedAllocation m_DedicatedAllocation;
	};

	VkDeviceSize m_Alignment;
	VkDeviceSize m_Size;
	uint32_t m_MemoryTypeIndex;
	uint8_t m_Type; // ALLOCATION_TYPE
	uint8_t m_SuballocationType; // VmaSuballocationType
	uint8_t m_Flags; // enum FLAGS
};
#endif // _VMA_ALLOCATION_T

#ifndef _VMA_DEDICATED_ALLOCATION_LIST_ITEM_TRAITS
struct VmaDedicatedAllocationListItemTraits
{
	typedef VmaAllocation_T ItemType;

	static ItemType* GetPrev(const ItemType* item)
	{
		return item->m_DedicatedAllocation.m_Prev;
	}
	static ItemType* GetNext(const ItemType* item)
	{
		return item->m_DedicatedAllocation.m_Next;
	}
	static ItemType*& AccessPrev(ItemType* item)
	{
		return item->m_DedicatedAllocation.m_Prev;
	}
	static ItemType*& AccessNext(ItemType* item)
	{
		return item->m_DedicatedAllocation.m_Next;
	}
};
#endif // _VMA_DEDICATED_ALLOCATION_LIST_ITEM_TRAITS

#ifndef _VMA_DEDICATED_ALLOCATION_LIST
/*
Stores linked list of VmaAllocation_T objects.
Thread-safe, synchronized internally.
*/
class VmaDedicatedAllocationList
{
public:
	VmaDedicatedAllocationList() = default;
	~VmaDedicatedAllocationList();

	void Init() {}

	bool IsEmpty();
	void Register(VmaAllocation alloc);
	void Unregister(VmaAllocation alloc);

private:
	typedef VmaIntrusiveLinkedList<VmaDedicatedAllocationListItemTraits> DedicatedAllocationLinkedList;
	DedicatedAllocationLinkedList m_AllocationList;
};

#ifndef _VMA_DEDICATED_ALLOCATION_LIST_FUNCTIONS

VmaDedicatedAllocationList::~VmaDedicatedAllocationList()
{
}

bool VmaDedicatedAllocationList::IsEmpty()
{
	return m_AllocationList.IsEmpty();
}

void VmaDedicatedAllocationList::Register(VmaAllocation alloc)
{
	m_AllocationList.PushBack(alloc);
}

void VmaDedicatedAllocationList::Unregister(VmaAllocation alloc)
{
	m_AllocationList.Remove(alloc);
}
#endif // _VMA_DEDICATED_ALLOCATION_LIST_FUNCTIONS
#endif // _VMA_DEDICATED_ALLOCATION_LIST

#ifndef _VMA_SUBALLOCATION
/*
Represents a region of VmaDeviceMemoryBlock that is either assigned and returned as
allocated memory block or free.
*/
struct VmaSuballocation
{
	VkDeviceSize offset;
	VkDeviceSize size;
	void* userData;
	VmaSuballocationType type;
};

// Comparator for offsets.
struct VmaSuballocationOffsetLess
{
	bool operator()(const VmaSuballocation& lhs, const VmaSuballocation& rhs) const
	{
		return lhs.offset < rhs.offset;
	}
};

struct VmaSuballocationOffsetGreater
{
	bool operator()(const VmaSuballocation& lhs, const VmaSuballocation& rhs) const
	{
		return lhs.offset > rhs.offset;
	}
};

struct VmaSuballocationItemSizeLess
{
	bool operator()(const VmaSuballocationList::iterator lhs,
		const VmaSuballocationList::iterator rhs) const
	{
		return lhs->size < rhs->size;
	}

	bool operator()(const VmaSuballocationList::iterator lhs,
		VkDeviceSize rhsSize) const
	{
		return lhs->size < rhsSize;
	}
};
#endif // _VMA_SUBALLOCATION

#ifndef _VMA_ALLOCATION_REQUEST
/*
Parameters of planned allocation inside a VmaDeviceMemoryBlock.
item points to a FREE suballocation.
*/
struct VmaAllocationRequest
{
	VmaAllocHandle allocHandle;
	VkDeviceSize size;
	VmaSuballocationList::iterator item;
	void* customData;
	uint64_t algorithmData;
	VmaAllocationRequestType type;
};
#endif // _VMA_ALLOCATION_REQUEST

#ifndef _VMA_BLOCK_METADATA
/*
Data structure used for bookkeeping of allocations and unused ranges of memory
in a single VkDeviceMemory block.
*/
class VmaBlockMetadata
{
public:
	// pAllocationCallbacks, if not null, must be owned externally - alive and unchanged for the whole lifetime of this object.
	VmaBlockMetadata(VkDeviceSize bufferImageGranularity, bool isVirtual);
	virtual ~VmaBlockMetadata() = default;

	virtual void Init(VkDeviceSize size) { m_Size = size; }
	bool IsVirtual() const { return m_IsVirtual; }
	VkDeviceSize GetSize() const { return m_Size; }

	virtual size_t GetAllocationCount() const = 0;
	virtual VkDeviceSize GetSumFreeSize() const = 0;
	// Returns true if this block is empty - contains only single free suballocation.
	virtual bool IsEmpty() const = 0;
	virtual VkDeviceSize GetAllocationOffset(VmaAllocHandle allocHandle) const = 0;

	// Tries to find a place for suballocation with given parameters inside this block.
	// If succeeded, fills pAllocationRequest and returns true.
	// If failed, returns false.
	virtual bool CreateAllocationRequest(
		VkDeviceSize allocSize,
		VkDeviceSize allocAlignment,
		VmaSuballocationType allocType,
		// Always one of VMA_ALLOCATION_CREATE_STRATEGY_* or VMA_ALLOCATION_INTERNAL_STRATEGY_* flags.
		uint32_t strategy,
		VmaAllocationRequest* pAllocationRequest) = 0;

	// Makes actual allocation based on request. Request must already be checked and valid.
	virtual void Alloc(
		const VmaAllocationRequest& request,
		VmaSuballocationType type,
		void* userData) = 0;

	// Frees suballocation assigned to given memory region.
	virtual void Free(VmaAllocHandle allocHandle) = 0;

	// Frees all allocations.
	// Careful! Don't call it if there are VmaAllocation objects owned by userData of cleared allocations!
	virtual void Clear() = 0;

	virtual void SetAllocationUserData(VmaAllocHandle allocHandle, void* userData) = 0;

protected:
	VkDeviceSize GetBufferImageGranularity() const { return m_BufferImageGranularity; }

private:
	VkDeviceSize m_Size;
	const VkDeviceSize m_BufferImageGranularity;
	const bool m_IsVirtual;
};

#ifndef _VMA_BLOCK_METADATA_FUNCTIONS
VmaBlockMetadata::VmaBlockMetadata(VkDeviceSize bufferImageGranularity, bool isVirtual)
	: m_Size(0),
	m_BufferImageGranularity(bufferImageGranularity),
	m_IsVirtual(isVirtual) {}

#endif // _VMA_BLOCK_METADATA_FUNCTIONS
#endif // _VMA_BLOCK_METADATA

#ifndef _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY
// Before deleting object of this class remember to call 'Destroy()'
class VmaBlockBufferImageGranularity final
{
public:
	struct ValidationContext
	{
		uint16_t* pageAllocs;
	};

	explicit VmaBlockBufferImageGranularity(VkDeviceSize bufferImageGranularity);
	~VmaBlockBufferImageGranularity();

	bool IsEnabled() const { return m_BufferImageGranularity > MAX_LOW_BUFFER_IMAGE_GRANULARITY; }

	void Init(VkDeviceSize size);
	// Before destroying object you must call free it's memory
	void Destroy();

	void RoundupAllocRequest(VmaSuballocationType allocType,
		VkDeviceSize& inOutAllocSize,
		VkDeviceSize& inOutAllocAlignment) const;

	bool CheckConflictAndAlignUp(VkDeviceSize& inOutAllocOffset,
		VkDeviceSize allocSize,
		VkDeviceSize blockOffset,
		VkDeviceSize blockSize,
		VmaSuballocationType allocType) const;

	void AllocPages(uint8_t allocType, VkDeviceSize offset, VkDeviceSize size);
	void FreePages(VkDeviceSize offset, VkDeviceSize size);
	void Clear();

private:
	static const uint16_t MAX_LOW_BUFFER_IMAGE_GRANULARITY = 256;

	struct RegionInfo
	{
		uint8_t allocType;
		uint16_t allocCount;
	};

	VkDeviceSize m_BufferImageGranularity;
	uint32_t m_RegionCount;
	RegionInfo* m_RegionInfo;

	uint32_t GetStartPage(VkDeviceSize offset) const { return OffsetToPageIndex(offset & ~(m_BufferImageGranularity - 1)); }
	uint32_t GetEndPage(VkDeviceSize offset, VkDeviceSize size) const { return OffsetToPageIndex((offset + size - 1) & ~(m_BufferImageGranularity - 1)); }

	uint32_t OffsetToPageIndex(VkDeviceSize offset) const;
	static void AllocPage(RegionInfo& page, uint8_t allocType);
};

#ifndef _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY_FUNCTIONS
VmaBlockBufferImageGranularity::VmaBlockBufferImageGranularity(VkDeviceSize bufferImageGranularity)
	: m_BufferImageGranularity(bufferImageGranularity),
	m_RegionCount(0),
	m_RegionInfo(nullptr) {}

VmaBlockBufferImageGranularity::~VmaBlockBufferImageGranularity()
{
}

void VmaBlockBufferImageGranularity::Init(VkDeviceSize size)
{
	if (IsEnabled())
	{
		m_RegionCount = static_cast<uint32_t>(VmaDivideRoundingUp(size, m_BufferImageGranularity));
		m_RegionInfo = vma_new_array(RegionInfo, m_RegionCount);
		memset(m_RegionInfo, 0, m_RegionCount * sizeof(RegionInfo));
	}
}

void VmaBlockBufferImageGranularity::Destroy()
{
	if (m_RegionInfo)
	{
		vma_delete_array(m_RegionInfo, m_RegionCount);
		m_RegionInfo = nullptr;
	}
}

void VmaBlockBufferImageGranularity::RoundupAllocRequest(VmaSuballocationType allocType,
	VkDeviceSize& inOutAllocSize,
	VkDeviceSize& inOutAllocAlignment) const
{
	if (m_BufferImageGranularity > 1 &&
		m_BufferImageGranularity <= MAX_LOW_BUFFER_IMAGE_GRANULARITY)
	{
		if (allocType == VMA_SUBALLOCATION_TYPE_UNKNOWN ||
			allocType == VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN ||
			allocType == VMA_SUBALLOCATION_TYPE_IMAGE_OPTIMAL)
		{
			inOutAllocAlignment = VMA_MAX(inOutAllocAlignment, m_BufferImageGranularity);
			inOutAllocSize = VmaAlignUp(inOutAllocSize, m_BufferImageGranularity);
		}
	}
}

bool VmaBlockBufferImageGranularity::CheckConflictAndAlignUp(VkDeviceSize& inOutAllocOffset,
	VkDeviceSize allocSize,
	VkDeviceSize blockOffset,
	VkDeviceSize blockSize,
	VmaSuballocationType allocType) const
{
	if (IsEnabled())
	{
		uint32_t startPage = GetStartPage(inOutAllocOffset);
		if (m_RegionInfo[startPage].allocCount > 0 &&
			VmaIsBufferImageGranularityConflict(static_cast<VmaSuballocationType>(m_RegionInfo[startPage].allocType), allocType))
		{
			inOutAllocOffset = VmaAlignUp(inOutAllocOffset, m_BufferImageGranularity);
			if (blockSize < allocSize + inOutAllocOffset - blockOffset)
				return true;
			++startPage;
		}
		uint32_t endPage = GetEndPage(inOutAllocOffset, allocSize);
		if (endPage != startPage &&
			m_RegionInfo[endPage].allocCount > 0 &&
			VmaIsBufferImageGranularityConflict(static_cast<VmaSuballocationType>(m_RegionInfo[endPage].allocType), allocType))
		{
			return true;
		}
	}
	return false;
}

void VmaBlockBufferImageGranularity::AllocPages(uint8_t allocType, VkDeviceSize offset, VkDeviceSize size)
{
	if (IsEnabled())
	{
		uint32_t startPage = GetStartPage(offset);
		AllocPage(m_RegionInfo[startPage], allocType);

		uint32_t endPage = GetEndPage(offset, size);
		if (startPage != endPage)
			AllocPage(m_RegionInfo[endPage], allocType);
	}
}

void VmaBlockBufferImageGranularity::FreePages(VkDeviceSize offset, VkDeviceSize size)
{
	if (IsEnabled())
	{
		uint32_t startPage = GetStartPage(offset);
		--m_RegionInfo[startPage].allocCount;
		if (m_RegionInfo[startPage].allocCount == 0)
			m_RegionInfo[startPage].allocType = VMA_SUBALLOCATION_TYPE_FREE;
		uint32_t endPage = GetEndPage(offset, size);
		if (startPage != endPage)
		{
			--m_RegionInfo[endPage].allocCount;
			if (m_RegionInfo[endPage].allocCount == 0)
				m_RegionInfo[endPage].allocType = VMA_SUBALLOCATION_TYPE_FREE;
		}
	}
}

void VmaBlockBufferImageGranularity::Clear()
{
	if (m_RegionInfo)
		memset(m_RegionInfo, 0, m_RegionCount * sizeof(RegionInfo));
}

uint32_t VmaBlockBufferImageGranularity::OffsetToPageIndex(VkDeviceSize offset) const
{
	return static_cast<uint32_t>(offset >> VMA_BITSCAN_MSB(m_BufferImageGranularity));
}

void VmaBlockBufferImageGranularity::AllocPage(RegionInfo& page, uint8_t allocType)
{
	// When current alloc type is free then it can be overridden by new type
	if (page.allocCount == 0 || (page.allocCount > 0 && page.allocType == VMA_SUBALLOCATION_TYPE_FREE))
		page.allocType = allocType;

	++page.allocCount;
}
#endif // _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY_FUNCTIONS
#endif // _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY

#ifndef _VMA_BLOCK_METADATA_LINEAR
/*
Allocations and their references in internal data structure look like this:

if(m_2ndVectorMode == SECOND_VECTOR_EMPTY):

		0 +-------+
		  |       |
		  |       |
		  |       |
		  +-------+
		  | Alloc |  1st[m_1stNullItemsBeginCount]
		  +-------+
		  | Alloc |  1st[m_1stNullItemsBeginCount + 1]
		  +-------+
		  |  ...  |
		  +-------+
		  | Alloc |  1st[1st.size() - 1]
		  +-------+
		  |       |
		  |       |
		  |       |
GetSize() +-------+

if(m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER):

		0 +-------+
		  | Alloc |  2nd[0]
		  +-------+
		  | Alloc |  2nd[1]
		  +-------+
		  |  ...  |
		  +-------+
		  | Alloc |  2nd[2nd.size() - 1]
		  +-------+
		  |       |
		  |       |
		  |       |
		  +-------+
		  | Alloc |  1st[m_1stNullItemsBeginCount]
		  +-------+
		  | Alloc |  1st[m_1stNullItemsBeginCount + 1]
		  +-------+
		  |  ...  |
		  +-------+
		  | Alloc |  1st[1st.size() - 1]
		  +-------+
		  |       |
GetSize() +-------+

if(m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK):

		0 +-------+
		  |       |
		  |       |
		  |       |
		  +-------+
		  | Alloc |  1st[m_1stNullItemsBeginCount]
		  +-------+
		  | Alloc |  1st[m_1stNullItemsBeginCount + 1]
		  +-------+
		  |  ...  |
		  +-------+
		  | Alloc |  1st[1st.size() - 1]
		  +-------+
		  |       |
		  |       |
		  |       |
		  +-------+
		  | Alloc |  2nd[2nd.size() - 1]
		  +-------+
		  |  ...  |
		  +-------+
		  | Alloc |  2nd[1]
		  +-------+
		  | Alloc |  2nd[0]
GetSize() +-------+

*/
class VmaBlockMetadata_Linear : public VmaBlockMetadata
{
public:
	VmaBlockMetadata_Linear(VkDeviceSize bufferImageGranularity, bool isVirtual);
	~VmaBlockMetadata_Linear() override = default;

	VkDeviceSize GetSumFreeSize() const override { return m_SumFreeSize; }
	bool IsEmpty() const override { return GetAllocationCount() == 0; }
	VkDeviceSize GetAllocationOffset(VmaAllocHandle allocHandle) const override { return (VkDeviceSize)allocHandle - 1; }

	void Init(VkDeviceSize size) override;
	size_t GetAllocationCount() const override;

	bool CreateAllocationRequest(
		VkDeviceSize allocSize,
		VkDeviceSize allocAlignment,
		VmaSuballocationType allocType,
		uint32_t strategy,
		VmaAllocationRequest* pAllocationRequest) override;

	void Alloc(
		const VmaAllocationRequest& request,
		VmaSuballocationType type,
		void* userData) override;

	void Free(VmaAllocHandle allocHandle) override;
	void Clear() override;
	void SetAllocationUserData(VmaAllocHandle allocHandle, void* userData) override;

private:
	/*
	There are two suballocation vectors, used in ping-pong way.
	The one with index m_1stVectorIndex is called 1st.
	The one with index (m_1stVectorIndex ^ 1) is called 2nd.
	2nd can be non-empty only when 1st is not empty.
	When 2nd is not empty, m_2ndVectorMode indicates its mode of operation.
	*/
	typedef std::vector<VmaSuballocation> SuballocationVectorType;

	enum SECOND_VECTOR_MODE
	{
		SECOND_VECTOR_EMPTY,
		/*
		Suballocations in 2nd vector are created later than the ones in 1st, but they
		all have smaller offset.
		*/
		SECOND_VECTOR_RING_BUFFER,
		/*
		Suballocations in 2nd vector are upper side of double stack.
		They all have offsets higher than those in 1st vector.
		Top of this stack means smaller offsets, but higher indices in this vector.
		*/
		SECOND_VECTOR_DOUBLE_STACK,
	};

	VkDeviceSize m_SumFreeSize;
	SuballocationVectorType m_Suballocations0, m_Suballocations1;
	uint32_t m_1stVectorIndex;
	SECOND_VECTOR_MODE m_2ndVectorMode;
	// Number of items in 1st vector with hAllocation = null at the beginning.
	size_t m_1stNullItemsBeginCount;
	// Number of other items in 1st vector with hAllocation = null somewhere in the middle.
	size_t m_1stNullItemsMiddleCount;
	// Number of items in 2nd vector with hAllocation = null.
	size_t m_2ndNullItemsCount;

	SuballocationVectorType& AccessSuballocations1st() { return m_1stVectorIndex ? m_Suballocations1 : m_Suballocations0; }
	SuballocationVectorType& AccessSuballocations2nd() { return m_1stVectorIndex ? m_Suballocations0 : m_Suballocations1; }
	const SuballocationVectorType& AccessSuballocations1st() const { return m_1stVectorIndex ? m_Suballocations1 : m_Suballocations0; }
	const SuballocationVectorType& AccessSuballocations2nd() const { return m_1stVectorIndex ? m_Suballocations0 : m_Suballocations1; }

	VmaSuballocation& FindSuballocation(VkDeviceSize offset) const;
	bool ShouldCompact1st() const;
	void CleanupAfterFree();

	bool CreateAllocationRequest_LowerAddress(
		VkDeviceSize allocSize,
		VkDeviceSize allocAlignment,
		VmaSuballocationType allocType,
		uint32_t strategy,
		VmaAllocationRequest* pAllocationRequest);
	bool CreateAllocationRequest_UpperAddress(
		VkDeviceSize allocSize,
		VkDeviceSize allocAlignment,
		VmaSuballocationType allocType,
		uint32_t strategy,
		VmaAllocationRequest* pAllocationRequest);
};

#ifndef _VMA_BLOCK_METADATA_LINEAR_FUNCTIONS
VmaBlockMetadata_Linear::VmaBlockMetadata_Linear(VkDeviceSize bufferImageGranularity, bool isVirtual)
	: VmaBlockMetadata(bufferImageGranularity, isVirtual),
	m_SumFreeSize(0),
	m_Suballocations0(),
	m_Suballocations1(),
	m_1stVectorIndex(0),
	m_2ndVectorMode(SECOND_VECTOR_EMPTY),
	m_1stNullItemsBeginCount(0),
	m_1stNullItemsMiddleCount(0),
	m_2ndNullItemsCount(0) {}

void VmaBlockMetadata_Linear::Init(VkDeviceSize size)
{
	VmaBlockMetadata::Init(size);
	m_SumFreeSize = size;
}

size_t VmaBlockMetadata_Linear::GetAllocationCount() const
{
	return AccessSuballocations1st().size() - m_1stNullItemsBeginCount - m_1stNullItemsMiddleCount +
		AccessSuballocations2nd().size() - m_2ndNullItemsCount;
}

bool VmaBlockMetadata_Linear::CreateAllocationRequest(
	VkDeviceSize allocSize,
	VkDeviceSize allocAlignment,
	VmaSuballocationType allocType,
	uint32_t strategy,
	VmaAllocationRequest* pAllocationRequest)
{
	if(allocSize > GetSize())
		return false;

	pAllocationRequest->size = allocSize;
	return CreateAllocationRequest_LowerAddress(allocSize, allocAlignment, allocType, strategy, pAllocationRequest);
}

void VmaBlockMetadata_Linear::Alloc(
	const VmaAllocationRequest& request,
	VmaSuballocationType type,
	void* userData)
{
	const VkDeviceSize offset = (VkDeviceSize)request.allocHandle - 1;
	const VmaSuballocation newSuballoc = { offset, request.size, userData, type };

	switch (request.type)
	{
	case VmaAllocationRequestType::UpperAddress:
	{
		SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
		suballocations2nd.push_back(newSuballoc);
		m_2ndVectorMode = SECOND_VECTOR_DOUBLE_STACK;
	}
	break;
	case VmaAllocationRequestType::EndOf1st:
	{
		SuballocationVectorType& suballocations1st = AccessSuballocations1st();

		suballocations1st.push_back(newSuballoc);
	}
	break;
	case VmaAllocationRequestType::EndOf2nd:
	{
		SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

		switch (m_2ndVectorMode)
		{
		case SECOND_VECTOR_EMPTY:
			// First allocation from second part ring buffer.
			m_2ndVectorMode = SECOND_VECTOR_RING_BUFFER;
			break;
		case SECOND_VECTOR_RING_BUFFER:
			// 2-part ring buffer is already started.
			break;
		case SECOND_VECTOR_DOUBLE_STACK:
			break;
		}

		suballocations2nd.push_back(newSuballoc);
	}
	break;
	}

	m_SumFreeSize -= newSuballoc.size;
}

void VmaBlockMetadata_Linear::Free(VmaAllocHandle allocHandle)
{
	SuballocationVectorType& suballocations1st = AccessSuballocations1st();
	SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();
	VkDeviceSize offset = (VkDeviceSize)allocHandle - 1;

	if (!suballocations1st.empty())
	{
		// First allocation: Mark it as next empty at the beginning.
		VmaSuballocation& firstSuballoc = suballocations1st[m_1stNullItemsBeginCount];
		if (firstSuballoc.offset == offset)
		{
			firstSuballoc.type = VMA_SUBALLOCATION_TYPE_FREE;
			firstSuballoc.userData = nullptr;
			m_SumFreeSize += firstSuballoc.size;
			++m_1stNullItemsBeginCount;
			CleanupAfterFree();
			return;
		}
	}

	// Last allocation in 2-part ring buffer or top of upper stack (same logic).
	if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER ||
		m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
	{
		VmaSuballocation& lastSuballoc = suballocations2nd.back();
		if (lastSuballoc.offset == offset)
		{
			m_SumFreeSize += lastSuballoc.size;
			suballocations2nd.pop_back();
			CleanupAfterFree();
			return;
		}
	}
	// Last allocation in 1st vector.
	else if (m_2ndVectorMode == SECOND_VECTOR_EMPTY)
	{
		VmaSuballocation& lastSuballoc = suballocations1st.back();
		if (lastSuballoc.offset == offset)
		{
			m_SumFreeSize += lastSuballoc.size;
			suballocations1st.pop_back();
			CleanupAfterFree();
			return;
		}
	}

	VmaSuballocation refSuballoc;
	refSuballoc.offset = offset;
	// Rest of members stays uninitialized intentionally for better performance.

	// Item from the middle of 1st vector.
	{
		const SuballocationVectorType::iterator it = VmaBinaryFindSorted(
			suballocations1st.begin() + m_1stNullItemsBeginCount,
			suballocations1st.end(),
			refSuballoc,
			VmaSuballocationOffsetLess());
		if (it != suballocations1st.end())
		{
			it->type = VMA_SUBALLOCATION_TYPE_FREE;
			it->userData = nullptr;
			++m_1stNullItemsMiddleCount;
			m_SumFreeSize += it->size;
			CleanupAfterFree();
			return;
		}
	}

	if (m_2ndVectorMode != SECOND_VECTOR_EMPTY)
	{
		// Item from the middle of 2nd vector.
		const SuballocationVectorType::iterator it = m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER ?
			VmaBinaryFindSorted(suballocations2nd.begin(), suballocations2nd.end(), refSuballoc, VmaSuballocationOffsetLess()) :
			VmaBinaryFindSorted(suballocations2nd.begin(), suballocations2nd.end(), refSuballoc, VmaSuballocationOffsetGreater());
		if (it != suballocations2nd.end())
		{
			it->type = VMA_SUBALLOCATION_TYPE_FREE;
			it->userData = nullptr;
			++m_2ndNullItemsCount;
			m_SumFreeSize += it->size;
			CleanupAfterFree();
			return;
		}
	}
}

void VmaBlockMetadata_Linear::Clear()
{
	m_SumFreeSize = GetSize();
	m_Suballocations0.clear();
	m_Suballocations1.clear();
	// Leaving m_1stVectorIndex unchanged - it doesn't matter.
	m_2ndVectorMode = SECOND_VECTOR_EMPTY;
	m_1stNullItemsBeginCount = 0;
	m_1stNullItemsMiddleCount = 0;
	m_2ndNullItemsCount = 0;
}

void VmaBlockMetadata_Linear::SetAllocationUserData(VmaAllocHandle allocHandle, void* userData)
{
	VmaSuballocation& suballoc = FindSuballocation((VkDeviceSize)allocHandle - 1);
	suballoc.userData = userData;
}

VmaSuballocation& VmaBlockMetadata_Linear::FindSuballocation(VkDeviceSize offset) const
{
	const SuballocationVectorType& suballocations1st = AccessSuballocations1st();
	const SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

	VmaSuballocation refSuballoc;
	refSuballoc.offset = offset;
	// Rest of members stays uninitialized intentionally for better performance.

	// Item from the 1st vector.
	{
		SuballocationVectorType::const_iterator it = VmaBinaryFindSorted(
			suballocations1st.begin() + m_1stNullItemsBeginCount,
			suballocations1st.end(),
			refSuballoc,
			VmaSuballocationOffsetLess());
		if (it != suballocations1st.end())
		{
			return const_cast<VmaSuballocation&>(*it);
		}
	}

	if (m_2ndVectorMode != SECOND_VECTOR_EMPTY)
	{
		// Rest of members stays uninitialized intentionally for better performance.
		SuballocationVectorType::const_iterator it = m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER ?
			VmaBinaryFindSorted(suballocations2nd.begin(), suballocations2nd.end(), refSuballoc, VmaSuballocationOffsetLess()) :
			VmaBinaryFindSorted(suballocations2nd.begin(), suballocations2nd.end(), refSuballoc, VmaSuballocationOffsetGreater());
		if (it != suballocations2nd.end())
		{
			return const_cast<VmaSuballocation&>(*it);
		}
	}

	return const_cast<VmaSuballocation&>(suballocations1st.back()); // Should never occur.
}

bool VmaBlockMetadata_Linear::ShouldCompact1st() const
{
	const size_t nullItemCount = m_1stNullItemsBeginCount + m_1stNullItemsMiddleCount;
	const size_t suballocCount = AccessSuballocations1st().size();
	return suballocCount > 32 && nullItemCount * 2 >= (suballocCount - nullItemCount) * 3;
}

void VmaBlockMetadata_Linear::CleanupAfterFree()
{
	SuballocationVectorType& suballocations1st = AccessSuballocations1st();
	SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

	if (IsEmpty())
	{
		suballocations1st.clear();
		suballocations2nd.clear();
		m_1stNullItemsBeginCount = 0;
		m_1stNullItemsMiddleCount = 0;
		m_2ndNullItemsCount = 0;
		m_2ndVectorMode = SECOND_VECTOR_EMPTY;
	}
	else
	{
		const size_t suballoc1stCount = suballocations1st.size();
		const size_t nullItem1stCount = m_1stNullItemsBeginCount + m_1stNullItemsMiddleCount;

		// Find more null items at the beginning of 1st vector.
		while (m_1stNullItemsBeginCount < suballoc1stCount &&
			suballocations1st[m_1stNullItemsBeginCount].type == VMA_SUBALLOCATION_TYPE_FREE)
		{
			++m_1stNullItemsBeginCount;
			--m_1stNullItemsMiddleCount;
		}

		// Find more null items at the end of 1st vector.
		while (m_1stNullItemsMiddleCount > 0 &&
			suballocations1st.back().type == VMA_SUBALLOCATION_TYPE_FREE)
		{
			--m_1stNullItemsMiddleCount;
			suballocations1st.pop_back();
		}

		// Find more null items at the end of 2nd vector.
		while (m_2ndNullItemsCount > 0 &&
			suballocations2nd.back().type == VMA_SUBALLOCATION_TYPE_FREE)
		{
			--m_2ndNullItemsCount;
			suballocations2nd.pop_back();
		}

		// Find more null items at the beginning of 2nd vector.
		while (m_2ndNullItemsCount > 0 &&
			suballocations2nd[0].type == VMA_SUBALLOCATION_TYPE_FREE)
		{
			--m_2ndNullItemsCount;
			suballocations2nd.erase(suballocations2nd.begin());
		}

		if (ShouldCompact1st())
		{
			const size_t nonNullItemCount = suballoc1stCount - nullItem1stCount;
			size_t srcIndex = m_1stNullItemsBeginCount;
			for (size_t dstIndex = 0; dstIndex < nonNullItemCount; ++dstIndex)
			{
				while (suballocations1st[srcIndex].type == VMA_SUBALLOCATION_TYPE_FREE)
				{
					++srcIndex;
				}
				if (dstIndex != srcIndex)
				{
					suballocations1st[dstIndex] = suballocations1st[srcIndex];
				}
				++srcIndex;
			}
			suballocations1st.resize(nonNullItemCount);
			m_1stNullItemsBeginCount = 0;
			m_1stNullItemsMiddleCount = 0;
		}

		// 2nd vector became empty.
		if (suballocations2nd.empty())
		{
			m_2ndVectorMode = SECOND_VECTOR_EMPTY;
		}

		// 1st vector became empty.
		if (suballocations1st.size() - m_1stNullItemsBeginCount == 0)
		{
			suballocations1st.clear();
			m_1stNullItemsBeginCount = 0;

			if (!suballocations2nd.empty() && m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
			{
				// Swap 1st with 2nd. Now 2nd is empty.
				m_2ndVectorMode = SECOND_VECTOR_EMPTY;
				m_1stNullItemsMiddleCount = m_2ndNullItemsCount;
				while (m_1stNullItemsBeginCount < suballocations2nd.size() &&
					suballocations2nd[m_1stNullItemsBeginCount].type == VMA_SUBALLOCATION_TYPE_FREE)
				{
					++m_1stNullItemsBeginCount;
					--m_1stNullItemsMiddleCount;
				}
				m_2ndNullItemsCount = 0;
				m_1stVectorIndex ^= 1;
			}
		}
	}
}

bool VmaBlockMetadata_Linear::CreateAllocationRequest_LowerAddress(
	VkDeviceSize allocSize,
	VkDeviceSize allocAlignment,
	VmaSuballocationType allocType,
	uint32_t ,
	VmaAllocationRequest* pAllocationRequest)
{
	const VkDeviceSize blockSize = GetSize();
	const VkDeviceSize bufferImageGranularity = GetBufferImageGranularity();
	SuballocationVectorType& suballocations1st = AccessSuballocations1st();
	SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

	if (m_2ndVectorMode == SECOND_VECTOR_EMPTY || m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
	{
		// Try to allocate at the end of 1st vector.

		VkDeviceSize resultBaseOffset = 0;
		if (!suballocations1st.empty())
		{
			const VmaSuballocation& lastSuballoc = suballocations1st.back();
			resultBaseOffset = lastSuballoc.offset + lastSuballoc.size;
		}

		// Start from offset equal to beginning of free space.
		VkDeviceSize resultOffset = resultBaseOffset;

		// Apply alignment.
		resultOffset = VmaAlignUp(resultOffset, allocAlignment);

		// Check previous suballocations for BufferImageGranularity conflicts.
		// Make bigger alignment if necessary.
		if (bufferImageGranularity > 1 && bufferImageGranularity != allocAlignment && !suballocations1st.empty())
		{
			bool bufferImageGranularityConflict = false;
			for (size_t prevSuballocIndex = suballocations1st.size(); prevSuballocIndex--; )
			{
				const VmaSuballocation& prevSuballoc = suballocations1st[prevSuballocIndex];
				if (VmaBlocksOnSamePage(prevSuballoc.offset, prevSuballoc.size, resultOffset, bufferImageGranularity))
				{
					if (VmaIsBufferImageGranularityConflict(prevSuballoc.type, allocType))
					{
						bufferImageGranularityConflict = true;
						break;
					}
				}
				else
					// Already on previous page.
					break;
			}
			if (bufferImageGranularityConflict)
			{
				resultOffset = VmaAlignUp(resultOffset, bufferImageGranularity);
			}
		}

		const VkDeviceSize freeSpaceEnd = m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK ?
			suballocations2nd.back().offset : blockSize;

		// There is enough free space at the end after alignment.
		if (resultOffset + allocSize<= freeSpaceEnd)
		{
			// Check next suballocations for BufferImageGranularity conflicts.
			// If conflict exists, allocation cannot be made here.
			if ((allocSize % bufferImageGranularity || resultOffset % bufferImageGranularity) && m_2ndVectorMode == SECOND_VECTOR_DOUBLE_STACK)
			{
				for (size_t nextSuballocIndex = suballocations2nd.size(); nextSuballocIndex--; )
				{
					const VmaSuballocation& nextSuballoc = suballocations2nd[nextSuballocIndex];
					if (VmaBlocksOnSamePage(resultOffset, allocSize, nextSuballoc.offset, bufferImageGranularity))
					{
						if (VmaIsBufferImageGranularityConflict(allocType, nextSuballoc.type))
						{
							return false;
						}
					}
					else
					{
						// Already on previous page.
						break;
					}
				}
			}

			// All tests passed: Success.
			pAllocationRequest->allocHandle = (VmaAllocHandle)(resultOffset + 1);
			// pAllocationRequest->item, customData unused.
			pAllocationRequest->type = VmaAllocationRequestType::EndOf1st;
			return true;
		}
	}

	// Wrap-around to end of 2nd vector. Try to allocate there, watching for the
	// beginning of 1st vector as the end of free space.
	if (m_2ndVectorMode == SECOND_VECTOR_EMPTY || m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
	{
		VkDeviceSize resultBaseOffset = 0;
		if (!suballocations2nd.empty())
		{
			const VmaSuballocation& lastSuballoc = suballocations2nd.back();
			resultBaseOffset = lastSuballoc.offset + lastSuballoc.size;
		}

		// Start from offset equal to beginning of free space.
		VkDeviceSize resultOffset = resultBaseOffset;

		// Apply alignment.
		resultOffset = VmaAlignUp(resultOffset, allocAlignment);

		// Check previous suballocations for BufferImageGranularity conflicts.
		// Make bigger alignment if necessary.
		if (bufferImageGranularity > 1 && bufferImageGranularity != allocAlignment && !suballocations2nd.empty())
		{
			bool bufferImageGranularityConflict = false;
			for (size_t prevSuballocIndex = suballocations2nd.size(); prevSuballocIndex--; )
			{
				const VmaSuballocation& prevSuballoc = suballocations2nd[prevSuballocIndex];
				if (VmaBlocksOnSamePage(prevSuballoc.offset, prevSuballoc.size, resultOffset, bufferImageGranularity))
				{
					if (VmaIsBufferImageGranularityConflict(prevSuballoc.type, allocType))
					{
						bufferImageGranularityConflict = true;
						break;
					}
				}
				else
					// Already on previous page.
					break;
			}
			if (bufferImageGranularityConflict)
			{
				resultOffset = VmaAlignUp(resultOffset, bufferImageGranularity);
			}
		}

		size_t index1st = m_1stNullItemsBeginCount;

		// There is enough free space at the end after alignment.
		if ((index1st == suballocations1st.size() && resultOffset + allocSize <= blockSize) ||
			(index1st < suballocations1st.size() && resultOffset + allocSize <= suballocations1st[index1st].offset))
		{
			// Check next suballocations for BufferImageGranularity conflicts.
			// If conflict exists, allocation cannot be made here.
			if (allocSize % bufferImageGranularity || resultOffset % bufferImageGranularity)
			{
				for (size_t nextSuballocIndex = index1st;
					nextSuballocIndex < suballocations1st.size();
					nextSuballocIndex++)
				{
					const VmaSuballocation& nextSuballoc = suballocations1st[nextSuballocIndex];
					if (VmaBlocksOnSamePage(resultOffset, allocSize, nextSuballoc.offset, bufferImageGranularity))
					{
						if (VmaIsBufferImageGranularityConflict(allocType, nextSuballoc.type))
						{
							return false;
						}
					}
					else
					{
						// Already on next page.
						break;
					}
				}
			}

			// All tests passed: Success.
			pAllocationRequest->allocHandle = (VmaAllocHandle)(resultOffset + 1);
			pAllocationRequest->type = VmaAllocationRequestType::EndOf2nd;
			// pAllocationRequest->item, customData unused.
			return true;
		}
	}

	return false;
}

bool VmaBlockMetadata_Linear::CreateAllocationRequest_UpperAddress(
	VkDeviceSize allocSize,
	VkDeviceSize allocAlignment,
	VmaSuballocationType allocType,
	uint32_t ,
	VmaAllocationRequest* pAllocationRequest)
{
	const VkDeviceSize blockSize = GetSize();
	const VkDeviceSize bufferImageGranularity = GetBufferImageGranularity();
	SuballocationVectorType& suballocations1st = AccessSuballocations1st();
	SuballocationVectorType& suballocations2nd = AccessSuballocations2nd();

	if (m_2ndVectorMode == SECOND_VECTOR_RING_BUFFER)
	{
		return false;
	}

	// Try to allocate before 2nd.back(), or end of block if 2nd.empty().
	if (allocSize > blockSize)
	{
		return false;
	}
	VkDeviceSize resultBaseOffset = blockSize - allocSize;
	if (!suballocations2nd.empty())
	{
		const VmaSuballocation& lastSuballoc = suballocations2nd.back();
		resultBaseOffset = lastSuballoc.offset - allocSize;
		if (allocSize > lastSuballoc.offset)
		{
			return false;
		}
	}

	// Start from offset equal to end of free space.
	VkDeviceSize resultOffset = resultBaseOffset;

	// Apply alignment.
	resultOffset = VmaAlignDown(resultOffset, allocAlignment);

	// Check next suballocations from 2nd for BufferImageGranularity conflicts.
	// Make bigger alignment if necessary.
	if (bufferImageGranularity > 1 && bufferImageGranularity != allocAlignment && !suballocations2nd.empty())
	{
		bool bufferImageGranularityConflict = false;
		for (size_t nextSuballocIndex = suballocations2nd.size(); nextSuballocIndex--; )
		{
			const VmaSuballocation& nextSuballoc = suballocations2nd[nextSuballocIndex];
			if (VmaBlocksOnSamePage(resultOffset, allocSize, nextSuballoc.offset, bufferImageGranularity))
			{
				if (VmaIsBufferImageGranularityConflict(nextSuballoc.type, allocType))
				{
					bufferImageGranularityConflict = true;
					break;
				}
			}
			else
				// Already on previous page.
				break;
		}
		if (bufferImageGranularityConflict)
		{
			resultOffset = VmaAlignDown(resultOffset, bufferImageGranularity);
		}
	}

	// There is enough free space.
	const VkDeviceSize endOf1st = !suballocations1st.empty() ?
		suballocations1st.back().offset + suballocations1st.back().size :
		0;
	if (endOf1st <= resultOffset)
	{
		// Check previous suballocations for BufferImageGranularity conflicts.
		// If conflict exists, allocation cannot be made here.
		if (bufferImageGranularity > 1)
		{
			for (size_t prevSuballocIndex = suballocations1st.size(); prevSuballocIndex--; )
			{
				const VmaSuballocation& prevSuballoc = suballocations1st[prevSuballocIndex];
				if (VmaBlocksOnSamePage(prevSuballoc.offset, prevSuballoc.size, resultOffset, bufferImageGranularity))
				{
					if (VmaIsBufferImageGranularityConflict(allocType, prevSuballoc.type))
					{
						return false;
					}
				}
				else
				{
					// Already on next page.
					break;
				}
			}
		}

		// All tests passed: Success.
		pAllocationRequest->allocHandle = (VmaAllocHandle)(resultOffset + 1);
		// pAllocationRequest->item unused.
		pAllocationRequest->type = VmaAllocationRequestType::UpperAddress;
		return true;
	}

	return false;
}
#endif // _VMA_BLOCK_METADATA_LINEAR_FUNCTIONS
#endif // _VMA_BLOCK_METADATA_LINEAR

#ifndef _VMA_BLOCK_METADATA_TLSF
class VmaBlockMetadata_TLSF : public VmaBlockMetadata
{
public:
	VmaBlockMetadata_TLSF(VkDeviceSize bufferImageGranularity, bool isVirtual);
	~VmaBlockMetadata_TLSF() override;

	size_t GetAllocationCount() const override { return m_AllocCount; }
	VkDeviceSize GetSumFreeSize() const override { return m_BlocksFreeSize + m_NullBlock->size; }
	bool IsEmpty() const override { return m_NullBlock->offset == 0; }
	VkDeviceSize GetAllocationOffset(VmaAllocHandle allocHandle) const override { return ((Block*)allocHandle)->offset; }

	void Init(VkDeviceSize size) override;

	bool CreateAllocationRequest(
		VkDeviceSize allocSize,
		VkDeviceSize allocAlignment,
		VmaSuballocationType allocType,
		uint32_t strategy,
		VmaAllocationRequest* pAllocationRequest) override;

	void Alloc(
		const VmaAllocationRequest& request,
		VmaSuballocationType type,
		void* userData) override;

	void Free(VmaAllocHandle allocHandle) override;
	void Clear() override;
	void SetAllocationUserData(VmaAllocHandle allocHandle, void* userData) override;

private:
	// According to original paper it should be preferable 4 or 5:
	// M. Masmano, I. Ripoll, A. Crespo, and J. Real "TLSF: a New Dynamic Memory Allocator for Real-Time Systems"
	// http://www.gii.upv.es/tlsf/files/ecrts04_tlsf.pdf
	static const uint8_t SECOND_LEVEL_INDEX = 5;
	static const uint16_t SMALL_BUFFER_SIZE = 256;
	static const uint32_t INITIAL_BLOCK_ALLOC_COUNT = 16;
	static const uint8_t MEMORY_CLASS_SHIFT = 7;
	static const uint8_t MAX_MEMORY_CLASSES = 65 - MEMORY_CLASS_SHIFT;

	class Block
	{
	public:
		VkDeviceSize offset;
		VkDeviceSize size;
		Block* prevPhysical;
		Block* nextPhysical;

		void MarkFree() { prevFree = nullptr; }
		void MarkTaken() { prevFree = this; }
		bool IsFree() const { return prevFree != this; }
		void*& UserData() { return userData; }
		Block*& PrevFree() { return prevFree; }
		Block*& NextFree() { return nextFree; }

	private:
		Block* prevFree; // Address of the same block here indicates that block is taken
		union
		{
			Block* nextFree;
			void* userData;
		};
	};

	size_t m_AllocCount;
	// Total number of free blocks besides null block
	size_t m_BlocksFreeCount;
	// Total size of free blocks excluding null block
	VkDeviceSize m_BlocksFreeSize;
	uint32_t m_IsFreeBitmap;
	uint8_t m_MemoryClasses;
	uint32_t m_InnerIsFreeBitmap[MAX_MEMORY_CLASSES];
	uint32_t m_ListsCount;
	/*
	* 0: 0-3 lists for small buffers
	* 1+: 0-(2^SLI-1) lists for normal buffers
	*/
	Block** m_FreeList;
	VmaPoolAllocator<Block> m_BlockAllocator;
	Block* m_NullBlock;
	VmaBlockBufferImageGranularity m_GranularityHandler;

	static uint8_t SizeToMemoryClass(VkDeviceSize size);
	uint16_t SizeToSecondIndex(VkDeviceSize size, uint8_t memoryClass) const;
	uint32_t GetListIndex(uint8_t memoryClass, uint16_t secondIndex) const;
	uint32_t GetListIndex(VkDeviceSize size) const;

	void RemoveFreeBlock(Block* block);
	void InsertFreeBlock(Block* block);
	void MergeBlock(Block* block, Block* prev);

	Block* FindFreeBlock(VkDeviceSize size, uint32_t& listIndex) const;
	bool CheckBlock(
		Block& block,
		uint32_t listIndex,
		VkDeviceSize allocSize,
		VkDeviceSize allocAlignment,
		VmaSuballocationType allocType,
		VmaAllocationRequest* pAllocationRequest);
};

#ifndef _VMA_BLOCK_METADATA_TLSF_FUNCTIONS
VmaBlockMetadata_TLSF::VmaBlockMetadata_TLSF(VkDeviceSize bufferImageGranularity, bool isVirtual)
	: VmaBlockMetadata(bufferImageGranularity, isVirtual),
	m_AllocCount(0),
	m_BlocksFreeCount(0),
	m_BlocksFreeSize(0),
	m_IsFreeBitmap(0),
	m_MemoryClasses(0),
	m_ListsCount(0),
	m_FreeList(nullptr),
	m_BlockAllocator(INITIAL_BLOCK_ALLOC_COUNT),
	m_NullBlock(nullptr),
	m_GranularityHandler(bufferImageGranularity) {}

VmaBlockMetadata_TLSF::~VmaBlockMetadata_TLSF()
{
	if (m_FreeList)
		vma_delete_array(m_FreeList, m_ListsCount);
	m_GranularityHandler.Destroy();
}

void VmaBlockMetadata_TLSF::Init(VkDeviceSize size)
{
	VmaBlockMetadata::Init(size);

	if (!IsVirtual())
		m_GranularityHandler.Init(size);

	m_NullBlock = m_BlockAllocator.Alloc();
	m_NullBlock->size = size;
	m_NullBlock->offset = 0;
	m_NullBlock->prevPhysical = nullptr;
	m_NullBlock->nextPhysical = nullptr;
	m_NullBlock->MarkFree();
	m_NullBlock->NextFree() = nullptr;
	m_NullBlock->PrevFree() = nullptr;
	uint8_t memoryClass = SizeToMemoryClass(size);
	uint16_t sli = SizeToSecondIndex(size, memoryClass);
	m_ListsCount = (memoryClass == 0 ? 0 : (memoryClass - 1) * (1UL << SECOND_LEVEL_INDEX) + sli) + 1;
	if (IsVirtual())
		m_ListsCount += 1UL << SECOND_LEVEL_INDEX;
	else
		m_ListsCount += 4;

	m_MemoryClasses = memoryClass + uint8_t(2);
	memset(m_InnerIsFreeBitmap, 0, MAX_MEMORY_CLASSES * sizeof(uint32_t));

	m_FreeList = vma_new_array(Block*, m_ListsCount);
	memset(m_FreeList, 0, m_ListsCount * sizeof(Block*));
}

bool VmaBlockMetadata_TLSF::CreateAllocationRequest(
	VkDeviceSize allocSize,
	VkDeviceSize allocAlignment,
	VmaSuballocationType allocType,
	uint32_t,
	VmaAllocationRequest* pAllocationRequest)
{
	// For small granularity round up
	if (!IsVirtual())
		m_GranularityHandler.RoundupAllocRequest(allocType, allocSize, allocAlignment);

	// Quick check for too small pool
	if (allocSize > GetSumFreeSize())
		return false;

	// If no free blocks in pool then check only null block
	if (m_BlocksFreeCount == 0)
		return CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest);

	// Round up to the next block
	VkDeviceSize sizeForNextList = allocSize;
	VkDeviceSize smallSizeStep = VkDeviceSize(SMALL_BUFFER_SIZE / (IsVirtual() ? 1U << SECOND_LEVEL_INDEX : 4U));
	if (allocSize > SMALL_BUFFER_SIZE)
	{
		sizeForNextList += (1ULL << (VMA_BITSCAN_MSB(allocSize) - SECOND_LEVEL_INDEX));
	}
	else if (allocSize > SMALL_BUFFER_SIZE - smallSizeStep)
		sizeForNextList = SMALL_BUFFER_SIZE + 1;
	else
		sizeForNextList += smallSizeStep;

	uint32_t nextListIndex = m_ListsCount;
	uint32_t prevListIndex = m_ListsCount;
	Block* nextListBlock = nullptr;
	Block* prevListBlock = nullptr;

	// Check larger bucket
	nextListBlock = FindFreeBlock(sizeForNextList, nextListIndex);
	while (nextListBlock)
	{
		if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
			return true;
		nextListBlock = nextListBlock->NextFree();
	}

	// If failed check null block
	if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest))
		return true;

	// Check best fit bucket
	prevListBlock = FindFreeBlock(allocSize, prevListIndex);
	while (prevListBlock)
	{
		if (CheckBlock(*prevListBlock, prevListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
			return true;
		prevListBlock = prevListBlock->NextFree();
	}

	// Worst case, full search has to be done
	while (++nextListIndex < m_ListsCount)
	{
		nextListBlock = m_FreeList[nextListIndex];
		while (nextListBlock)
		{
			if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
				return true;
			nextListBlock = nextListBlock->NextFree();
		}
	}

	// No more memory sadly
	return false;
}

void VmaBlockMetadata_TLSF::Alloc(
	const VmaAllocationRequest& request,
	VmaSuballocationType ,
	void* userData)
{
	// Get block and pop it from the free list
	Block* currentBlock = (Block*)request.allocHandle;
	VkDeviceSize offset = request.algorithmData;

	if (currentBlock != m_NullBlock)
		RemoveFreeBlock(currentBlock);

	VkDeviceSize missingAlignment = offset - currentBlock->offset;

	// Append missing alignment to prev block or create new one
	if (missingAlignment)
	{
		Block* prevBlock = currentBlock->prevPhysical;
		if (prevBlock->IsFree() && prevBlock->size)
		{
			uint32_t oldList = GetListIndex(prevBlock->size);
			prevBlock->size += missingAlignment;
			// Check if new size crosses list bucket
			if (oldList != GetListIndex(prevBlock->size))
			{
				prevBlock->size -= missingAlignment;
				RemoveFreeBlock(prevBlock);
				prevBlock->size += missingAlignment;
				InsertFreeBlock(prevBlock);
			}
			else
				m_BlocksFreeSize += missingAlignment;
		}
		else
		{
			Block* newBlock = m_BlockAllocator.Alloc();
			currentBlock->prevPhysical = newBlock;
			prevBlock->nextPhysical = newBlock;
			newBlock->prevPhysical = prevBlock;
			newBlock->nextPhysical = currentBlock;
			newBlock->size = missingAlignment;
			newBlock->offset = currentBlock->offset;
			newBlock->MarkTaken();

			InsertFreeBlock(newBlock);
		}

		currentBlock->size -= missingAlignment;
		currentBlock->offset += missingAlignment;
	}

	VkDeviceSize size = request.size;
	if (currentBlock->size == size)
	{
		if (currentBlock == m_NullBlock)
		{
			// Setup new null block
			m_NullBlock = m_BlockAllocator.Alloc();
			m_NullBlock->size = 0;
			m_NullBlock->offset = currentBlock->offset + size;
			m_NullBlock->prevPhysical = currentBlock;
			m_NullBlock->nextPhysical = nullptr;
			m_NullBlock->MarkFree();
			m_NullBlock->PrevFree() = nullptr;
			m_NullBlock->NextFree() = nullptr;
			currentBlock->nextPhysical = m_NullBlock;
			currentBlock->MarkTaken();
		}
	}
	else
	{
		// Create new free block
		Block* newBlock = m_BlockAllocator.Alloc();
		newBlock->size = currentBlock->size - size;
		newBlock->offset = currentBlock->offset + size;
		newBlock->prevPhysical = currentBlock;
		newBlock->nextPhysical = currentBlock->nextPhysical;
		currentBlock->nextPhysical = newBlock;
		currentBlock->size = size;

		if (currentBlock == m_NullBlock)
		{
			m_NullBlock = newBlock;
			m_NullBlock->MarkFree();
			m_NullBlock->NextFree() = nullptr;
			m_NullBlock->PrevFree() = nullptr;
			currentBlock->MarkTaken();
		}
		else
		{
			newBlock->nextPhysical->prevPhysical = newBlock;
			newBlock->MarkTaken();
			InsertFreeBlock(newBlock);
		}
	}
	currentBlock->UserData() = userData;

	if (!IsVirtual())
		m_GranularityHandler.AllocPages((uint8_t)(uintptr_t)request.customData,
			currentBlock->offset, currentBlock->size);
	++m_AllocCount;
}

void VmaBlockMetadata_TLSF::Free(VmaAllocHandle allocHandle)
{
	Block* block = (Block*)allocHandle;
	Block* next = block->nextPhysical;

	if (!IsVirtual())
		m_GranularityHandler.FreePages(block->offset, block->size);
	--m_AllocCount;

	// Try merging
	Block* prev = block->prevPhysical;
	if (prev != nullptr && prev->IsFree() && prev->size)
	{
		RemoveFreeBlock(prev);
		MergeBlock(block, prev);
	}

	if (!next->IsFree())
		InsertFreeBlock(block);
	else if (next == m_NullBlock)
		MergeBlock(m_NullBlock, block);
	else
	{
		RemoveFreeBlock(next);
		MergeBlock(next, block);
		InsertFreeBlock(next);
	}
}

void VmaBlockMetadata_TLSF::Clear()
{
	m_AllocCount = 0;
	m_BlocksFreeCount = 0;
	m_BlocksFreeSize = 0;
	m_IsFreeBitmap = 0;
	m_NullBlock->offset = 0;
	m_NullBlock->size = GetSize();
	Block* block = m_NullBlock->prevPhysical;
	m_NullBlock->prevPhysical = nullptr;
	while (block)
	{
		Block* prev = block->prevPhysical;
		m_BlockAllocator.Free(block);
		block = prev;
	}
	memset(m_FreeList, 0, m_ListsCount * sizeof(Block*));
	memset(m_InnerIsFreeBitmap, 0, m_MemoryClasses * sizeof(uint32_t));
	m_GranularityHandler.Clear();
}

void VmaBlockMetadata_TLSF::SetAllocationUserData(VmaAllocHandle allocHandle, void* userData)
{
	Block* block = (Block*)allocHandle;
	block->UserData() = userData;
}

uint8_t VmaBlockMetadata_TLSF::SizeToMemoryClass(VkDeviceSize size)
{
	if (size > SMALL_BUFFER_SIZE)
		return uint8_t(VMA_BITSCAN_MSB(size) - MEMORY_CLASS_SHIFT);
	return 0;
}

uint16_t VmaBlockMetadata_TLSF::SizeToSecondIndex(VkDeviceSize size, uint8_t memoryClass) const
{
	if (memoryClass == 0)
	{
		if (IsVirtual())
			return static_cast<uint16_t>((size - 1) / 8);
		return static_cast<uint16_t>((size - 1) / 64);
	}
	return static_cast<uint16_t>((size >> (memoryClass + MEMORY_CLASS_SHIFT - SECOND_LEVEL_INDEX)) ^ (1U << SECOND_LEVEL_INDEX));
}

uint32_t VmaBlockMetadata_TLSF::GetListIndex(uint8_t memoryClass, uint16_t secondIndex) const
{
	if (memoryClass == 0)
		return secondIndex;

	const uint32_t index = static_cast<uint32_t>(memoryClass - 1) * (1 << SECOND_LEVEL_INDEX) + secondIndex;
	if (IsVirtual())
		return index + (1 << SECOND_LEVEL_INDEX);
	return index + 4;
}

uint32_t VmaBlockMetadata_TLSF::GetListIndex(VkDeviceSize size) const
{
	uint8_t memoryClass = SizeToMemoryClass(size);
	return GetListIndex(memoryClass, SizeToSecondIndex(size, memoryClass));
}

void VmaBlockMetadata_TLSF::RemoveFreeBlock(Block* block)
{
	if (block->NextFree() != nullptr)
		block->NextFree()->PrevFree() = block->PrevFree();
	if (block->PrevFree() != nullptr)
		block->PrevFree()->NextFree() = block->NextFree();
	else
	{
		uint8_t memClass = SizeToMemoryClass(block->size);
		uint16_t secondIndex = SizeToSecondIndex(block->size, memClass);
		uint32_t index = GetListIndex(memClass, secondIndex);
		m_FreeList[index] = block->NextFree();
		if (block->NextFree() == nullptr)
		{
			m_InnerIsFreeBitmap[memClass] &= ~(1U << secondIndex);
			if (m_InnerIsFreeBitmap[memClass] == 0)
				m_IsFreeBitmap &= ~(1UL << memClass);
		}
	}
	block->MarkTaken();
	block->UserData() = nullptr;
	--m_BlocksFreeCount;
	m_BlocksFreeSize -= block->size;
}

void VmaBlockMetadata_TLSF::InsertFreeBlock(Block* block)
{
	uint8_t memClass = SizeToMemoryClass(block->size);
	uint16_t secondIndex = SizeToSecondIndex(block->size, memClass);
	uint32_t index = GetListIndex(memClass, secondIndex);
	block->PrevFree() = nullptr;
	block->NextFree() = m_FreeList[index];
	m_FreeList[index] = block;
	if (block->NextFree() != nullptr)
		block->NextFree()->PrevFree() = block;
	else
	{
		m_InnerIsFreeBitmap[memClass] |= 1U << secondIndex;
		m_IsFreeBitmap |= 1UL << memClass;
	}
	++m_BlocksFreeCount;
	m_BlocksFreeSize += block->size;
}

void VmaBlockMetadata_TLSF::MergeBlock(Block* block, Block* prev)
{
	block->offset = prev->offset;
	block->size += prev->size;
	block->prevPhysical = prev->prevPhysical;
	if (block->prevPhysical)
		block->prevPhysical->nextPhysical = block;
	m_BlockAllocator.Free(prev);
}

VmaBlockMetadata_TLSF::Block* VmaBlockMetadata_TLSF::FindFreeBlock(VkDeviceSize size, uint32_t& listIndex) const
{
	uint8_t memoryClass = SizeToMemoryClass(size);
	uint32_t innerFreeMap = m_InnerIsFreeBitmap[memoryClass] & (~0U << SizeToSecondIndex(size, memoryClass));
	if (!innerFreeMap)
	{
		// Check higher levels for available blocks
		uint32_t freeMap = m_IsFreeBitmap & (~0UL << (memoryClass + 1));
		if (!freeMap)
			return nullptr; // No more memory available

		// Find lowest free region
		memoryClass = VMA_BITSCAN_LSB(freeMap);
		innerFreeMap = m_InnerIsFreeBitmap[memoryClass];
	}
	// Find lowest free subregion
	listIndex = GetListIndex(memoryClass, VMA_BITSCAN_LSB(innerFreeMap));
	return m_FreeList[listIndex];
}

bool VmaBlockMetadata_TLSF::CheckBlock(
	Block& block,
	uint32_t listIndex,
	VkDeviceSize allocSize,
	VkDeviceSize allocAlignment,
	VmaSuballocationType allocType,
	VmaAllocationRequest* pAllocationRequest)
{
	VkDeviceSize alignedOffset = VmaAlignUp(block.offset, allocAlignment);
	if (block.size < allocSize + alignedOffset - block.offset)
		return false;

	// Check for granularity conflicts
	if (!IsVirtual() &&
		m_GranularityHandler.CheckConflictAndAlignUp(alignedOffset, allocSize, block.offset, block.size, allocType))
		return false;

	// Alloc successful
	pAllocationRequest->type = VmaAllocationRequestType::TLSF;
	pAllocationRequest->allocHandle = (VmaAllocHandle)&block;
	pAllocationRequest->size = allocSize;
	pAllocationRequest->customData = (void*)allocType;
	pAllocationRequest->algorithmData = alignedOffset;

	// Place block at the start of list if it's normal block
	if (listIndex != m_ListsCount && block.PrevFree())
	{
		block.PrevFree()->NextFree() = block.NextFree();
		if (block.NextFree())
			block.NextFree()->PrevFree() = block.PrevFree();
		block.PrevFree() = nullptr;
		block.NextFree() = m_FreeList[listIndex];
		m_FreeList[listIndex] = &block;
		if (block.NextFree())
			block.NextFree()->PrevFree() = &block;
	}

	return true;
}
#endif // _VMA_BLOCK_METADATA_TLSF_FUNCTIONS
#endif // _VMA_BLOCK_METADATA_TLSF

#ifndef _VMA_BLOCK_VECTOR

struct VmaBlockVector
{
	VmaBlockVector(
		VmaAllocator hAllocator,
		uint32_t memoryTypeIndex,
		VkDeviceSize preferredBlockSize,
		size_t minBlockCount,
		size_t maxBlockCount,
		VkDeviceSize bufferImageGranularity,
		bool explicitBlockSize,
		uint32_t algorithm,
		float priority,
		VkDeviceSize minAllocationAlignment,
		void* pMemoryAllocateNext);
	~VmaBlockVector();

	VmaAllocator GetAllocator() const { return m_hAllocator; }
	uint32_t GetMemoryTypeIndex() const { return m_MemoryTypeIndex; }
	VkDeviceSize GetPreferredBlockSize() const { return m_PreferredBlockSize; }
	VkDeviceSize GetBufferImageGranularity() const { return m_BufferImageGranularity; }
	uint32_t GetAlgorithm() const { return m_Algorithm; }
	bool HasExplicitBlockSize() const { return m_ExplicitBlockSize; }
	float GetPriority() const { return m_Priority; }
	const void* GetAllocationNextPtr() const { return m_pMemoryAllocateNext; }

	VkResult CreateMinBlocks();
	bool IsEmpty();

	VkResult Allocate(
		VkDeviceSize size,
		VkDeviceSize alignment,
		VmaSuballocationType suballocType,
		size_t allocationCount,
		VmaAllocation* pAllocations);

	void Free(VmaAllocation hAllocation);

private:
	const VmaAllocator m_hAllocator;
	const uint32_t m_MemoryTypeIndex;
	const VkDeviceSize m_PreferredBlockSize;
	const size_t m_MinBlockCount;
	const size_t m_MaxBlockCount;
	const VkDeviceSize m_BufferImageGranularity;
	const bool m_ExplicitBlockSize;
	const uint32_t m_Algorithm;
	const float m_Priority;
	const VkDeviceSize m_MinAllocationAlignment;

	void* const m_pMemoryAllocateNext;
	// Incrementally sorted by sumFreeSize, ascending.
	std::vector<VmaDeviceMemoryBlock*> m_Blocks;
	uint32_t m_NextBlockId;
	bool m_IncrementalSort = true;

	void SetIncrementalSort(bool val) { m_IncrementalSort = val; }

	VkDeviceSize CalcMaxBlockSize() const;
	// Finds and removes given block from vector.
	void Remove(VmaDeviceMemoryBlock* pBlock);
	// Performs single step in sorting m_Blocks. They may not be fully sorted
	// after this call.
	void IncrementallySortBlocks();
	void SortByFreeSize();

	VkResult AllocatePage(
		VkDeviceSize size,
		VkDeviceSize alignment,
		VmaSuballocationType suballocType,
		VmaAllocation* pAllocation);

	VkResult AllocateFromBlock(
		VmaDeviceMemoryBlock* pBlock,
		VkDeviceSize size,
		VkDeviceSize alignment,
		VmaSuballocationType suballocType,
		uint32_t strategy,
		VmaAllocation* pAllocation);

	VkResult CommitAllocationRequest(
		VmaAllocationRequest& allocRequest,
		VmaDeviceMemoryBlock* pBlock,
		VkDeviceSize alignment,
		VmaSuballocationType suballocType,
		VmaAllocation* pAllocation);

	VkResult CreateBlock(VkDeviceSize blockSize, size_t* pNewBlockIndex);
	bool HasEmptyBlock();
};
#endif // _VMA_BLOCK_VECTOR

#ifndef _VMA_CURRENT_BUDGET_DATA
struct VmaCurrentBudgetData
{
public:

	uint32_t m_BlockCount[VK_MAX_MEMORY_HEAPS];
	uint32_t m_AllocationCount[VK_MAX_MEMORY_HEAPS];
	uint64_t m_BlockBytes[VK_MAX_MEMORY_HEAPS];
	uint64_t m_AllocationBytes[VK_MAX_MEMORY_HEAPS];

	uint32_t m_OperationsSinceBudgetFetch;
	uint64_t m_VulkanUsage[VK_MAX_MEMORY_HEAPS];
	uint64_t m_VulkanBudget[VK_MAX_MEMORY_HEAPS];
	uint64_t m_BlockBytesAtBudgetFetch[VK_MAX_MEMORY_HEAPS];

	VmaCurrentBudgetData();

	void AddAllocation(uint32_t heapIndex, VkDeviceSize allocationSize);
	void RemoveAllocation(uint32_t heapIndex, VkDeviceSize allocationSize);
};

#ifndef _VMA_CURRENT_BUDGET_DATA_FUNCTIONS
VmaCurrentBudgetData::VmaCurrentBudgetData()
{
	for (uint32_t heapIndex = 0; heapIndex < VK_MAX_MEMORY_HEAPS; ++heapIndex)
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

void VmaCurrentBudgetData::AddAllocation(uint32_t heapIndex, VkDeviceSize allocationSize)
{
	m_AllocationBytes[heapIndex] += allocationSize;
	++m_AllocationCount[heapIndex];
	++m_OperationsSinceBudgetFetch;
}

void VmaCurrentBudgetData::RemoveAllocation(uint32_t heapIndex, VkDeviceSize allocationSize)
{
	m_AllocationBytes[heapIndex] -= allocationSize;
	--m_AllocationCount[heapIndex];
	++m_OperationsSinceBudgetFetch;
}
#endif // _VMA_CURRENT_BUDGET_DATA_FUNCTIONS
#endif // _VMA_CURRENT_BUDGET_DATA

#ifndef _VMA_ALLOCATION_OBJECT_ALLOCATOR
/*
Thread-safe wrapper over VmaPoolAllocator free list, for allocation of VmaAllocation_T objects.
*/
class VmaAllocationObjectAllocator
{
public:
	explicit VmaAllocationObjectAllocator()
		: m_Allocator(1024) {}

	template<typename... Types> VmaAllocation Allocate(Types&&... args);
	void Free(VmaAllocation hAlloc);

private:
	VmaPoolAllocator<VmaAllocation_T> m_Allocator;
};

template<typename... Types>
VmaAllocation VmaAllocationObjectAllocator::Allocate(Types&&... args)
{
	return m_Allocator.Alloc<Types...>(std::forward<Types>(args)...);
}

void VmaAllocationObjectAllocator::Free(VmaAllocation hAlloc)
{
	m_Allocator.Free(hAlloc);
}
#endif // _VMA_ALLOCATION_OBJECT_ALLOCATOR


struct VmaAllocator_T
{
	const VkDevice m_hDevice;
	const VkInstance m_hInstance;
	VmaAllocationObjectAllocator m_AllocationObjectAllocator;

	// Each bit (1 << i) is set if HeapSizeLimit is enabled for that heap, so cannot allocate more than the heap size.
	uint32_t m_HeapSizeLimitMask;

	VkPhysicalDeviceProperties m_PhysicalDeviceProperties;
	VkPhysicalDeviceMemoryProperties m_MemProps;

	// Default pools.
	VmaBlockVector* m_pBlockVectors[VK_MAX_MEMORY_TYPES];
	VmaDedicatedAllocationList m_DedicatedAllocations[VK_MAX_MEMORY_TYPES];

	VmaCurrentBudgetData m_Budget;
	uint32_t m_DeviceMemoryCount; // Total number of VkDeviceMemory objects.

	explicit VmaAllocator_T(const VmaAllocatorCreateInfo* pCreateInfo);
	VkResult Init(const VmaAllocatorCreateInfo* pCreateInfo);
	~VmaAllocator_T();

	const VmaVulkanFunctions& GetVulkanFunctions() const
	{
		return m_VulkanFunctions;
	}
	void GetHeapBudgets(VmaBudget* outBudgets, uint32_t firstHeap, uint32_t heapCount);
	VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

	VkDeviceSize GetBufferImageGranularity() const
	{
		return m_PhysicalDeviceProperties.limits.bufferImageGranularity;
	}

	uint32_t GetMemoryHeapCount() const { return m_MemProps.memoryHeapCount; }
	uint32_t GetMemoryTypeCount() const { return m_MemProps.memoryTypeCount; }

	uint32_t MemoryTypeIndexToHeapIndex(uint32_t memTypeIndex) const
	{
		return m_MemProps.memoryTypes[memTypeIndex].heapIndex;
	}
	// True when specific memory type is HOST_VISIBLE but not HOST_COHERENT.
	bool IsMemoryTypeNonCoherent(uint32_t memTypeIndex) const
	{
		return (m_MemProps.memoryTypes[memTypeIndex].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	}
	// Minimum alignment for all allocations in specific memory type.
	VkDeviceSize GetMemoryTypeMinAlignment(uint32_t memTypeIndex) const
	{
		return IsMemoryTypeNonCoherent(memTypeIndex) ? m_PhysicalDeviceProperties.limits.nonCoherentAtomSize : 1;
	}

	bool IsIntegratedGpu() const
	{
		return m_PhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
	}

	uint32_t GetGlobalMemoryTypeBits() const { return m_GlobalMemoryTypeBits; }

	void GetBufferMemoryRequirements(
		VkBuffer hBuffer,
		VkMemoryRequirements& memReq,
		bool& requiresDedicatedAllocation,
		bool& prefersDedicatedAllocation) const;
	void GetImageMemoryRequirements(
		VkImage hImage,
		VkMemoryRequirements& memReq,
		bool& requiresDedicatedAllocation,
		bool& prefersDedicatedAllocation) const;
	VkResult FindMemoryTypeIndex(
		uint32_t memoryTypeBits,
		const VmaAllocationCreateInfo* pAllocationCreateInfo,
		VmaBufferImageUsage bufImgUsage,
		uint32_t* pMemoryTypeIndex) const;

	// Main allocation function.
	VkResult AllocateMemory(
		const VkMemoryRequirements& vkMemReq,
		bool requiresDedicatedAllocation,
		bool prefersDedicatedAllocation,
		VkBuffer dedicatedBuffer,
		VkImage dedicatedImage,
		VmaBufferImageUsage dedicatedBufferImageUsage,
		const VmaAllocationCreateInfo& createInfo,
		VmaSuballocationType suballocType,
		size_t allocationCount,
		VmaAllocation* pAllocations);

	// Main deallocation function.
	void FreeMemory(
		size_t allocationCount,
		const VmaAllocation* pAllocations);

	static void GetAllocationInfo(VmaAllocation hAllocation, VmaAllocationInfo* pAllocationInfo);

	void SetCurrentFrameIndex(uint32_t frameIndex);
	uint32_t GetCurrentFrameIndex() const { return m_CurrentFrameIndex; }

	// Call to Vulkan function vkAllocateMemory with accompanying bookkeeping.
	VkResult AllocateVulkanMemory(const VkMemoryAllocateInfo* pAllocateInfo, VkDeviceMemory* pMemory);
	// Call to Vulkan function vkFreeMemory with accompanying bookkeeping.
	void FreeVulkanMemory(uint32_t memoryType, VkDeviceSize size, VkDeviceMemory hMemory);

private:
	VkDeviceSize m_PreferredLargeHeapBlockSize;

	VkPhysicalDevice m_PhysicalDevice;
	uint32_t m_CurrentFrameIndex;

	VmaVulkanFunctions m_VulkanFunctions;

	// Global bit mask AND-ed with any memoryTypeBits to disallow certain memory types.
	uint32_t m_GlobalMemoryTypeBits;

	VkDeviceSize CalcPreferredBlockSize(uint32_t memTypeIndex);

	VkResult AllocateMemoryOfType(
		VkDeviceSize size,
		VkDeviceSize alignment,
		bool dedicatedPreferred,
		VkBuffer dedicatedBuffer,
		VkImage dedicatedImage,
		VmaBufferImageUsage dedicatedBufferImageUsage,
		const VmaAllocationCreateInfo& createInfo,
		uint32_t memTypeIndex,
		VmaSuballocationType suballocType,
		VmaDedicatedAllocationList& dedicatedAllocations,
		VmaBlockVector& blockVector,
		size_t allocationCount,
		VmaAllocation* pAllocations);

	// Helper function only to be used inside AllocateDedicatedMemory.
	VkResult AllocateDedicatedMemoryPage(
		VkDeviceSize size,
		VmaSuballocationType suballocType,
		uint32_t memTypeIndex,
		const VkMemoryAllocateInfo& allocInfo,
		VmaAllocation* pAllocation);

	// Allocates and registers new VkDeviceMemory specifically for dedicated allocations.
	VkResult AllocateDedicatedMemory(
		VkDeviceSize size,
		VmaSuballocationType suballocType,
		VmaDedicatedAllocationList& dedicatedAllocations,
		uint32_t memTypeIndex,
		float priority,
		VkBuffer dedicatedBuffer,
		VkImage dedicatedImage,
		VmaBufferImageUsage dedicatedBufferImageUsage,
		size_t allocationCount,
		VmaAllocation* pAllocations,
		const void* pNextChain = nullptr);

	void FreeDedicatedMemory(VmaAllocation allocation);

	VkResult CalcMemTypeParams(
		VmaAllocationCreateInfo& outCreateInfo,
		uint32_t memTypeIndex,
		VkDeviceSize size,
		size_t allocationCount);
	static VkResult CalcAllocationParams(
		VmaAllocationCreateInfo& outCreateInfo,
		bool dedicatedRequired);

	uint32_t CalculateGlobalMemoryTypeBits() const;

	void UpdateVulkanBudget();
};


#ifndef _VMA_DEVICE_MEMORY_BLOCK_FUNCTIONS
VmaDeviceMemoryBlock::VmaDeviceMemoryBlock(VmaAllocator )
	: m_pMetadata(nullptr),
	m_MemoryTypeIndex(UINT32_MAX),
	m_Id(0),
	m_hMemory(VK_NULL_HANDLE)
{}

VmaDeviceMemoryBlock::~VmaDeviceMemoryBlock()
{
}

void VmaDeviceMemoryBlock::Init(
	uint32_t newMemoryTypeIndex,
	VkDeviceMemory newMemory,
	VkDeviceSize newSize,
	uint32_t id,
	uint32_t algorithm,
	VkDeviceSize bufferImageGranularity)
{
	m_MemoryTypeIndex = newMemoryTypeIndex;
	m_Id = id;
	m_hMemory = newMemory;


	if (algorithm == 0) {
		m_pMetadata = vma_new(VmaBlockMetadata_TLSF)(bufferImageGranularity, false); // isVirtual
	} else {
		m_pMetadata = vma_new(VmaBlockMetadata_Linear)(bufferImageGranularity, false); // isVirtual
	}
	m_pMetadata->Init(newSize);
}

void VmaDeviceMemoryBlock::Destroy(VmaAllocator allocator)
{
	allocator->FreeVulkanMemory(m_MemoryTypeIndex, m_pMetadata->GetSize(), m_hMemory);
	m_hMemory = VK_NULL_HANDLE;

	vma_delete(m_pMetadata);
	m_pMetadata = nullptr;
}

#endif // _VMA_DEVICE_MEMORY_BLOCK_FUNCTIONS

#ifndef _VMA_ALLOCATION_T_FUNCTIONS
VmaAllocation_T::VmaAllocation_T()
	: m_Alignment{ 1 },
	m_Size{ 0 },
	m_MemoryTypeIndex{ 0 },
	m_Type{ (uint8_t)ALLOCATION_TYPE_NONE },
	m_SuballocationType{ (uint8_t)VMA_SUBALLOCATION_TYPE_UNKNOWN },
	m_Flags{ 0 }
{}

VmaAllocation_T::~VmaAllocation_T()
{
}

void VmaAllocation_T::InitBlockAllocation(
	VmaDeviceMemoryBlock* block,
	VmaAllocHandle allocHandle,
	VkDeviceSize alignment,
	VkDeviceSize size,
	uint32_t memoryTypeIndex,
	VmaSuballocationType suballocationType
)
{
	m_Type = (uint8_t)ALLOCATION_TYPE_BLOCK;
	m_Alignment = alignment;
	m_Size = size;
	m_MemoryTypeIndex = memoryTypeIndex;
	m_SuballocationType = (uint8_t)suballocationType;
	m_BlockAllocation.m_Block = block;
	m_BlockAllocation.m_AllocHandle = allocHandle;
}

void VmaAllocation_T::InitDedicatedAllocation(
	uint32_t memoryTypeIndex,
	VkDeviceMemory hMemory,
	VmaSuballocationType suballocationType,
	VkDeviceSize size)
{
	m_Type = (uint8_t)ALLOCATION_TYPE_DEDICATED;
	m_Alignment = 0;
	m_Size = size;
	m_MemoryTypeIndex = memoryTypeIndex;
	m_SuballocationType = (uint8_t)suballocationType;
	m_DedicatedAllocation.m_hMemory = hMemory;
	m_DedicatedAllocation.m_Prev = nullptr;
	m_DedicatedAllocation.m_Next = nullptr;
}

void VmaAllocation_T::Destroy()
{
}

void VmaAllocation_T::SwapBlockAllocation(VmaAllocation allocation)
{
	m_BlockAllocation.m_Block->m_pMetadata->SetAllocationUserData(m_BlockAllocation.m_AllocHandle, allocation);
	std::swap(m_BlockAllocation, allocation->m_BlockAllocation);
	m_BlockAllocation.m_Block->m_pMetadata->SetAllocationUserData(m_BlockAllocation.m_AllocHandle, this);
}

VmaAllocHandle VmaAllocation_T::GetAllocHandle() const
{
	switch (m_Type)
	{
	case ALLOCATION_TYPE_BLOCK:
		return m_BlockAllocation.m_AllocHandle;
	case ALLOCATION_TYPE_DEDICATED:
		return VK_NULL_HANDLE;
	default:
		return VK_NULL_HANDLE;
	}
}

VkDeviceSize VmaAllocation_T::GetOffset() const
{
	switch (m_Type)
	{
	case ALLOCATION_TYPE_BLOCK:
		return m_BlockAllocation.m_Block->m_pMetadata->GetAllocationOffset(m_BlockAllocation.m_AllocHandle);
	case ALLOCATION_TYPE_DEDICATED:
		return 0;
	default:
		return 0;
	}
}

VkDeviceMemory VmaAllocation_T::GetMemory() const
{
	switch (m_Type)
	{
	case ALLOCATION_TYPE_BLOCK:
		return m_BlockAllocation.m_Block->GetDeviceMemory();
	case ALLOCATION_TYPE_DEDICATED:
		return m_DedicatedAllocation.m_hMemory;
	default:
		return VK_NULL_HANDLE;
	}
}

#endif // _VMA_ALLOCATION_T_FUNCTIONS

#ifndef _VMA_BLOCK_VECTOR_FUNCTIONS
VmaBlockVector::VmaBlockVector(
	VmaAllocator hAllocator,
	uint32_t memoryTypeIndex,
	VkDeviceSize preferredBlockSize,
	size_t minBlockCount,
	size_t maxBlockCount,
	VkDeviceSize bufferImageGranularity,
	bool explicitBlockSize,
	uint32_t algorithm,
	float priority,
	VkDeviceSize minAllocationAlignment,
	void* pMemoryAllocateNext)
	: m_hAllocator(hAllocator),
	m_MemoryTypeIndex(memoryTypeIndex),
	m_PreferredBlockSize(preferredBlockSize),
	m_MinBlockCount(minBlockCount),
	m_MaxBlockCount(maxBlockCount),
	m_BufferImageGranularity(bufferImageGranularity),
	m_ExplicitBlockSize(explicitBlockSize),
	m_Algorithm(algorithm),
	m_Priority(priority),
	m_MinAllocationAlignment(minAllocationAlignment),
	m_pMemoryAllocateNext(pMemoryAllocateNext),
	m_Blocks(),
	m_NextBlockId(0) {}

VmaBlockVector::~VmaBlockVector()
{
	for (size_t i = m_Blocks.size(); i--; )
	{
		m_Blocks[i]->Destroy(m_hAllocator);
		vma_delete(m_Blocks[i]);
	}
}

VkResult VmaBlockVector::CreateMinBlocks()
{
	for (size_t i = 0; i < m_MinBlockCount; ++i)
	{
		VkResult res = CreateBlock(m_PreferredBlockSize, nullptr);
		if (res != VK_SUCCESS)
		{
			return res;
		}
	}
	return VK_SUCCESS;
}

bool VmaBlockVector::IsEmpty()
{
	return m_Blocks.empty();
}

VkResult VmaBlockVector::Allocate(
	VkDeviceSize size,
	VkDeviceSize alignment,
	VmaSuballocationType suballocType,
	size_t allocationCount,
	VmaAllocation* pAllocations)
{
	size_t allocIndex = 0;
	VkResult res = VK_SUCCESS;

	alignment = VMA_MAX(alignment, m_MinAllocationAlignment);

	{
		for (; allocIndex < allocationCount; ++allocIndex)
		{
			res = AllocatePage(
				size,
				alignment,
				suballocType,
				pAllocations + allocIndex);
			if (res != VK_SUCCESS)
			{
				break;
			}
		}
	}

	if (res != VK_SUCCESS)
	{
		// Free all already created allocations.
		while (allocIndex--)
			Free(pAllocations[allocIndex]);
		memset(pAllocations, 0, sizeof(VmaAllocation) * allocationCount);
	}

	return res;
}

VkResult VmaBlockVector::AllocatePage(
	VkDeviceSize size,
	VkDeviceSize alignment,
	VmaSuballocationType suballocType,
	VmaAllocation* pAllocation)
{
	VkDeviceSize freeMemory = 0;
	{
		const uint32_t heapIndex = m_hAllocator->MemoryTypeIndexToHeapIndex(m_MemoryTypeIndex);
		VmaBudget heapBudget = {};
		m_hAllocator->GetHeapBudgets(&heapBudget, heapIndex, 1);
		freeMemory = (heapBudget.usage < heapBudget.budget) ? (heapBudget.budget - heapBudget.usage) : 0;
	}

	const bool canFallbackToDedicated = !HasExplicitBlockSize();
	const bool canCreateNewBlock = (m_Blocks.size() < m_MaxBlockCount) && (freeMemory >= size || !canFallbackToDedicated);

	// 1. Search existing allocations. Try to allocate.
	if (m_Algorithm == 0)	// linear
	{
		// Use only last block.
		if (!m_Blocks.empty())
		{
			VmaDeviceMemoryBlock* const pCurrBlock = m_Blocks.back();
			VkResult res = AllocateFromBlock(pCurrBlock, size, alignment, suballocType, 0, pAllocation);	// 0 strategy
			if (res == VK_SUCCESS)
			{
				IncrementallySortBlocks();
				return VK_SUCCESS;
			}
		}
	}
	else
	{
		const bool isHostVisible = (m_hAllocator->m_MemProps.memoryTypes[m_MemoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
		if(!isHostVisible)	// VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT
		{
			// Backward order in m_Blocks - prefer blocks with largest amount of free space.
			for (size_t blockIndex = m_Blocks.size(); blockIndex--; )
			{
				VmaDeviceMemoryBlock* const pCurrBlock = m_Blocks[blockIndex];
				VkResult res = AllocateFromBlock(pCurrBlock, size, alignment, suballocType, 0, pAllocation);	// 0 strategy
				if (res == VK_SUCCESS)
				{
					IncrementallySortBlocks();
					return VK_SUCCESS;
				}
			}
		}
	}

	// 2. Try to create new block.
	if (canCreateNewBlock)
	{
		// Calculate optimal size for new block.
		VkDeviceSize newBlockSize = m_PreferredBlockSize;
		uint32_t newBlockSizeShift = 0;
		const uint32_t NEW_BLOCK_SIZE_SHIFT_MAX = 3;

		if (!m_ExplicitBlockSize)
		{
			// Allocate 1/8, 1/4, 1/2 as first blocks.
			const VkDeviceSize maxExistingBlockSize = CalcMaxBlockSize();
			for (uint32_t i = 0; i < NEW_BLOCK_SIZE_SHIFT_MAX; ++i)
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
		}

		size_t newBlockIndex = 0;
		VkResult res = (newBlockSize <= freeMemory || !canFallbackToDedicated) ?
			CreateBlock(newBlockSize, &newBlockIndex) : VK_ERROR_OUT_OF_DEVICE_MEMORY;
		// Allocation of this size failed? Try 1/2, 1/4, 1/8 of m_PreferredBlockSize.
		if (!m_ExplicitBlockSize)
		{
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
		}

		if (res == VK_SUCCESS)
		{
			VmaDeviceMemoryBlock* const pBlock = m_Blocks[newBlockIndex];
			res = AllocateFromBlock(pBlock, size, alignment, suballocType, 0, pAllocation);	// 0 strategy
			if (res == VK_SUCCESS)
			{
				IncrementallySortBlocks();
				return VK_SUCCESS;
			}

			// Allocation from new block failed, possibly due to alignment.
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}
	}

	return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

void VmaBlockVector::Free(VmaAllocation hAllocation)
{
	VmaDeviceMemoryBlock* pBlockToDelete = nullptr;

	bool budgetExceeded = false;
	{
		const uint32_t heapIndex = m_hAllocator->MemoryTypeIndexToHeapIndex(m_MemoryTypeIndex);
		VmaBudget heapBudget = {};
		m_hAllocator->GetHeapBudgets(&heapBudget, heapIndex, 1);
		budgetExceeded = heapBudget.usage >= heapBudget.budget;
	}

	// Scope for lock.
	{
		VmaDeviceMemoryBlock* pBlock = hAllocation->GetBlock();

		const bool hadEmptyBlockBeforeFree = HasEmptyBlock();
		pBlock->m_pMetadata->Free(hAllocation->GetAllocHandle());

		const bool canDeleteBlock = m_Blocks.size() > m_MinBlockCount;
		// pBlock became empty after this deallocation.
		if (pBlock->m_pMetadata->IsEmpty())
		{
			// Already had empty block. We don't want to have two, so delete this one.
			if ((hadEmptyBlockBeforeFree || budgetExceeded) && canDeleteBlock)
			{
				pBlockToDelete = pBlock;
				Remove(pBlock);
			}
			// else: We now have one empty block - leave it. A hysteresis to avoid allocating whole block back and forth.
		}
		// pBlock didn't become empty, but we have another empty block - find and free that one.
		// (This is optional, heuristics.)
		else if (hadEmptyBlockBeforeFree && canDeleteBlock)
		{
			VmaDeviceMemoryBlock* pLastBlock = m_Blocks.back();
			if (pLastBlock->m_pMetadata->IsEmpty())
			{
				pBlockToDelete = pLastBlock;
				m_Blocks.pop_back();
			}
		}

		IncrementallySortBlocks();

		m_hAllocator->m_Budget.RemoveAllocation(m_hAllocator->MemoryTypeIndexToHeapIndex(m_MemoryTypeIndex), hAllocation->GetSize());
		hAllocation->Destroy();
		m_hAllocator->m_AllocationObjectAllocator.Free(hAllocation);
	}

	// Destruction of a free block. Deferred until this point, outside of mutex
	// lock, for performance reason.
	if (pBlockToDelete != nullptr)
	{
		pBlockToDelete->Destroy(m_hAllocator);
		vma_delete(pBlockToDelete);
	}
}

VkDeviceSize VmaBlockVector::CalcMaxBlockSize() const
{
	VkDeviceSize result = 0;
	for (size_t i = m_Blocks.size(); i--; )
	{
		result = VMA_MAX(result, m_Blocks[i]->m_pMetadata->GetSize());
		if (result >= m_PreferredBlockSize)
		{
			break;
		}
	}
	return result;
}

void VmaBlockVector::Remove(VmaDeviceMemoryBlock* pBlock)
{
	for (uint32_t blockIndex = 0; blockIndex < m_Blocks.size(); ++blockIndex)
	{
		if (m_Blocks[blockIndex] == pBlock)
		{
			m_Blocks.erase(m_Blocks.begin() + blockIndex);
			return;
		}
	}
}

void VmaBlockVector::IncrementallySortBlocks()
{
	if (!m_IncrementalSort)
		return;
	//if (m_Algorithm != VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT)
	{
		// Bubble sort only until first swap.
		for (size_t i = 1; i < m_Blocks.size(); ++i)
		{
			if (m_Blocks[i - 1]->m_pMetadata->GetSumFreeSize() > m_Blocks[i]->m_pMetadata->GetSumFreeSize())
			{
				std::swap(m_Blocks[i - 1], m_Blocks[i]);
				return;
			}
		}
	}
}

void VmaBlockVector::SortByFreeSize()
{
	VMA_SORT(m_Blocks.begin(), m_Blocks.end(),
		[](VmaDeviceMemoryBlock* b1, VmaDeviceMemoryBlock* b2) -> bool
		{
			return b1->m_pMetadata->GetSumFreeSize() < b2->m_pMetadata->GetSumFreeSize();
		});
}

VkResult VmaBlockVector::AllocateFromBlock(
	VmaDeviceMemoryBlock* pBlock,
	VkDeviceSize size,
	VkDeviceSize alignment,
	VmaSuballocationType suballocType,
	uint32_t strategy,
	VmaAllocation* pAllocation)
{
	VmaAllocationRequest currRequest = {};
	if (pBlock->m_pMetadata->CreateAllocationRequest(
		size,
		alignment,
		suballocType,
		strategy,
		&currRequest))
	{
		return CommitAllocationRequest(currRequest, pBlock, alignment, suballocType, pAllocation);
	}
	return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

VkResult VmaBlockVector::CommitAllocationRequest(
	VmaAllocationRequest& allocRequest,
	VmaDeviceMemoryBlock* pBlock,
	VkDeviceSize alignment,
	VmaSuballocationType suballocType,
	VmaAllocation* pAllocation)
{
	*pAllocation = m_hAllocator->m_AllocationObjectAllocator.Allocate();
	pBlock->m_pMetadata->Alloc(allocRequest, suballocType, *pAllocation);
	(*pAllocation)->InitBlockAllocation(
		pBlock,
		allocRequest.allocHandle,
		alignment,
		allocRequest.size, // Not size, as actual allocation size may be larger than requested!
		m_MemoryTypeIndex,
		suballocType
	);
	m_hAllocator->m_Budget.AddAllocation(m_hAllocator->MemoryTypeIndexToHeapIndex(m_MemoryTypeIndex), allocRequest.size);
	return VK_SUCCESS;
}

VkResult VmaBlockVector::CreateBlock(VkDeviceSize blockSize, size_t* pNewBlockIndex)
{
	VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocInfo.pNext = m_pMemoryAllocateNext;
	allocInfo.memoryTypeIndex = m_MemoryTypeIndex;
	allocInfo.allocationSize = blockSize;

	// Every standalone block can potentially contain a buffer with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT - always enable the feature.
	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR };
	allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
	VmaPnextChainPushFront(&allocInfo, &allocFlagsInfo);

	VkMemoryPriorityAllocateInfoEXT priorityInfo = { VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };
	priorityInfo.priority = m_Priority;
	VmaPnextChainPushFront(&allocInfo, &priorityInfo);

	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkResult res = m_hAllocator->AllocateVulkanMemory(&allocInfo, &mem);
	if (res < 0)
	{
		return res;
	}

	// New VkDeviceMemory successfully created.

	// Create new Allocation for it.
	VmaDeviceMemoryBlock* const pBlock = vma_new(VmaDeviceMemoryBlock)(m_hAllocator);
	pBlock->Init(
		m_MemoryTypeIndex,
		mem,
		allocInfo.allocationSize,
		m_NextBlockId++,
		m_Algorithm,
		m_BufferImageGranularity);

	m_Blocks.push_back(pBlock);
	if (pNewBlockIndex != nullptr)
	{
		*pNewBlockIndex = m_Blocks.size() - 1;
	}

	return VK_SUCCESS;
}

bool VmaBlockVector::HasEmptyBlock()
{
	for (size_t index = 0, count = m_Blocks.size(); index < count; ++index)
	{
		VmaDeviceMemoryBlock* const pBlock = m_Blocks[index];
		if (pBlock->m_pMetadata->IsEmpty())
		{
			return true;
		}
	}
	return false;
}

#endif // _VMA_BLOCK_VECTOR_FUNCTIONS

#ifndef _VMA_ALLOCATOR_T_FUNCTIONS
VmaAllocator_T::VmaAllocator_T(const VmaAllocatorCreateInfo* pCreateInfo) :
	m_hDevice(pCreateInfo->device),
	m_hInstance(pCreateInfo->instance),
	m_AllocationObjectAllocator(),
	m_HeapSizeLimitMask(0),
	m_DeviceMemoryCount(0),
	m_PreferredLargeHeapBlockSize(0),
	m_PhysicalDevice(pCreateInfo->physicalDevice),
	m_GlobalMemoryTypeBits(UINT32_MAX)
{
	memset(&m_PhysicalDeviceProperties, 0, sizeof(m_PhysicalDeviceProperties));
	memset(&m_MemProps, 0, sizeof(m_MemProps));

	memset(&m_pBlockVectors, 0, sizeof(m_pBlockVectors));
	memset(&m_VulkanFunctions, 0, sizeof(m_VulkanFunctions));

	(*m_VulkanFunctions.vkGetPhysicalDeviceProperties)(m_PhysicalDevice, &m_PhysicalDeviceProperties);
	(*m_VulkanFunctions.vkGetPhysicalDeviceMemoryProperties)(m_PhysicalDevice, &m_MemProps);

	m_PreferredLargeHeapBlockSize = VMA_DEFAULT_LARGE_HEAP_BLOCK_SIZE;

	m_GlobalMemoryTypeBits = CalculateGlobalMemoryTypeBits();

	for(uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
	{
		// Create only supported types
		if((m_GlobalMemoryTypeBits & (1U << memTypeIndex)) != 0)
		{
			const VkDeviceSize preferredBlockSize = CalcPreferredBlockSize(memTypeIndex);
			m_pBlockVectors[memTypeIndex] = vma_new(VmaBlockVector)(
				this,
				memTypeIndex,
				preferredBlockSize,
				0,
				SIZE_MAX,
				GetBufferImageGranularity(),
				false, // explicitBlockSize
				0, // algorithm
				0.5F, // priority (0.5 is the default per Vulkan spec)
				GetMemoryTypeMinAlignment(memTypeIndex), // minAllocationAlignment
				nullptr); // // pMemoryAllocateNext
			// No need to call m_pBlockVectors[memTypeIndex][blockVectorTypeIndex]->CreateMinBlocks here,
			// because minBlockCount is 0.
		}
	}
}

VkResult VmaAllocator_T::Init(const VmaAllocatorCreateInfo* ) {
	UpdateVulkanBudget();
	return VK_SUCCESS;
}

VmaAllocator_T::~VmaAllocator_T() {
	for(size_t memTypeIndex = GetMemoryTypeCount(); memTypeIndex--; ) {
		vma_delete(m_pBlockVectors[memTypeIndex]);
	}
}

VkDeviceSize VmaAllocator_T::CalcPreferredBlockSize(uint32_t memTypeIndex)
{
	const uint32_t heapIndex = MemoryTypeIndexToHeapIndex(memTypeIndex);
	const VkDeviceSize heapSize = m_MemProps.memoryHeaps[heapIndex].size;
	const bool isSmallHeap = heapSize <= VMA_SMALL_HEAP_MAX_SIZE;
	return VmaAlignUp(isSmallHeap ? (heapSize / 8) : m_PreferredLargeHeapBlockSize, (VkDeviceSize)32);
}

VkResult VmaAllocator_T::AllocateMemoryOfType(
	VkDeviceSize size,
	VkDeviceSize alignment,
	bool dedicatedPreferred,
	VkBuffer dedicatedBuffer,
	VkImage dedicatedImage,
	VmaBufferImageUsage dedicatedBufferImageUsage,
	const VmaAllocationCreateInfo& createInfo,
	uint32_t memTypeIndex,
	VmaSuballocationType suballocType,
	VmaDedicatedAllocationList& dedicatedAllocations,
	VmaBlockVector& blockVector,
	size_t allocationCount,
	VmaAllocation* pAllocations)
{
	VmaAllocationCreateInfo finalCreateInfo = createInfo;
	VkResult res = CalcMemTypeParams(
		finalCreateInfo,
		memTypeIndex,
		size,
		allocationCount);
	if(res != VK_SUCCESS)
		return res;

	if((finalCreateInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) != 0)
	{
		return AllocateDedicatedMemory(
			size,
			suballocType,
			dedicatedAllocations,
			memTypeIndex,
			finalCreateInfo.priority,
			dedicatedBuffer,
			dedicatedImage,
			dedicatedBufferImageUsage,
			allocationCount,
			pAllocations,
			blockVector.GetAllocationNextPtr());
	}

	const bool canAllocateDedicated = !blockVector.HasExplicitBlockSize();

	if(canAllocateDedicated)
	{
		// Heuristics: Allocate dedicated memory if requested size if greater than half of preferred block size.
		if(size > blockVector.GetPreferredBlockSize() / 2)
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
				finalCreateInfo.priority,
				dedicatedBuffer,
				dedicatedImage,
				dedicatedBufferImageUsage,
				allocationCount,
				pAllocations,
				blockVector.GetAllocationNextPtr());
			if(res == VK_SUCCESS)
			{
				return VK_SUCCESS;
			}
		}
	}

	res = blockVector.Allocate(
		size,
		alignment,
		suballocType,
		allocationCount,
		pAllocations);
	if(res == VK_SUCCESS)
		return VK_SUCCESS;

	// Try dedicated memory.
	if(canAllocateDedicated && !dedicatedPreferred)
	{
		res = AllocateDedicatedMemory(
			size,
			suballocType,
			dedicatedAllocations,
			memTypeIndex,
			finalCreateInfo.priority,
			dedicatedBuffer,
			dedicatedImage,
			dedicatedBufferImageUsage,
			allocationCount,
			pAllocations,
			blockVector.GetAllocationNextPtr());
		if(res == VK_SUCCESS)
		{
			return VK_SUCCESS;
		}
	}
	// Everything failed: Return error code.
	return res;
}

VkResult VmaAllocator_T::AllocateDedicatedMemory(
	VkDeviceSize size,
	VmaSuballocationType suballocType,
	VmaDedicatedAllocationList& dedicatedAllocations,
	uint32_t memTypeIndex,
	float priority,
	VkBuffer dedicatedBuffer,
	VkImage dedicatedImage,
	VmaBufferImageUsage dedicatedBufferImageUsage,
	size_t allocationCount,
	VmaAllocation* pAllocations,
	const void* pNextChain)
{
	VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocInfo.memoryTypeIndex = memTypeIndex;
	allocInfo.allocationSize = size;
	allocInfo.pNext = pNextChain;

	VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR };
	if(dedicatedBuffer != VK_NULL_HANDLE)
	{
		dedicatedAllocInfo.buffer = dedicatedBuffer;
		VmaPnextChainPushFront(&allocInfo, &dedicatedAllocInfo);
	}
	else if(dedicatedImage != VK_NULL_HANDLE)
	{
		dedicatedAllocInfo.image = dedicatedImage;
		VmaPnextChainPushFront(&allocInfo, &dedicatedAllocInfo);
	}

	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR };
	bool canContainBufferWithDeviceAddress = true;
	if(dedicatedBuffer != VK_NULL_HANDLE)
	{
		canContainBufferWithDeviceAddress = dedicatedBufferImageUsage == VmaBufferImageUsage::UNKNOWN ||
			dedicatedBufferImageUsage.Contains(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_EXT);
	}
	else if(dedicatedImage != VK_NULL_HANDLE)
	{
		canContainBufferWithDeviceAddress = false;
	}
	if(canContainBufferWithDeviceAddress)
	{
		allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		VmaPnextChainPushFront(&allocInfo, &allocFlagsInfo);
	}

	VkMemoryPriorityAllocateInfoEXT priorityInfo = { VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };
	priorityInfo.priority = priority;
	VmaPnextChainPushFront(&allocInfo, &priorityInfo);

	size_t allocIndex = 0;
	VkResult res = VK_SUCCESS;
	for(; allocIndex < allocationCount; ++allocIndex)
	{
		res = AllocateDedicatedMemoryPage(
			size,
			suballocType,
			memTypeIndex,
			allocInfo,
			pAllocations + allocIndex);
		if(res != VK_SUCCESS)
		{
			break;
		}
	}

	if(res == VK_SUCCESS)
	{
		for (allocIndex = 0; allocIndex < allocationCount; ++allocIndex)
		{
			dedicatedAllocations.Register(pAllocations[allocIndex]);
		}
	}
	else
	{
		// Free all already created allocations.
		while(allocIndex--)
		{
			VmaAllocation currAlloc = pAllocations[allocIndex];
			VkDeviceMemory hMemory = currAlloc->GetMemory();
			FreeVulkanMemory(memTypeIndex, currAlloc->GetSize(), hMemory);
			m_Budget.RemoveAllocation(MemoryTypeIndexToHeapIndex(memTypeIndex), currAlloc->GetSize());
			m_AllocationObjectAllocator.Free(currAlloc);
		}

		memset(pAllocations, 0, sizeof(VmaAllocation) * allocationCount);
	}

	return res;
}

VkResult VmaAllocator_T::AllocateDedicatedMemoryPage(
	VkDeviceSize size,
	VmaSuballocationType suballocType,
	uint32_t memTypeIndex,
	const VkMemoryAllocateInfo& allocInfo,
	VmaAllocation* pAllocation)
{
	VkDeviceMemory hMemory = VK_NULL_HANDLE;
	VkResult res = AllocateVulkanMemory(&allocInfo, &hMemory);
	if(res < 0)
	{
		return res;
	}

	*pAllocation = m_AllocationObjectAllocator.Allocate();
	(*pAllocation)->InitDedicatedAllocation(memTypeIndex, hMemory, suballocType, size);
	m_Budget.AddAllocation(MemoryTypeIndexToHeapIndex(memTypeIndex), size);

	return VK_SUCCESS;
}

void VmaAllocator_T::GetBufferMemoryRequirements(
	VkBuffer hBuffer,
	VkMemoryRequirements& memReq,
	bool& requiresDedicatedAllocation,
	bool& prefersDedicatedAllocation) const
{
	VkBufferMemoryRequirementsInfo2KHR memReqInfo = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR };
	memReqInfo.buffer = hBuffer;

	VkMemoryDedicatedRequirementsKHR memDedicatedReq = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };

	VkMemoryRequirements2KHR memReq2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
	VmaPnextChainPushFront(&memReq2, &memDedicatedReq);

	(*m_VulkanFunctions.vkGetBufferMemoryRequirements2KHR)(m_hDevice, &memReqInfo, &memReq2);

	memReq = memReq2.memoryRequirements;
	requiresDedicatedAllocation = (memDedicatedReq.requiresDedicatedAllocation != VK_FALSE);
	prefersDedicatedAllocation  = (memDedicatedReq.prefersDedicatedAllocation  != VK_FALSE);
}

void VmaAllocator_T::GetImageMemoryRequirements(
	VkImage hImage,
	VkMemoryRequirements& memReq,
	bool& requiresDedicatedAllocation,
	bool& prefersDedicatedAllocation) const
{
	VkImageMemoryRequirementsInfo2KHR memReqInfo = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR };
	memReqInfo.image = hImage;

	VkMemoryDedicatedRequirementsKHR memDedicatedReq = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };

	VkMemoryRequirements2KHR memReq2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
	VmaPnextChainPushFront(&memReq2, &memDedicatedReq);

	(*m_VulkanFunctions.vkGetImageMemoryRequirements2KHR)(m_hDevice, &memReqInfo, &memReq2);

	memReq = memReq2.memoryRequirements;
	requiresDedicatedAllocation = (memDedicatedReq.requiresDedicatedAllocation != VK_FALSE);
	prefersDedicatedAllocation  = (memDedicatedReq.prefersDedicatedAllocation  != VK_FALSE);
}

VkResult VmaAllocator_T::FindMemoryTypeIndex(
	uint32_t memoryTypeBits,
	const VmaAllocationCreateInfo* pAllocationCreateInfo,
	VmaBufferImageUsage bufImgUsage,
	uint32_t* pMemoryTypeIndex) const
{
	memoryTypeBits &= GetGlobalMemoryTypeBits();

	if(pAllocationCreateInfo->memoryTypeBits != 0)
	{
		memoryTypeBits &= pAllocationCreateInfo->memoryTypeBits;
	}

	VkMemoryPropertyFlags requiredFlags = 0;
	VkMemoryPropertyFlags preferredFlags = 0;
	VkMemoryPropertyFlags notPreferredFlags = 0;
	if(!FindMemoryPreferences(
		IsIntegratedGpu(),
		*pAllocationCreateInfo,
		bufImgUsage,
		requiredFlags, preferredFlags, notPreferredFlags))
	{
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	*pMemoryTypeIndex = UINT32_MAX;
	uint32_t minCost = UINT32_MAX;
	for(uint32_t memTypeIndex = 0, memTypeBit = 1;
		memTypeIndex < GetMemoryTypeCount();
		++memTypeIndex, memTypeBit <<= 1)
	{
		// This memory type is acceptable according to memoryTypeBits bitmask.
		if((memTypeBit & memoryTypeBits) != 0)
		{
			const VkMemoryPropertyFlags currFlags =
				m_MemProps.memoryTypes[memTypeIndex].propertyFlags;
			// This memory type contains requiredFlags.
			if((requiredFlags & ~currFlags) == 0)
			{
				// Calculate cost as number of bits from preferredFlags not present in this memory type.
				uint32_t currCost = VMA_COUNT_BITS_SET(preferredFlags & ~currFlags) +
					VMA_COUNT_BITS_SET(currFlags & notPreferredFlags);
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
		}
	}
	return (*pMemoryTypeIndex != UINT32_MAX) ? VK_SUCCESS : VK_ERROR_FEATURE_NOT_PRESENT;
}

VkResult VmaAllocator_T::CalcMemTypeParams(
	VmaAllocationCreateInfo& inoutCreateInfo,
	uint32_t memTypeIndex,
	VkDeviceSize size,
	size_t allocationCount)
{
	if((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) != 0 &&
		(inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT) != 0)
	{
		const uint32_t heapIndex = MemoryTypeIndexToHeapIndex(memTypeIndex);
		VmaBudget heapBudget = {};
		GetHeapBudgets(&heapBudget, heapIndex, 1);
		if(heapBudget.usage + size * allocationCount > heapBudget.budget)
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}
	}
	return VK_SUCCESS;
}

VkResult VmaAllocator_T::CalcAllocationParams(VmaAllocationCreateInfo& inoutCreateInfo, bool)
{
	// Non-auto USAGE values imply HOST_ACCESS flags.
	// And so does VMA_MEMORY_USAGE_UNKNOWN because it is used with custom pools.
	// Which specific flag is used doesn't matter. They change things only when used with VMA_MEMORY_USAGE_AUTO*.
	// Otherwise they just protect from assert on mapping.
	if(inoutCreateInfo.usage != VMA_MEMORY_USAGE_AUTO &&
		inoutCreateInfo.usage != VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE &&
		inoutCreateInfo.usage != VMA_MEMORY_USAGE_AUTO_PREFER_HOST)
	{
		if((inoutCreateInfo.flags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) == 0)
		{
			inoutCreateInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
		}
	}

	return VK_SUCCESS;
}

VkResult VmaAllocator_T::AllocateMemory(
	const VkMemoryRequirements& vkMemReq,
	bool requiresDedicatedAllocation,
	bool prefersDedicatedAllocation,
	VkBuffer dedicatedBuffer,
	VkImage dedicatedImage,
	VmaBufferImageUsage dedicatedBufferImageUsage,
	const VmaAllocationCreateInfo& createInfo,
	VmaSuballocationType suballocType,
	size_t allocationCount,
	VmaAllocation* pAllocations)
{
	memset(pAllocations, 0, sizeof(VmaAllocation) * allocationCount);

	if(vkMemReq.size == 0)
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VmaAllocationCreateInfo createInfoFinal = createInfo;
	VkResult res = CalcAllocationParams(createInfoFinal, requiresDedicatedAllocation);
	if(res != VK_SUCCESS)
		return res;

	// Bit mask of memory Vulkan types acceptable for this allocation.
	uint32_t memoryTypeBits = vkMemReq.memoryTypeBits;
	uint32_t memTypeIndex = UINT32_MAX;
	res = FindMemoryTypeIndex(memoryTypeBits, &createInfoFinal, dedicatedBufferImageUsage, &memTypeIndex);
	// Can't find any single memory type matching requirements. res is VK_ERROR_FEATURE_NOT_PRESENT.
	if(res != VK_SUCCESS)
		return res;
	
	do
	{
		VmaBlockVector* blockVector = m_pBlockVectors[memTypeIndex];
		res = AllocateMemoryOfType(
			vkMemReq.size,
			vkMemReq.alignment,
			requiresDedicatedAllocation || prefersDedicatedAllocation,
			dedicatedBuffer,
			dedicatedImage,
			dedicatedBufferImageUsage,
			createInfoFinal,
			memTypeIndex,
			suballocType,
			m_DedicatedAllocations[memTypeIndex],
			*blockVector,
			allocationCount,
			pAllocations);
		// Allocation succeeded
		if(res == VK_SUCCESS)
			return VK_SUCCESS;

		// Remove old memTypeIndex from list of possibilities.
		memoryTypeBits &= ~(1U << memTypeIndex);
		// Find alternative memTypeIndex.
		res = FindMemoryTypeIndex(memoryTypeBits, &createInfoFinal, dedicatedBufferImageUsage, &memTypeIndex);
	} while(res == VK_SUCCESS);

	// No other matching memory type index could be found.
	// Not returning res, which is VK_ERROR_FEATURE_NOT_PRESENT, because we already failed to allocate once.
	return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

void VmaAllocator_T::FreeMemory(
	size_t allocationCount,
	const VmaAllocation* pAllocations)
{
	for(size_t allocIndex = allocationCount; allocIndex--; )
	{
		VmaAllocation allocation = pAllocations[allocIndex];

		if(allocation != VK_NULL_HANDLE)
		{
			switch(allocation->GetType())
			{
			case VmaAllocation_T::ALLOCATION_TYPE_BLOCK:
				{
					VmaBlockVector* pBlockVector = nullptr;
					const uint32_t memTypeIndex = allocation->GetMemoryTypeIndex();
					pBlockVector = m_pBlockVectors[memTypeIndex];
					pBlockVector->Free(allocation);
				}
				break;
			case VmaAllocation_T::ALLOCATION_TYPE_DEDICATED:
				FreeDedicatedMemory(allocation);
				break;
			}
		}
	}
}

void VmaAllocator_T::GetHeapBudgets(VmaBudget* outBudgets, uint32_t firstHeap, uint32_t heapCount)
{
	if(m_Budget.m_OperationsSinceBudgetFetch < 30)
	{
		for(uint32_t i = 0; i < heapCount; ++i, ++outBudgets)
		{
			const uint32_t heapIndex = firstHeap + i;

			outBudgets->statistics.blockCount = m_Budget.m_BlockCount[heapIndex];
			outBudgets->statistics.allocationCount = m_Budget.m_AllocationCount[heapIndex];
			outBudgets->statistics.blockBytes = m_Budget.m_BlockBytes[heapIndex];
			outBudgets->statistics.allocationBytes = m_Budget.m_AllocationBytes[heapIndex];

			if(m_Budget.m_VulkanUsage[heapIndex] + outBudgets->statistics.blockBytes > m_Budget.m_BlockBytesAtBudgetFetch[heapIndex])
			{
				outBudgets->usage = m_Budget.m_VulkanUsage[heapIndex] +
					outBudgets->statistics.blockBytes - m_Budget.m_BlockBytesAtBudgetFetch[heapIndex];
			}
			else
			{
				outBudgets->usage = 0;
			}

			// Have to take MIN with heap size because explicit HeapSizeLimit is included in it.
			outBudgets->budget = VMA_MIN(
				m_Budget.m_VulkanBudget[heapIndex], m_MemProps.memoryHeaps[heapIndex].size);
		}
	}
	else
	{
		UpdateVulkanBudget(); // Outside of mutex lock
		GetHeapBudgets(outBudgets, firstHeap, heapCount); // Recursion
	}
}

void VmaAllocator_T::GetAllocationInfo(VmaAllocation hAllocation, VmaAllocationInfo* pAllocationInfo)
{
	pAllocationInfo->memoryType = hAllocation->GetMemoryTypeIndex();
	pAllocationInfo->deviceMemory = hAllocation->GetMemory();
	pAllocationInfo->offset = hAllocation->GetOffset();
	pAllocationInfo->size = hAllocation->GetSize();
}

void VmaAllocator_T::SetCurrentFrameIndex(uint32_t frameIndex)
{
	m_CurrentFrameIndex = frameIndex;
	UpdateVulkanBudget();
}

VkResult VmaAllocator_T::AllocateVulkanMemory(const VkMemoryAllocateInfo* pAllocateInfo, VkDeviceMemory* pMemory)
{
	const uint32_t heapIndex = MemoryTypeIndexToHeapIndex(pAllocateInfo->memoryTypeIndex);

	/*
	Set this to 1 to make VMA never exceed VkPhysicalDeviceMemoryProperties::memoryHeaps[i].size
	with a single allocation size VkMemoryAllocateInfo::allocationSize
	and return error instead of leaving up to Vulkan implementation what to do in such cases.
	It protects agaist validation error VUID-vkAllocateMemory-pAllocateInfo-01713.
	On the other hand, allowing exceeding this size may result in a successful allocation despite the validation error.
	*/
#if VMA_DEBUG_DONT_EXCEED_HEAP_SIZE_WITH_ALLOCATION_SIZE
	if (pAllocateInfo->allocationSize > m_MemProps.memoryHeaps[heapIndex].size)
	{
		return VK_ERROR_OUT_OF_DEVICE_MEMORY;
	}
#endif

	uint32_t deviceMemoryCountIncrement = 0;
	const uint64_t prevDeviceMemoryCount = deviceMemoryCountIncrement + m_DeviceMemoryCount;
	if(prevDeviceMemoryCount >= m_PhysicalDeviceProperties.limits.maxMemoryAllocationCount)
	{
		return VK_ERROR_TOO_MANY_OBJECTS;
	}

	// HeapSizeLimit is in effect for this heap.
	if((m_HeapSizeLimitMask & (1U << heapIndex)) != 0)
	{
		const VkDeviceSize heapSize = m_MemProps.memoryHeaps[heapIndex].size;
		const VkDeviceSize blockBytesAfterAllocation = m_Budget.m_BlockBytes[heapIndex] + pAllocateInfo->allocationSize;
		if(blockBytesAfterAllocation > heapSize)
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}
		m_Budget.m_BlockBytes[heapIndex] = blockBytesAfterAllocation;
	}
	else
	{
		m_Budget.m_BlockBytes[heapIndex] += pAllocateInfo->allocationSize;
	}
	++m_Budget.m_BlockCount[heapIndex];

	// VULKAN CALL vkAllocateMemory.
	VkResult res = (*m_VulkanFunctions.vkAllocateMemory)(m_hDevice, pAllocateInfo, nullptr, pMemory);

	if(res == VK_SUCCESS)
	{
		++m_Budget.m_OperationsSinceBudgetFetch;
	}
	else
	{
		--m_Budget.m_BlockCount[heapIndex];
		m_Budget.m_BlockBytes[heapIndex] -= pAllocateInfo->allocationSize;
	}

	return res;
}

void VmaAllocator_T::FreeVulkanMemory(uint32_t memoryType, VkDeviceSize size, VkDeviceMemory hMemory)
{
	// VULKAN CALL vkFreeMemory.
	(*m_VulkanFunctions.vkFreeMemory)(m_hDevice, hMemory, nullptr);

	const uint32_t heapIndex = MemoryTypeIndexToHeapIndex(memoryType);
	--m_Budget.m_BlockCount[heapIndex];
	m_Budget.m_BlockBytes[heapIndex] -= size;

	--m_DeviceMemoryCount;
}

void VmaAllocator_T::FreeDedicatedMemory(VmaAllocation allocation)
{
	const uint32_t memTypeIndex = allocation->GetMemoryTypeIndex();
	m_DedicatedAllocations[memTypeIndex].Unregister(allocation);
	VkDeviceMemory hMemory = allocation->GetMemory();
	FreeVulkanMemory(memTypeIndex, allocation->GetSize(), hMemory);

	m_Budget.RemoveAllocation(MemoryTypeIndexToHeapIndex(allocation->GetMemoryTypeIndex()), allocation->GetSize());
	allocation->Destroy();
	m_AllocationObjectAllocator.Free(allocation);
}

uint32_t VmaAllocator_T::CalculateGlobalMemoryTypeBits() const
{
	uint32_t memoryTypeBits = UINT32_MAX;

	// Exclude memory types that have VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD.
	for(uint32_t memTypeIndex = 0; memTypeIndex < GetMemoryTypeCount(); ++memTypeIndex)
	{
		if((m_MemProps.memoryTypes[memTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD_COPY) != 0)
		{
			memoryTypeBits &= ~(1U << memTypeIndex);
		}
	}

	return memoryTypeBits;
}

void VmaAllocator_T::UpdateVulkanBudget()
{
	VkPhysicalDeviceMemoryProperties2KHR memProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2_KHR };

	VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProps = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT };
	VmaPnextChainPushFront(&memProps, &budgetProps);

	GetVulkanFunctions().vkGetPhysicalDeviceMemoryProperties2KHR(m_PhysicalDevice, &memProps);

	for(uint32_t heapIndex = 0; heapIndex < GetMemoryHeapCount(); ++heapIndex)
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

#endif // _VMA_ALLOCATOR_T_FUNCTIONS


VkResult vmaCreateAllocator(
	const VmaAllocatorCreateInfo* pCreateInfo,
	VmaAllocator* pAllocator)
{
	*pAllocator = vma_new(VmaAllocator_T)(pCreateInfo);
	VkResult result = (*pAllocator)->Init(pCreateInfo);
	if(result < 0)
	{
		vma_delete(*pAllocator);
		*pAllocator = VK_NULL_HANDLE;
	}
	return result;
}

void vmaSetCurrentFrameIndex(
	VmaAllocator allocator,
	uint32_t frameIndex)
{
	allocator->SetCurrentFrameIndex(frameIndex);
}

VkResult vmaAllocateMemory(
	VmaAllocator allocator,
	const VkMemoryRequirements* pVkMemoryRequirements,
	const VmaAllocationCreateInfo* pCreateInfo,
	VmaAllocation* pAllocation,
	VmaAllocationInfo* pAllocationInfo)
{
	VkResult result = allocator->AllocateMemory(
		*pVkMemoryRequirements,
		false, // requiresDedicatedAllocation
		false, // prefersDedicatedAllocation
		VK_NULL_HANDLE, // dedicatedBuffer
		VK_NULL_HANDLE, // dedicatedImage
		VmaBufferImageUsage::UNKNOWN, // dedicatedBufferImageUsage
		*pCreateInfo,
		VMA_SUBALLOCATION_TYPE_UNKNOWN,
		1, // allocationCount
		pAllocation);

	if(pAllocationInfo != nullptr && result == VK_SUCCESS)
	{
		allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
	}

	return result;
}

VkResult vmaAllocateMemoryForBuffer(
	VmaAllocator allocator,
	VkBuffer buffer,
	const VmaAllocationCreateInfo* pCreateInfo,
	VmaAllocation* pAllocation,
	VmaAllocationInfo* pAllocationInfo)
{
	VkMemoryRequirements vkMemReq = {};
	bool requiresDedicatedAllocation = false;
	bool prefersDedicatedAllocation = false;
	allocator->GetBufferMemoryRequirements(buffer, vkMemReq,
		requiresDedicatedAllocation,
		prefersDedicatedAllocation);

	VkResult result = allocator->AllocateMemory(
		vkMemReq,
		requiresDedicatedAllocation,
		prefersDedicatedAllocation,
		buffer, // dedicatedBuffer
		VK_NULL_HANDLE, // dedicatedImage
		VmaBufferImageUsage::UNKNOWN, // dedicatedBufferImageUsage
		*pCreateInfo,
		VMA_SUBALLOCATION_TYPE_BUFFER,
		1, // allocationCount
		pAllocation);

	if(pAllocationInfo && result == VK_SUCCESS)
	{
		allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
	}

	return result;
}

VkResult vmaAllocateMemoryForImage(
	VmaAllocator allocator,
	VkImage image,
	const VmaAllocationCreateInfo* pCreateInfo,
	VmaAllocation* pAllocation,
	VmaAllocationInfo* pAllocationInfo)
{
	VkMemoryRequirements vkMemReq = {};
	bool requiresDedicatedAllocation = false;
	bool prefersDedicatedAllocation  = false;
	allocator->GetImageMemoryRequirements(image, vkMemReq,
		requiresDedicatedAllocation, prefersDedicatedAllocation);

	VkResult result = allocator->AllocateMemory(
		vkMemReq,
		requiresDedicatedAllocation,
		prefersDedicatedAllocation,
		VK_NULL_HANDLE, // dedicatedBuffer
		image, // dedicatedImage
		VmaBufferImageUsage::UNKNOWN, // dedicatedBufferImageUsage
		*pCreateInfo,
		VMA_SUBALLOCATION_TYPE_IMAGE_UNKNOWN,
		1, // allocationCount
		pAllocation);

	if(pAllocationInfo && result == VK_SUCCESS)
	{
		allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
	}

	return result;
}

void vmaFreeMemory(
	VmaAllocator allocator,
	VmaAllocation allocation)
{
	if(allocation == VK_NULL_HANDLE)
	{
		return;
	}

	allocator->FreeMemory(
		1, // allocationCount
		&allocation);
}