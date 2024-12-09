#include "JC/Common.h"

#include "JC/Array.h"
#include "JC/Mem.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <utility>
#include <type_traits>
#include <intrin.h> // For functions like __popcnt, _BitScanForward etc.
#include <bit>
#include <cassert> // for assert
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <vector>
#include "vulkan/vulkan_core.h"
#include "vulkan/vk_platform.h"
#include "vulkan/vulkan_win32.h"
#define NOMINMAX
#include <windows.h>

namespace JC {

//--------------------------------------------------------------------------------------------------

typedef enum VmaMemoryUsage
{
	VMA_MEMORY_USAGE_UNKNOWN = 0,
	VMA_MEMORY_USAGE_GPU_ONLY = 1,
	VMA_MEMORY_USAGE_CPU_ONLY = 2,
	VMA_MEMORY_USAGE_CPU_TO_GPU = 3,
	VMA_MEMORY_USAGE_GPU_TO_CPU = 4,
	VMA_MEMORY_USAGE_CPU_COPY = 5,
	VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED = 6,
	VMA_MEMORY_USAGE_AUTO = 7,
	VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE = 8,
	VMA_MEMORY_USAGE_AUTO_PREFER_HOST = 9,
} VmaMemoryUsage;

typedef enum VmaAllocationCreateFlagBits
{
	VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT = 0x00000001,
	VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT = 0x00000002,
	VMA_ALLOCATION_CREATE_MAPPED_BIT = 0x00000004,
	VMA_ALLOCATION_CREATE_DONT_BIND_BIT = 0x00000080,
	VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT = 0x00000100,
	VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT = 0x00000200,
	VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT = 0x00000400,
	VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT = 0x00000800,
	VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT = 0x00001000,
	VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT = 0x00010000,
	VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT = 0x00020000,
	VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT  = 0x00040000,
	VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
	VMA_ALLOCATION_CREATE_STRATEGY_FIRST_FIT_BIT = VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT,
	VMA_ALLOCATION_CREATE_STRATEGY_MASK =
		VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT |
		VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT |
		VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT,
} VmaAllocationCreateFlagBits;
typedef VkFlags VmaAllocationCreateFlags;

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaDefragmentationContext)

typedef struct VmaStatistics
{
	u32 blockCount;
	u32 allocationCount;
	VkDeviceSize blockBytes;
	VkDeviceSize allocationBytes;
} VmaStatistics;

typedef struct VmaBudget
{
	VmaStatistics statistics;
	VkDeviceSize usage;
	VkDeviceSize budget;
} VmaBudget;

typedef struct AllocationInfo2
{
	AllocationInfo allocationInfo;
	VkDeviceSize blockSize;
	VkBool32 dedicatedMemory;
} AllocationInfo2;

typedef struct VmaVirtualAllocationInfo
{
	VkDeviceSize offset;
	VkDeviceSize size;
} VmaVirtualAllocationInfo;

VkResult vmaFindMemoryTypeIndex(VmaAllocator allocator, u32 memoryTypeBits, const VmaAllocationCreateInfo* pAllocationCreateInfo, u32* pMemoryTypeIndex);
VkResult vmaFindMemoryTypeIndexForBufferInfo(VmaAllocator allocator, const VkBufferCreateInfo* pBufferCreateInfo, const VmaAllocationCreateInfo* pAllocationCreateInfo, u32* pMemoryTypeIndex);
VkResult vmaFindMemoryTypeIndexForImageInfo(VmaAllocator allocator, const VkImageCreateInfo* pImageCreateInfo, const VmaAllocationCreateInfo* pAllocationCreateInfo, u32* pMemoryTypeIndex);

VkResult vmaAllocateMemory(VmaAllocator allocator, const VkMemoryRequirements* pVkMemoryRequirements, const VmaAllocationCreateInfo* createInfo, Allocation** pAllocation, AllocationInfo* pAllocationInfo);
VkResult vmaAllocateMemoryPages(VmaAllocator allocator, const VkMemoryRequirements* pVkMemoryRequirements, const VmaAllocationCreateInfo* createInfo, u64 allocationCount, Allocation** pAllocations, AllocationInfo* pAllocationInfo);
VkResult vmaAllocateMemoryForBuffer(VmaAllocator allocator, VkBuffer buffer, const VmaAllocationCreateInfo* createInfo, Allocation** pAllocation, AllocationInfo* pAllocationInfo);
VkResult vmaAllocateMemoryForImage(VmaAllocator allocator, VkImage image, const VmaAllocationCreateInfo* createInfo, Allocation** pAllocation, AllocationInfo* pAllocationInfo);
void vmaFreeMemory(VmaAllocator allocator, const Allocation* allocation);
void vmaFreeMemoryPages(VmaAllocator allocator, u64 allocationCount, const Allocation** pAllocations);
void vmaGetAllocationInfo(VmaAllocator allocator, Allocation* allocation, AllocationInfo* pAllocationInfo);
void vmaGetAllocationMemoryProperties(VmaAllocator allocator, Allocation* allocation, VkMemoryPropertyFlags* pFlags);
VkResult vmaMapMemory(VmaAllocator allocator, Allocation* allocation, void** ppData);
void vmaUnmapMemory(VmaAllocator allocator, Allocation* allocation);
VkResult vmaFlushAllocation(VmaAllocator allocator, Allocation* allocation, VkDeviceSize offset, VkDeviceSize size);
VkResult vmaInvalidateAllocation(VmaAllocator allocator, Allocation* allocation, VkDeviceSize offset, VkDeviceSize size);
VkResult vmaFlushAllocations(VmaAllocator allocator, u32 allocationCount, const Allocation** allocations, const VkDeviceSize* offsets, const VkDeviceSize* sizes);
VkResult vmaInvalidateAllocations(VmaAllocator allocator, u32 allocationCount, const Allocation** allocations, const VkDeviceSize* offsets, const VkDeviceSize* sizes);
VkResult vmaCopyMemoryToAllocation(VmaAllocator allocator, const void* pSrcHostPointer, Allocation* dstAllocation, VkDeviceSize dstAllocationLocalOffset, VkDeviceSize size);
VkResult vmaCopyAllocationToMemory(VmaAllocator allocator, Allocation* srcAllocation, VkDeviceSize srcAllocationLocalOffset, void* pDstHostPointer, VkDeviceSize size);
VkResult vmaBindBufferMemory(VmaAllocator allocator, Allocation* allocation, VkBuffer buffer);
VkResult vmaBindBufferMemory2(VmaAllocator allocator, Allocation* allocation, VkDeviceSize allocationLocalOffset, VkBuffer buffer, const void* pNext);
VkResult vmaBindImageMemory(VmaAllocator allocator, Allocation* allocation, VkImage image);
VkResult vmaBindImageMemory2(VmaAllocator allocator, Allocation* allocation, VkDeviceSize allocationLocalOffset, VkImage image, const void* pNext);

VkResult vmaCreateBufferWithAlignment(VmaAllocator allocator, const VkBufferCreateInfo* pBufferCreateInfo, const VmaAllocationCreateInfo* pAllocationCreateInfo, VkDeviceSize minAlignment, VkBuffer* pBuffer, Allocation** pAllocation, AllocationInfo* pAllocationInfo);
VkResult vmaCreateAliasingBuffer(VmaAllocator allocator, Allocation* allocation, const VkBufferCreateInfo* pBufferCreateInfo, VkBuffer* pBuffer);
VkResult vmaCreateAliasingBuffer2(VmaAllocator allocator, Allocation* allocation, VkDeviceSize allocationLocalOffset, const VkBufferCreateInfo* pBufferCreateInfo, VkBuffer* pBuffer);
void vmaDestroyBuffer(VmaAllocator allocator, VkBuffer buffer, Allocation* allocation);
VkResult vmaCreateImage(VmaAllocator allocator, const VkImageCreateInfo* pImageCreateInfo, const VmaAllocationCreateInfo* pAllocationCreateInfo, VkImage* pImage, Allocation** pAllocation, AllocationInfo* pAllocationInfo);
VkResult vmaCreateAliasingImage(VmaAllocator allocator, Allocation* allocation, const VkImageCreateInfo* pImageCreateInfo, VkImage* pImage);
VkResult vmaCreateAliasingImage2(VmaAllocator allocator, Allocation* allocation, VkDeviceSize allocationLocalOffset, const VkImageCreateInfo* pImageCreateInfo, VkImage* pImage);
void vmaDestroyImage(VmaAllocator allocator, VkImage image, Allocation* allocation);

//--------------------------------------------------------------------------------------------------

#define Assert(expr)
#define VMA_HEAVY_ASSERT(expr)
#define VMA_ASSERT_LEAK(expr)
#define VMA_VALIDATE(cond) do { if(!(cond)) { Assert(0 && "Validation failed: " #cond); return false; } } while(false)

template<typename T> static T* VmaAllocateArray(u64 count) {
	return (T*)_aligned_malloc(sizeof(T) * count, alignof(T));
}
#define vma_new(T) new((T*)_aligned_malloc(sizeof(T), alignof(T)))(T)
#define vma_new_array(T, count) new((T*)_aligned_malloc(sizeof(T) * count, alignof(T)))(T)

static void VmaFree(void* ptr) {
	_aligned_free(ptr);
}
template<typename T> static void vma_delete(T* ptr) {
	if(ptr != 0) {
		ptr->~T();
		_aligned_free(ptr);
	}
}
template<typename T> static void vma_delete_array(T* ptr, u64 count) {
	if (ptr != 0) {
		for (u64 i = count; i--; ) {
			ptr[i].~T();
		}
		_aligned_free(ptr);
	}
}

//--------------------------------------------------------------------------------------------------

// To not search current larger region if first allocation won't succeed and skip to smaller range
// use with VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT as strategy in CreateAllocationRequest().
// When fragmentation and reusal of previous blocks doesn't matter then use with
// VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT for fastest alloc time possible.

struct MemBlockMeta
{
	// According to original paper it should be preferable 4 or 5:
	// M. Masmano, I. Ripoll, A. Crespo, and J. Real "TLSF: a New Dynamic Memory Allocator for Real-Time Systems"
	// http://www.gii.upv.es/tlsf/files/ecrts04_tlsf.pdf
	static const u8 SECOND_LEVEL_INDEX = 5;
	static const uint16_t SMALL_BUFFER_SIZE = 256;
	static const u32 INITIAL_BLOCK_ALLOC_COUNT = 16;
	static const u8 MEMORY_CLASS_SHIFT = 7;
	static const u8 MAX_MEMORY_CLASSES = 65 - MEMORY_CLASS_SHIFT;

	class Block
	{
	public:
		VkDeviceSize offset;
		VkDeviceSize size;
		Block* prevPhysical;
		Block* nextPhysical;

		void MarkFree() { prevFree = 0; }
		void MarkTaken() { prevFree = this; }
		bool IsFree() const { return prevFree != this; }
		Block*& PrevFree() { return prevFree; }
		Block*& NextFree() { VMA_HEAVY_ASSERT(IsFree()); return nextFree; }

	private:
		Block* prevFree; // Address of the same block here indicates that block is taken
		union
		{
			Block* nextFree;
		};
	};

	VkDeviceSize size = 0;
	u64 m_AllocCount;
	// Total number of free blocks besides null block
	u64 m_BlocksFreeCount;
	// Total size of free blocks excluding null block
	VkDeviceSize m_BlocksFreeSize;
	u32 m_IsFreeBitmap;
	u8 m_MemoryClasses;
	u32 m_InnerIsFreeBitmap[MAX_MEMORY_CLASSES];
	u32 m_ListsCount;
	/*
	* 0: 0-3 lists for small buffers
	* 1+: 0-(2^SLI-1) lists for normal buffers
	*/
	Block** m_FreeList;
	VmaPoolAllocator<Block> m_BlockAllocator;
	Block* m_NullBlock;
	VmaBlockBufferImageGranularity m_GranularityHandler;

	MemBlockMeta(VkDeviceSize bufferImageGranularity) :
		m_AllocCount(0),
		m_BlocksFreeCount(0),
		m_BlocksFreeSize(0),
		m_IsFreeBitmap(0),
		m_MemoryClasses(0),
		m_ListsCount(0),
		m_FreeList(0),
		m_BlockAllocator(INITIAL_BLOCK_ALLOC_COUNT),
		m_NullBlock(0),
		m_GranularityHandler(bufferImageGranularity)
	{}

	~MemBlockMeta() {
		if (m_FreeList) {
			vma_delete_array(m_FreeList, m_ListsCount);
		}
		m_GranularityHandler.Destroy();
	}


	static u8 SizeToMemoryClass(VkDeviceSize size) {
		return (size > SMALL_BUFFER_SIZE) ? u8(VmaBitScanMSB(size) - MEMORY_CLASS_SHIFT) : 0;
	}

	static uint16_t SizeToSecondIndex(VkDeviceSize size, u8 memoryClass) {
		return (memoryClass == 0)
			? static_cast<uint16_t>((size - 1) / 64)
			: static_cast<uint16_t>((size >> (memoryClass + MEMORY_CLASS_SHIFT - SECOND_LEVEL_INDEX)) ^ (1U << SECOND_LEVEL_INDEX));
	}

	static u32 GetListIndex(u8 memoryClass, uint16_t secondIndex) {
		return (memoryClass == 0)
			? secondIndex
			: static_cast<u32>(memoryClass - 1) * (1 << SECOND_LEVEL_INDEX) + secondIndex + 4;
	}

	static u32 GetListIndex(VkDeviceSize size) {
		u8 memoryClass = SizeToMemoryClass(size);
		return GetListIndex(memoryClass, SizeToSecondIndex(size, memoryClass));
	}

	void RemoveFreeBlock(Block* block) {
		Assert(block != m_NullBlock);
		Assert(block->IsFree());

		if (block->NextFree() != 0)
			block->NextFree()->PrevFree() = block->PrevFree();
		if (block->PrevFree() != 0)
			block->PrevFree()->NextFree() = block->NextFree();
		else
		{
			u8 memClass = SizeToMemoryClass(block->size);
			uint16_t secondIndex = SizeToSecondIndex(block->size, memClass);
			u32 index = GetListIndex(memClass, secondIndex);
			Assert(m_FreeList[index] == block);
			m_FreeList[index] = block->NextFree();
			if (block->NextFree() == 0)
			{
				m_InnerIsFreeBitmap[memClass] &= ~(1U << secondIndex);
				if (m_InnerIsFreeBitmap[memClass] == 0)
					m_IsFreeBitmap &= ~(1UL << memClass);
			}
		}
		block->MarkTaken();
		--m_BlocksFreeCount;
		m_BlocksFreeSize -= block->size;
	}

	void InsertFreeBlock(Block* block) {
		Assert(block != m_NullBlock);
		Assert(!block->IsFree() && "Cannot insert block twice!");

		u8 memClass = SizeToMemoryClass(block->size);
		uint16_t secondIndex = SizeToSecondIndex(block->size, memClass);
		u32 index = GetListIndex(memClass, secondIndex);
		Assert(index < m_ListsCount);
		block->PrevFree() = 0;
		block->NextFree() = m_FreeList[index];
		m_FreeList[index] = block;
		if (block->NextFree() != 0)
			block->NextFree()->PrevFree() = block;
		else
		{
			m_InnerIsFreeBitmap[memClass] |= 1U << secondIndex;
			m_IsFreeBitmap |= 1UL << memClass;
		}
		++m_BlocksFreeCount;
		m_BlocksFreeSize += block->size;
	}

	void MergeBlock(Block* block, Block* prev) {
		Assert(block->prevPhysical == prev && "Cannot merge separate physical regions!");
		Assert(!prev->IsFree() && "Cannot merge block that belongs to free list!");

		block->offset = prev->offset;
		block->size += prev->size;
		block->prevPhysical = prev->prevPhysical;
		if (block->prevPhysical)
			block->prevPhysical->nextPhysical = block;
		m_BlockAllocator.Free(prev);
	}

	Block* FindFreeBlock(VkDeviceSize size, u32& listIndex) const {
		u8 memoryClass = SizeToMemoryClass(size);
		u32 innerFreeMap = m_InnerIsFreeBitmap[memoryClass] & (~0U << SizeToSecondIndex(size, memoryClass));
		if (!innerFreeMap)
		{
			// Check higher levels for available blocks
			u32 freeMap = m_IsFreeBitmap & (~0UL << (memoryClass + 1));
			if (!freeMap)
				return 0; // No more memory available

			// Find lowest free region
			memoryClass = VmaBitScanLSB(freeMap);
			innerFreeMap = m_InnerIsFreeBitmap[memoryClass];
			Assert(innerFreeMap != 0);
		}
		// Find lowest free subregion
		listIndex = GetListIndex(memoryClass, VmaBitScanLSB(innerFreeMap));
		Assert(m_FreeList[listIndex]);
		return m_FreeList[listIndex];
	}

	bool CheckBlock(
		Block& block,
		u32 listIndex,
		VkDeviceSize allocSize,
		VkDeviceSize allocAlignment,
		SuballocationType allocType,
		VmaAllocationRequest* pAllocationRequest
	) {
		Assert(block.IsFree() && "Block is already taken!");

		VkDeviceSize alignedOffset = VmaAlignUp(block.offset, allocAlignment);
		if (block.size < allocSize + alignedOffset - block.offset)
			return false;

		// Check for granularity conflicts
		if (m_GranularityHandler.CheckConflictAndAlignUp(alignedOffset, allocSize, block.offset, block.size, allocType)) {
			return false;
		}

		// Alloc successful
		pAllocationRequest->type = VmaAllocationRequestType::TLSF;
		pAllocationRequest->allocHandle = (AllocHandle)&block;
		pAllocationRequest->size = allocSize;
		pAllocationRequest->customData = (void*)allocType;
		pAllocationRequest->algorithmData = alignedOffset;

		// Place block at the start of list if it's normal block
		if (listIndex != m_ListsCount && block.PrevFree())
		{
			block.PrevFree()->NextFree() = block.NextFree();
			if (block.NextFree())
				block.NextFree()->PrevFree() = block.PrevFree();
			block.PrevFree() = 0;
			block.NextFree() = m_FreeList[listIndex];
			m_FreeList[listIndex] = &block;
			if (block.NextFree())
				block.NextFree()->PrevFree() = &block;
		}

		return true;
	}

	VkDeviceSize GetSumFreeSize() const { return m_BlocksFreeSize + m_NullBlock->size; }
	bool IsEmpty() const { return m_NullBlock->offset == 0; }
	VkDeviceSize GetAllocationOffset(AllocHandle allocHandle) const { return ((Block*)allocHandle)->offset; }

	void Init(VkDeviceSize inSize) {
		size = inSize;
		m_GranularityHandler.Init(size);
		m_NullBlock = m_BlockAllocator.Alloc();
		m_NullBlock->size = size;
		m_NullBlock->offset = 0;
		m_NullBlock->prevPhysical = 0;
		m_NullBlock->nextPhysical = 0;
		m_NullBlock->MarkFree();
		m_NullBlock->NextFree() = 0;
		m_NullBlock->PrevFree() = 0;
		u8 memoryClass = SizeToMemoryClass(size);
		uint16_t sli = SizeToSecondIndex(size, memoryClass);
		m_ListsCount = (memoryClass == 0 ? 0 : (memoryClass - 1) * (1UL << SECOND_LEVEL_INDEX) + sli) + 1;
		m_ListsCount += 4;

		m_MemoryClasses = memoryClass + u8(2);
		memset(m_InnerIsFreeBitmap, 0, MAX_MEMORY_CLASSES * sizeof(u32));

		m_FreeList = vma_new_array(Block*, m_ListsCount);
		memset(m_FreeList, 0, m_ListsCount * sizeof(Block*));
	}


	// Tries to find a place for suballocation with given parameters inside this block.
	// If succeeded, fills pAllocationRequest and returns true.
	// If failed, returns false.
	bool CreateAllocationRequest(
		VkDeviceSize allocSize,
		VkDeviceSize allocAlignment,
		SuballocationType allocType,
		u32 strategy,
		VmaAllocationRequest* pAllocationRequest
	) {
		// For small granularity round up
		m_GranularityHandler.RoundupAllocRequest(allocType, allocSize, allocAlignment);

		// Quick check for too small pool
		if (allocSize > GetSumFreeSize())
			return false;

		// If no free blocks in pool then check only null block
		if (m_BlocksFreeCount == 0)
			return CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest);

		// Round up to the next block
		VkDeviceSize sizeForNextList = allocSize;
		VkDeviceSize smallSizeStep = VkDeviceSize(SMALL_BUFFER_SIZE / 4);
		if (allocSize > SMALL_BUFFER_SIZE)
		{
			sizeForNextList += (1ULL << (VmaBitScanMSB(allocSize) - SECOND_LEVEL_INDEX));
		}
		else if (allocSize > SMALL_BUFFER_SIZE - smallSizeStep)
			sizeForNextList = SMALL_BUFFER_SIZE + 1;
		else
			sizeForNextList += smallSizeStep;

		u32 nextListIndex = m_ListsCount;
		u32 prevListIndex = m_ListsCount;
		Block* nextListBlock = 0;
		Block* prevListBlock = 0;

		// Check blocks according to strategies
		if (strategy & VMA_ALLOCATION_CREATE_STRATEGY_MIN_TIME_BIT)
		{
			// Quick check for larger block first
			nextListBlock = FindFreeBlock(sizeForNextList, nextListIndex);
			if (nextListBlock != 0 && CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
				return true;

			// If not fitted then null block
			if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest))
				return true;

			// Null block failed, search larger bucket
			while (nextListBlock)
			{
				if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
					return true;
				nextListBlock = nextListBlock->NextFree();
			}

			// Failed again, check best fit bucket
			prevListBlock = FindFreeBlock(allocSize, prevListIndex);
			while (prevListBlock)
			{
				if (CheckBlock(*prevListBlock, prevListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
					return true;
				prevListBlock = prevListBlock->NextFree();
			}
		}
		else if (strategy & VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT)
		{
			// Check best fit bucket
			prevListBlock = FindFreeBlock(allocSize, prevListIndex);
			while (prevListBlock)
			{
				if (CheckBlock(*prevListBlock, prevListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
					return true;
				prevListBlock = prevListBlock->NextFree();
			}

			// If failed check null block
			if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest))
				return true;

			// Check larger bucket
			nextListBlock = FindFreeBlock(sizeForNextList, nextListIndex);
			while (nextListBlock)
			{
				if (CheckBlock(*nextListBlock, nextListIndex, allocSize, allocAlignment, allocType, pAllocationRequest))
					return true;
				nextListBlock = nextListBlock->NextFree();
			}
		}
		else if (strategy & VMA_ALLOCATION_CREATE_STRATEGY_MIN_OFFSET_BIT )
		{
			// Perform search from the start
			std::vector<Block*> blockList(m_BlocksFreeCount);

			u64 i = m_BlocksFreeCount;
			for (Block* block = m_NullBlock->prevPhysical; block != 0; block = block->prevPhysical)
			{
				if (block->IsFree() && block->size >= allocSize)
					blockList[--i] = block;
			}

			for (; i < m_BlocksFreeCount; ++i)
			{
				Block& block = *blockList[i];
				if (CheckBlock(block, GetListIndex(block.size), allocSize, allocAlignment, allocType, pAllocationRequest))
					return true;
			}

			// If failed check null block
			if (CheckBlock(*m_NullBlock, m_ListsCount, allocSize, allocAlignment, allocType, pAllocationRequest))
				return true;

			// Whole range searched, no more memory
			return false;
		}
		else
		{
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

	void Alloc(const VmaAllocationRequest& request, SuballocationType type)  {
		Assert(request.type == VmaAllocationRequestType::TLSF);

		// Get block and pop it from the free list
		Block* currentBlock = (Block*)request.allocHandle;
		VkDeviceSize offset = request.algorithmData;
		Assert(currentBlock != 0);
		Assert(currentBlock->offset <= offset);

		if (currentBlock != m_NullBlock)
			RemoveFreeBlock(currentBlock);

		VkDeviceSize misssingAlignment = offset - currentBlock->offset;

		// Append missing alignment to prev block or create new one
		if (misssingAlignment)
		{
			Block* prevBlock = currentBlock->prevPhysical;
			Assert(prevBlock != 0 && "There should be no missing alignment at offset 0!");

			if (prevBlock->IsFree() && prevBlock->size != 0)
			{
				u32 oldList = GetListIndex(prevBlock->size);
				prevBlock->size += misssingAlignment;
				// Check if new size crosses list bucket
				if (oldList != GetListIndex(prevBlock->size))
				{
					prevBlock->size -= misssingAlignment;
					RemoveFreeBlock(prevBlock);
					prevBlock->size += misssingAlignment;
					InsertFreeBlock(prevBlock);
				}
				else
					m_BlocksFreeSize += misssingAlignment;
			}
			else
			{
				Block* newBlock = m_BlockAllocator.Alloc();
				currentBlock->prevPhysical = newBlock;
				prevBlock->nextPhysical = newBlock;
				newBlock->prevPhysical = prevBlock;
				newBlock->nextPhysical = currentBlock;
				newBlock->size = misssingAlignment;
				newBlock->offset = currentBlock->offset;
				newBlock->MarkTaken();

				InsertFreeBlock(newBlock);
			}

			currentBlock->size -= misssingAlignment;
			currentBlock->offset += misssingAlignment;
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
				m_NullBlock->nextPhysical = 0;
				m_NullBlock->MarkFree();
				m_NullBlock->PrevFree() = 0;
				m_NullBlock->NextFree() = 0;
				currentBlock->nextPhysical = m_NullBlock;
				currentBlock->MarkTaken();
			}
		}
		else
		{
			Assert(currentBlock->size > size && "Proper block already found, shouldn't find smaller one!");

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
				m_NullBlock->NextFree() = 0;
				m_NullBlock->PrevFree() = 0;
				currentBlock->MarkTaken();
			}
			else
			{
				newBlock->nextPhysical->prevPhysical = newBlock;
				newBlock->MarkTaken();
				InsertFreeBlock(newBlock);
			}
		}
		m_GranularityHandler.AllocPages((u8)(uintptr_t)request.customData, currentBlock->offset, currentBlock->size);
		++m_AllocCount;
	}

	void Free(AllocHandle allocHandle) {
		Block* block = (Block*)allocHandle;
		Block* next = block->nextPhysical;
		Assert(!block->IsFree() && "Block is already free!");

		m_GranularityHandler.FreePages(block->offset, block->size);
		--m_AllocCount;

		// Try merging
		Block* prev = block->prevPhysical;
		if (prev != 0 && prev->IsFree() && prev->size != 0)
		{
			RemoveFreeBlock(prev);
			MergeBlock(block, prev);
		}

		if (!next->IsFree()) {
			InsertFreeBlock(block);
		} else if (next == m_NullBlock) {
			MergeBlock(m_NullBlock, block);
		} else {
			RemoveFreeBlock(next);
			MergeBlock(next, block);
			InsertFreeBlock(next);
		}
	}

	void GetAllocationInfo(AllocHandle allocHandle, VmaVirtualAllocationInfo& outInfo) {
		Block* block = (Block*)allocHandle;
		Assert(!block->IsFree() && "Cannot get allocation info for free block!");
		outInfo.offset = block->offset;
		outInfo.size = block->size;
	}

	void Clear() {
		m_AllocCount = 0;
		m_BlocksFreeCount = 0;
		m_BlocksFreeSize = 0;
		m_IsFreeBitmap = 0;
		m_NullBlock->offset = 0;
		m_NullBlock->size = size;
		Block* block = m_NullBlock->prevPhysical;
		m_NullBlock->prevPhysical = 0;
		while (block)
		{
			Block* prev = block->prevPhysical;
			m_BlockAllocator.Free(block);
			block = prev;
		}
		memset(m_FreeList, 0, m_ListsCount * sizeof(Block*));
		memset(m_InnerIsFreeBitmap, 0, m_MemoryClasses * sizeof(u32));
		m_GranularityHandler.Clear();
	}
};

//--------------------------------------------------------------------------------------------------

struct MemBlock {
	MemBlockMeta*        meta           = 0;
	u32                  memoryType     = UINT32_MAX;
	VkDeviceMemory       vkDeviceMemory = VK_NULL_HANDLE;
	VmaMappingHysteresis hysteresis     = {};
	u32                  mapCount       = 0;
	void*                mappedData     = 0;

	~MemBlock() {
		VMA_ASSERT_LEAK(mapCount == 0 && "VkDeviceMemory block is being destroyed while it is still mapped.");
		VMA_ASSERT_LEAK(vkDeviceMemory == VK_NULL_HANDLE);
	}

	void Init(u32 newMemoryTypeIndex, VkDeviceMemory newMemory, VkDeviceSize newSize, VkDeviceSize bufferImageGranularity) {
		memoryType     = newMemoryTypeIndex;
		vkDeviceMemory = newMemory;
		meta           = vma_new(MemBlockMeta)(bufferImageGranularity);
		meta->Init(newSize);
	}

	void Destroy() 	{
		// This is the most important assert in the entire library.
		// Hitting it means you have some memory leak - unreleased Allocation* objects.
		VMA_ASSERT_LEAK(meta->IsEmpty() && "Some allocations were not freed before destruction of this memory block!");
		VMA_ASSERT_LEAK(vkDeviceMemory != VK_NULL_HANDLE);
		FreeVulkanMemory(memoryType, meta->size, vkDeviceMemory);
		vkDeviceMemory = VK_NULL_HANDLE;
		vma_delete(meta);
		meta = 0;
	}

	void PostAlloc() {
		hysteresis.PostAlloc();
	}

	void PostFree() {
		if(hysteresis.PostFree()) {
			Assert(hysteresis.m_ExtraMapping == 0);
			if (mapCount == 0) {
				mappedData = 0;
				vkUnmapMemory(vkDevice, vkDeviceMemory);
			}
		}
	}

	// ppData can be null.
	VkResult Map(u32 count, void** ppData) {
		if (count == 0) {
			return VK_SUCCESS;
		}

		const u32 oldTotalMapCount = mapCount + hysteresis.m_ExtraMapping;
		if (oldTotalMapCount != 0) {
			Assert(mappedData != 0);
			hysteresis.PostMap();
			mapCount += count;
			if (ppData != 0) {
				*ppData = mappedData;
			}
			return VK_SUCCESS;
		} else {
			VkResult result = vkMapMemory(
				vkDevice,
				vkDeviceMemory,
				0, // offset
				VK_WHOLE_SIZE,
				0, // flags
				&mappedData);
			if (result == VK_SUCCESS)
			{
				Assert(mappedData != 0);
				hysteresis.PostMap();
				mapCount = count;
				if (ppData != 0)
				{
					*ppData = mappedData;
				}
			}
			return result;
		}
	}

	void Unmap(u32 count) {
		if (count == 0) {
			return;
		}

		if (mapCount >= count) {
			mapCount -= count;
			const u32 totalMapCount = mapCount + hysteresis.m_ExtraMapping;
			if (totalMapCount == 0) {
				mappedData = 0;
				vkUnmapMemory(vkDevice, vkDeviceMemory);
			}
			hysteresis.PostUnmap();
		} else {
			Assert(0 && "VkDeviceMemory block is being unmapped while it was not previously mapped.");
		}
	}

	VkResult BindBufferMemory(
		const Allocation* hAllocation,
		VkDeviceSize allocationLocalOffset,
		VkBuffer hBuffer,
		const void* pNext
	) {
		Assert(hAllocation->allocationType == Allocation::AllocationType::Block && hAllocation->blockAllocation.block == this);
		Assert(allocationLocalOffset < hAllocation->size && "Invalid allocationLocalOffset. Did you forget that this offset is relative to the beginning of the allocation, not the whole memory block?");
		return BindVulkanBuffer(vkDeviceMemory, hAllocation->GetOffset() + allocationLocalOffset, hBuffer, pNext);
	}

	VkResult BindImageMemory(
		const Allocation* hAllocation,
		VkDeviceSize allocationLocalOffset,
		VkImage hImage,
		const void* pNext
	) {
		Assert(hAllocation->allocationType == Allocation::AllocationType::Block && hAllocation->blockAllocation.block == this);
		Assert(allocationLocalOffset < hAllocation->size && "Invalid allocationLocalOffset. Did you forget that this offset is relative to the beginning of the allocation, not the whole memory block?");
		return BindVulkanImage(vkDeviceMemory, hAllocation->GetOffset() + allocationLocalOffset, hImage, pNext);
	}

};


struct AllocHandle { u64 opaque = 0; };

namespace AllocationFlags {
	constexpr u8 PersistentMap  = 1 << 0;
	constexpr u8 MappingAllowed = 1 << 1;
};

enum struct AllocationType {
	None = 0,
	Block,
	Dedicated,
};

enum struct SuballocationType {
	Free = 0,
	Unknown,
	Buffer,
	ImageUnknown,
	ImageLinear,
	ImageOptimal,
};

struct VmaBufferImageUsage {
	static const VmaBufferImageUsage UNKNOWN;

	u64 Value;

	constexpr VmaBufferImageUsage() { *this = UNKNOWN; }
	constexpr explicit VmaBufferImageUsage(u64 usage) : Value(usage) { }
	constexpr VmaBufferImageUsage(const VkBufferCreateInfo &createInfo) { Value = createInfo.usage; }
	constexpr explicit VmaBufferImageUsage(const VkImageCreateInfo &createInfo) { Value = createInfo.usage; }
	constexpr bool operator==(const VmaBufferImageUsage& rhs) const { return Value == rhs.Value; }
	constexpr bool operator!=(const VmaBufferImageUsage& rhs) const { return Value != rhs.Value; }
	constexpr bool Contains(u64 flag) const { return (Value & flag) != 0; }
	constexpr bool ContainsDeviceAccess() const {
		// This relies on values of VK_IMAGE_USAGE_TRANSFER* being the same as VK_BUFFER_IMAGE_TRANSFER*.
		return (Value & ~(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) != 0;
	}
};
const VmaBufferImageUsage VmaBufferImageUsage::UNKNOWN = VmaBufferImageUsage(0);

struct Allocation {
	struct BlockAllocation {
		MemBlock*   block;
		AllocHandle allocHandle;
	};

	struct DedicatedAllocation {
		VkDeviceMemory vkDeviceMemory;
		void*          mappedData;
		Allocation*    prev;
		Allocation*    next;
	};

	union {
		BlockAllocation     blockAllocation;
		DedicatedAllocation dedicatedAllocation;
	};

	VkDeviceSize        alignment;
	VkDeviceSize        size;
	u32                 memoryType;
	AllocationType      allocationType;
	SuballocationType   suballocationType;
	u8                  mapCount;	// Reference counter for vmaMapMemory()/vmaUnmapMemory().
	u8                  flags; // enum AllocationFlags
	VmaBufferImageUsage bufferImageUsage; // 0 if unknown.

	Allocation(bool mappingAllowed) {
		alignment         = 1;
		size              = 0;
		memoryType        = 0;
		allocationType    = AllocationType::None;
		suballocationType = SuballocationType::Unknown;
		mapCount          = 0;
		flags             = mappingAllowed ? AllocationFlags::MappingAllowed : 0;
	}

	void InitBlockAllocation(
		MemBlock* block,
		AllocHandle allocHandle,
		VkDeviceSize alignment,
		VkDeviceSize size,
		u32 memoryType,
		SuballocationType suballocationType,
		bool mapped
	) {
		allocationType = AllocationType::Block;
		alignment  = alignment;
		size       = size;
		memoryType = memoryType;
		if(mapped) {
			flags |= (u8)AllocationFlags::PersistentMap;
		}
		suballocationType = suballocationType;
		blockAllocation.block = block;
		blockAllocation.allocHandle = allocHandle;
	}

	// mappedData not null means allocation is created with MAPPED flag.
	void InitDedicatedAllocation(
		VmaAllocator allocator,
		u32 memoryType,
		VkDeviceMemory hMemory,
		SuballocationType suballocationType,
		void* mappedData,
		VkDeviceSize size
	) {
		allocationType = AllocationType::Dedicated;
		alignment = 0;
		size = size;
		memoryType = memoryType;
		suballocationType = suballocationType;
		dedicatedAllocation.mappedData = 0;
		dedicatedAllocation.vkDeviceMemory = hMemory;
		dedicatedAllocation.prev = 0;
		dedicatedAllocation.next = 0;
		if (mappedData != 0) {
			flags |= (u8)AllocationFlags::PersistentMap;
			dedicatedAllocation.mappedData = mappedData;
		}
	}

	VkDeviceSize GetOffset() const {
		switch (allocationType) {
			case AllocationType::Block:     return blockAllocation.block->meta->GetAllocationOffset(blockAllocation.allocHandle);
			case AllocationType::Dedicated: return 0;
		}
	}

	VkDeviceMemory GetMemory() const {
		switch (allocationType) {
			case AllocationType::Block:     return blockAllocation.block->vkDeviceMemory;
			case AllocationType::Dedicated: return dedicatedAllocation.vkDeviceMemory;
		}
	}

	void* GetMappedData() const {
		switch (allocationType) {
			case AllocationType::Block:
				if (mapCount != 0 || (flags & AllocationFlags::PersistentMap)) {
					void* pBlockData = blockAllocation.block->mappedData;
					Assert(pBlockData != 0);
					return (char*)pBlockData + GetOffset();
				} else {
					return 0;
				}
				break;
			case AllocationType::Dedicated:
				return dedicatedAllocation.mappedData != 0 ? dedicatedAllocation.mappedData : 0;
		}
	}

	void BlockAllocMap() {
		Assert(allocationType == AllocationType::Block);
		if (mapCount < 0xFF) {
			++mapCount;
		} else {
			Assert(0 && "Allocation mapped too many times simultaneously.");
		}
	}

	void BlockAllocUnmap() {
		Assert(allocationType == AllocationType::Block);
		if (mapCount > 0) {
			--mapCount;
		} else {
			Assert(0 && "Unmapping allocation not previously mapped.");
		}
	}

	VkResult DedicatedAllocMap(void** ppData) {
		Assert(allocationType == AllocationType::Dedicated);
		if (mapCount != 0 || (flags & AllocationFlags::PersistentMap)) {
			if (mapCount < 0xFF) {
				*ppData = dedicatedAllocation.mappedData;
				++mapCount;
				return VK_SUCCESS;
			} else {
				Assert(0 && "Dedicated allocation mapped too many times simultaneously.");
				return VK_ERROR_MEMORY_MAP_FAILED;
			}
		} else {
			VkResult result = vkMapMemory(
				vkDevice,
				dedicatedAllocation.vkDeviceMemory,
				0, // offset
				VK_WHOLE_SIZE,
				0, // flags
				ppData);
			if (result == VK_SUCCESS) {
				dedicatedAllocation.mappedData = *ppData;
				mapCount = 1;
			}
			return result;
		}
	}

	void DedicatedAllocUnmap() {
		Assert(allocationType == AllocationType::Dedicated);
		if (mapCount > 0) {
			--mapCount;
			if (mapCount == 0 && !(flags & AllocationFlags::PersistentMap)) {
				dedicatedAllocation.mappedData = 0;
				vkUnmapMemory(vkDevice, dedicatedAllocation.vkDeviceMemory);
			}
		} else {
			Assert(0 && "Unmapping dedicated allocation not previously mapped.");
		}
	}

	void InitBufferUsage(const VkBufferCreateInfo &createInfo)
	{
		Assert(bufferImageUsage == VmaBufferImageUsage::UNKNOWN);
		bufferImageUsage = VmaBufferImageUsage(createInfo);
	}
	void InitImageUsage(const VkImageCreateInfo &createInfo)
	{
		Assert(bufferImageUsage == VmaBufferImageUsage::UNKNOWN);
		bufferImageUsage = VmaBufferImageUsage(createInfo);
	}
};























////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
//	IMPLEMENTATION
//
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



// Copy of some Vulkan definitions so we don't need to check their existence just to handle few constants.
static const u32 VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD = 0x00000040;
static const u32 VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD = 0x00000080;
static const u32 VK_IMAGE_CREATE_DISJOINT_BIT = 0x00000200;
static const int32_t VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT = 1000158000;
// This one is tricky. Vulkan specification defines this code as available since
// Vulkan 1.0, but doesn't actually define it in Vulkan SDK earlier than 1.2.131.
// See pull request #207.
#define VK_ERROR_UNKNOWN_COPY ((VkResult)-13)

enum VMA_CACHE_OPERATION {
	VMA_CACHE_FLUSH,
	VMA_CACHE_INVALIDATE
};

enum class VmaAllocationRequestType {
	Normal,
	TLSF,
};

VK_DEFINE_NON_DISPATCHABLE_HANDLE(AllocHandle);

static inline u32 VmaCountBitsSet(u32 v) { return std::popcount(v); }
static inline u8 VmaBitScanLSB(u64 mask) { unsigned long pos; if (_BitScanForward64(&pos, mask)) return static_cast<u8>(pos); return UINT8_MAX; }
static inline u8 VmaBitScanLSB(u32 mask) { unsigned long pos; if (_BitScanForward(&pos, mask)) return static_cast<u8>(pos); return UINT8_MAX; }
static inline u8 VmaBitScanMSB(u64 mask) { unsigned long pos; if (_BitScanReverse64(&pos, mask)) return static_cast<u8>(pos); return UINT8_MAX; }
static inline u8 VmaBitScanMSB(u32 mask) { unsigned long pos; if (_BitScanReverse(&pos, mask)) return static_cast<u8>(pos); return UINT8_MAX; }

template <typename T> static constexpr bool VmaIsPow2(T x) { return (x & (x - 1)) == 0; }
template <typename T> static constexpr T VmaAlignUp(T val, T alignment) { return (val + alignment - 1) & ~(alignment - 1); }
template <typename T> static constexpr T VmaAlignDown(T val, T alignment) { return val & ~(alignment - 1); }
template <typename T> static constexpr T VmaRoundDiv(T x, T y) { return (x + (y / (T)2)) / y; }
template <typename T> static constexpr T VmaDivideRoundingUp(T x, T y) { return (x + y - (T)1) / y; }

/*
Returns true if given suballocation types could conflict and must respect
VkPhysicalDeviceLimits::bufferImageGranularity. They conflict if one is buffer
or linear image and another one is optimal image. If type is unknown, behave
conservatively.
*/
static inline bool VmaIsBufferImageGranularityConflict(SuballocationType suballocType1, SuballocationType suballocType2) {
	if (suballocType1 > suballocType2) {
		std::swap(suballocType1, suballocType2);
	}

	switch (suballocType1) {
		case SuballocationType::Free:
			return false;
		case SuballocationType::Unknown:
			return true;
		case SuballocationType::Buffer:
			return
				suballocType2 == SuballocationType::ImageUnknown ||
				suballocType2 == SuballocationType::ImageOptimal;
		case SuballocationType::ImageUnknown:
			return
				suballocType2 == SuballocationType::ImageUnknown ||
				suballocType2 == SuballocationType::ImageLinear ||
				suballocType2 == SuballocationType::ImageOptimal;
		case SuballocationType::ImageLinear:
			return
				suballocType2 == SuballocationType::ImageOptimal;
		case SuballocationType::ImageOptimal:
			return false;
		default:
			Assert(0);
			return true;
	}
}

template<typename MainT, typename NewT>
static inline void VmaPnextChainPushFront(MainT* mainStruct, NewT* newStruct)
{
	newStruct->pNext = mainStruct->pNext;
	mainStruct->pNext = newStruct;
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
	case VMA_MEMORY_USAGE_GPU_ONLY:
		if(!isIntegratedGPU || (outPreferredFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
		{
			outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		}
		break;
	case VMA_MEMORY_USAGE_CPU_ONLY:
		outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		break;
	case VMA_MEMORY_USAGE_CPU_TO_GPU:
		outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		if(!isIntegratedGPU || (outPreferredFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
		{
			outPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		}
		break;
	case VMA_MEMORY_USAGE_GPU_TO_CPU:
		outRequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		outPreferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		break;
	case VMA_MEMORY_USAGE_CPU_COPY:
		outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		break;
	case VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED:
		outRequiredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
		break;
	case VMA_MEMORY_USAGE_AUTO:
	case VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE:
	case VMA_MEMORY_USAGE_AUTO_PREFER_HOST:
	{
		if(bufImgUsage == VmaBufferImageUsage::UNKNOWN)
		{
			Assert(0 && "VMA_MEMORY_USAGE_AUTO* values can only be used with functions like vmaCreateBuffer, vmaCreateImage so that the details of the created resource are known. Maybe you use VkBufferUsageFlags2CreateInfoKHR but forgot to use VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT?" );
			return false;
		}

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
	default:
		Assert(0);
	}

	// Avoid DEVICE_COHERENT unless explicitly requested.
	if(((allocCreateInfo.requiredFlags | allocCreateInfo.preferredFlags) & (VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD)) == 0)
	{
		outNotPreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// Memory allocation


#ifndef _VMA_STATISTICS_FUNCTIONS

static void VmaClearStatistics(VmaStatistics& outStats)
{
	outStats.blockCount = 0;
	outStats.allocationCount = 0;
	outStats.blockBytes = 0;
	outStats.allocationBytes = 0;
}

static void VmaAddStatistics(VmaStatistics& inoutStats, const VmaStatistics& src)
{
	inoutStats.blockCount += src.blockCount;
	inoutStats.allocationCount += src.allocationCount;
	inoutStats.blockBytes += src.blockBytes;
	inoutStats.allocationBytes += src.allocationBytes;
}

#endif // _VMA_STATISTICS_FUNCTIONS








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
	VmaPoolAllocator(u32 firstBlockCapacity);
	~VmaPoolAllocator();
	template<typename... Types> T* Alloc(Types&&... args);
	void Free(T* ptr);

private:
	union Item
	{
		u32 NextFreeIndex;
		alignas(T) char Value[sizeof(T)];
	};
	struct ItemBlock
	{
		Item* pItems;
		u32 Capacity;
		u32 FirstFreeIndex;
	};

	const u32 m_FirstBlockCapacity;
	std::vector<ItemBlock> m_ItemBlocks;

	ItemBlock& CreateNewBlock();
};

#ifndef _VMA_POOL_ALLOCATOR_FUNCTIONS
template<typename T>
VmaPoolAllocator<T>::VmaPoolAllocator(u32 firstBlockCapacity) :
	m_FirstBlockCapacity(firstBlockCapacity),
	m_ItemBlocks(VmaStlAllocator<ItemBlock>())
{
	Assert(m_FirstBlockCapacity > 1);
}

template<typename T>
VmaPoolAllocator<T>::~VmaPoolAllocator()
{
	for (u64 i = m_ItemBlocks.size(); i--;)
		vma_delete_array(m_ItemBlocks[i].pItems, m_ItemBlocks[i].Capacity);
	m_ItemBlocks.clear();
}

template<typename T>
template<typename... Types> T* VmaPoolAllocator<T>::Alloc(Types&&... args)
{
	for (u64 i = m_ItemBlocks.size(); i--; )
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
	for (u64 i = m_ItemBlocks.size(); i--; )
	{
		ItemBlock& block = m_ItemBlocks[i];

		// Casting to union.
		Item* pItemPtr;
		memcpy(&pItemPtr, &ptr, sizeof(pItemPtr));

		// Check if pItemPtr is in address range of this block.
		if ((pItemPtr >= block.pItems) && (pItemPtr < block.pItems + block.Capacity))
		{
			ptr->~T(); // Explicit destructor call.
			const u32 index = static_cast<u32>(pItemPtr - block.pItems);
			pItemPtr->NextFreeIndex = block.FirstFreeIndex;
			block.FirstFreeIndex = index;
			return;
		}
	}
	Assert(0 && "Pointer doesn't belong to this memory pool.");
}

template<typename T>
typename VmaPoolAllocator<T>::ItemBlock& VmaPoolAllocator<T>::CreateNewBlock()
{
	const u32 newBlockCapacity = m_ItemBlocks.empty() ?
		m_FirstBlockCapacity : m_ItemBlocks.back().Capacity * 3 / 2;

	const ItemBlock newBlock =
	{
		vma_new_array(Item, newBlockCapacity),
		newBlockCapacity,
		0
	};

	m_ItemBlocks.push_back(newBlock);

	// Setup singly-linked list of all free items in this block.
	for (u32 i = 0; i < newBlockCapacity - 1; ++i)
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

	VmaRawList();
	// Intentionally not calling Clear, because that would be unnecessary
	// computations to return all items to m_ItemAllocator as free.
	~VmaRawList() = default;

	u64 GetCount() const { return m_Count; }
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
	u64 m_Count;
};

#ifndef _VMA_RAW_LIST_FUNCTIONS
template<typename T>
VmaRawList<T>::VmaRawList() :
	m_ItemAllocator(128),
	m_pFront(0),
	m_pBack(0),
	m_Count(0) {}

template<typename T>
VmaListItem<T>* VmaRawList<T>::PushFront()
{
	ItemType* const pNewItem = m_ItemAllocator.Alloc();
	pNewItem->pPrev = 0;
	if (IsEmpty())
	{
		pNewItem->pNext = 0;
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
	pNewItem->pNext = 0;
	if(IsEmpty())
	{
		pNewItem->pPrev = 0;
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
	VMA_HEAVY_ASSERT(m_Count > 0);
	ItemType* const pFrontItem = m_pFront;
	ItemType* const pNextItem = pFrontItem->pNext;
	if (pNextItem != 0)
	{
		pNextItem->pPrev = 0;
	}
	m_pFront = pNextItem;
	m_ItemAllocator.Free(pFrontItem);
	--m_Count;
}

template<typename T>
void VmaRawList<T>::PopBack()
{
	VMA_HEAVY_ASSERT(m_Count > 0);
	ItemType* const pBackItem = m_pBack;
	ItemType* const pPrevItem = pBackItem->pPrev;
	if(pPrevItem != 0)
	{
		pPrevItem->pNext = 0;
	}
	m_pBack = pPrevItem;
	m_ItemAllocator.Free(pBackItem);
	--m_Count;
}

template<typename T>
void VmaRawList<T>::Clear()
{
	if (IsEmpty() == false)
	{
		ItemType* pItem = m_pBack;
		while (pItem != 0)
		{
			ItemType* const pPrevItem = pItem->pPrev;
			m_ItemAllocator.Free(pItem);
			pItem = pPrevItem;
		}
		m_pFront = 0;
		m_pBack = 0;
		m_Count = 0;
	}
}

template<typename T>
void VmaRawList<T>::Remove(ItemType* pItem)
{
	VMA_HEAVY_ASSERT(pItem != 0);
	VMA_HEAVY_ASSERT(m_Count > 0);

	if(pItem->pPrev != 0)
	{
		pItem->pPrev->pNext = pItem->pNext;
	}
	else
	{
		VMA_HEAVY_ASSERT(m_pFront == pItem);
		m_pFront = pItem->pNext;
	}

	if(pItem->pNext != 0)
	{
		pItem->pNext->pPrev = pItem->pPrev;
	}
	else
	{
		VMA_HEAVY_ASSERT(m_pBack == pItem);
		m_pBack = pItem->pPrev;
	}

	m_ItemAllocator.Free(pItem);
	--m_Count;
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::InsertBefore(ItemType* pItem)
{
	if(pItem != 0)
	{
		ItemType* const prevItem = pItem->pPrev;
		ItemType* const newItem = m_ItemAllocator.Alloc();
		newItem->pPrev = prevItem;
		newItem->pNext = pItem;
		pItem->pPrev = newItem;
		if(prevItem != 0)
		{
			prevItem->pNext = newItem;
		}
		else
		{
			VMA_HEAVY_ASSERT(m_pFront == pItem);
			m_pFront = newItem;
		}
		++m_Count;
		return newItem;
	}
	else
		return PushBack();
}

template<typename T>
VmaListItem<T>* VmaRawList<T>::InsertAfter(ItemType* pItem)
{
	if(pItem != 0)
	{
		ItemType* const nextItem = pItem->pNext;
		ItemType* const newItem = m_ItemAllocator.Alloc();
		newItem->pNext = nextItem;
		newItem->pPrev = pItem;
		pItem->pNext = newItem;
		if(nextItem != 0)
		{
			nextItem->pPrev = newItem;
		}
		else
		{
			VMA_HEAVY_ASSERT(m_pBack == pItem);
			m_pBack = newItem;
		}
		++m_Count;
		return newItem;
	}
	else
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

template<typename T>
class VmaList
{
public:
	class reverse_iterator;
	class const_iterator;
	class const_reverse_iterator;

	struct iterator
	{
		iterator() :  m_pList(0), m_pItem(0) {}
		iterator(const reverse_iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

		T& operator*() const { VMA_HEAVY_ASSERT(m_pItem != 0); return m_pItem->Value; }
		T* operator->() const { VMA_HEAVY_ASSERT(m_pItem != 0); return &m_pItem->Value; }

		bool operator==(const iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem == rhs.m_pItem; }
		bool operator!=(const iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem != rhs.m_pItem; }

		iterator operator++(int) { iterator result = *this; ++*this; return result; }
		iterator operator--(int) { iterator result = *this; --*this; return result; }

		iterator& operator++() { VMA_HEAVY_ASSERT(m_pItem != 0); m_pItem = m_pItem->pNext; return *this; }
		iterator& operator--();

		VmaRawList<T>* m_pList;
		VmaListItem<T>* m_pItem;

		iterator(VmaRawList<T>* pList, VmaListItem<T>* pItem) : m_pList(pList),  m_pItem(pItem) {}
	};
	struct reverse_iterator
	{
		reverse_iterator() : m_pList(0), m_pItem(0) {}
		reverse_iterator(const iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

		T& operator*() const { VMA_HEAVY_ASSERT(m_pItem != 0); return m_pItem->Value; }
		T* operator->() const { VMA_HEAVY_ASSERT(m_pItem != 0); return &m_pItem->Value; }

		bool operator==(const reverse_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem == rhs.m_pItem; }
		bool operator!=(const reverse_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem != rhs.m_pItem; }

		reverse_iterator operator++(int) { reverse_iterator result = *this; ++* this; return result; }
		reverse_iterator operator--(int) { reverse_iterator result = *this; --* this; return result; }

		reverse_iterator& operator++() { VMA_HEAVY_ASSERT(m_pItem != 0); m_pItem = m_pItem->pPrev; return *this; }
		reverse_iterator& operator--();

		VmaRawList<T>* m_pList;
		VmaListItem<T>* m_pItem;

		reverse_iterator(VmaRawList<T>* pList, VmaListItem<T>* pItem) : m_pList(pList),  m_pItem(pItem) {}
	};
	struct const_iterator
	{
		const_iterator() : m_pList(0), m_pItem(0) {}
		const_iterator(const iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}
		const_iterator(const reverse_iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

		iterator drop_const() { return { const_cast<VmaRawList<T>*>(m_pList), const_cast<VmaListItem<T>*>(m_pItem) }; }

		const T& operator*() const { VMA_HEAVY_ASSERT(m_pItem != 0); return m_pItem->Value; }
		const T* operator->() const { VMA_HEAVY_ASSERT(m_pItem != 0); return &m_pItem->Value; }

		bool operator==(const const_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem == rhs.m_pItem; }
		bool operator!=(const const_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem != rhs.m_pItem; }

		const_iterator operator++(int) { const_iterator result = *this; ++* this; return result; }
		const_iterator operator--(int) { const_iterator result = *this; --* this; return result; }

		const_iterator& operator++() { VMA_HEAVY_ASSERT(m_pItem != 0); m_pItem = m_pItem->pNext; return *this; }
		const_iterator& operator--();

		const VmaRawList<T>* m_pList;
		const VmaListItem<T>* m_pItem;

		const_iterator(const VmaRawList<T>* pList, const VmaListItem<T>* pItem) : m_pList(pList), m_pItem(pItem) {}
	};
	struct const_reverse_iterator
	{
		const_reverse_iterator() : m_pList(0), m_pItem(0) {}
		const_reverse_iterator(const reverse_iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}
		const_reverse_iterator(const iterator& src) : m_pList(src.m_pList), m_pItem(src.m_pItem) {}

		reverse_iterator drop_const() { return { const_cast<VmaRawList<T>*>(m_pList), const_cast<VmaListItem<T>*>(m_pItem) }; }

		const T& operator*() const { VMA_HEAVY_ASSERT(m_pItem != 0); return m_pItem->Value; }
		const T* operator->() const { VMA_HEAVY_ASSERT(m_pItem != 0); return &m_pItem->Value; }

		bool operator==(const const_reverse_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem == rhs.m_pItem; }
		bool operator!=(const const_reverse_iterator& rhs) const { VMA_HEAVY_ASSERT(m_pList == rhs.m_pList); return m_pItem != rhs.m_pItem; }

		const_reverse_iterator operator++(int) { const_reverse_iterator result = *this; ++* this; return result; }
		const_reverse_iterator operator--(int) { const_reverse_iterator result = *this; --* this; return result; }

		const_reverse_iterator& operator++() { VMA_HEAVY_ASSERT(m_pItem != 0); m_pItem = m_pItem->pPrev; return *this; }
		const_reverse_iterator& operator--();

		const VmaRawList<T>* m_pList;
		const VmaListItem<T>* m_pItem;

		const_reverse_iterator(const VmaRawList<T>* pList, const VmaListItem<T>* pItem) : m_pList(pList), m_pItem(pItem) {}
	};

	VmaList() : m_RawList() {}

	bool empty() const { return m_RawList.IsEmpty(); }
	u64 size() const { return m_RawList.GetCount(); }

	iterator begin() { return iterator(&m_RawList, m_RawList.Front()); }
	iterator end() { return iterator(&m_RawList, 0); }

	const_iterator cbegin() const { return const_iterator(&m_RawList, m_RawList.Front()); }
	const_iterator cend() const { return const_iterator(&m_RawList, 0); }

	const_iterator begin() const { return cbegin(); }
	const_iterator end() const { return cend(); }

	reverse_iterator rbegin() { return reverse_iterator(&m_RawList, m_RawList.Back()); }
	reverse_iterator rend() { return reverse_iterator(&m_RawList, 0); }

	const_reverse_iterator crbegin() const { return const_reverse_iterator(&m_RawList, m_RawList.Back()); }
	const_reverse_iterator crend() const { return const_reverse_iterator(&m_RawList, 0); }

	const_reverse_iterator rbegin() const { return crbegin(); }
	const_reverse_iterator rend() const { return crend(); }

	void push_back(const T& value) { m_RawList.PushBack(value); }
	iterator insert(iterator it, const T& value) { return iterator(&m_RawList, m_RawList.InsertBefore(it.m_pItem, value)); }

	void clear() { m_RawList.Clear(); }
	void erase(iterator it) { m_RawList.Remove(it.m_pItem); }

private:
	VmaRawList<T> m_RawList;
};

template<typename T>
typename VmaList<T>::iterator& VmaList<T>::iterator::operator--()
{
	if (m_pItem != 0)
	{
		m_pItem = m_pItem->pPrev;
	}
	else
	{
		VMA_HEAVY_ASSERT(!m_pList->IsEmpty());
		m_pItem = m_pList->Back();
	}
	return *this;
}

template<typename T>
typename VmaList<T>::reverse_iterator& VmaList<T>::reverse_iterator::operator--()
{
	if (m_pItem != 0)
	{
		m_pItem = m_pItem->pNext;
	}
	else
	{
		VMA_HEAVY_ASSERT(!m_pList->IsEmpty());
		m_pItem = m_pList->Front();
	}
	return *this;
}

template<typename T>
typename VmaList<T>::const_iterator& VmaList<T>::const_iterator::operator--()
{
	if (m_pItem != 0)
	{
		m_pItem = m_pItem->pPrev;
	}
	else
	{
		VMA_HEAVY_ASSERT(!m_pList->IsEmpty());
		m_pItem = m_pList->Back();
	}
	return *this;
}

template<typename T>
typename VmaList<T>::const_reverse_iterator& VmaList<T>::const_reverse_iterator::operator--()
{
	if (m_pItem != 0)
	{
		m_pItem = m_pItem->pNext;
	}
	else
	{
		VMA_HEAVY_ASSERT(!m_pList->IsEmpty());
		m_pItem = m_pList->Back();
	}
	return *this;
}

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
	VmaIntrusiveLinkedList(VmaIntrusiveLinkedList && src);
	VmaIntrusiveLinkedList(const VmaIntrusiveLinkedList&) = delete;
	VmaIntrusiveLinkedList& operator=(VmaIntrusiveLinkedList&& src);
	VmaIntrusiveLinkedList& operator=(const VmaIntrusiveLinkedList&) = delete;
	~VmaIntrusiveLinkedList() { VMA_HEAVY_ASSERT(IsEmpty()); }

	u64 GetCount() const { return m_Count; }
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
	ItemType* m_Front = 0;
	ItemType* m_Back = 0;
	u64 m_Count = 0;
};

#ifndef _VMA_INTRUSIVE_LINKED_LIST_FUNCTIONS
template<typename ItemTypeTraits>
VmaIntrusiveLinkedList<ItemTypeTraits>::VmaIntrusiveLinkedList(VmaIntrusiveLinkedList&& src)
	: m_Front(src.m_Front), m_Back(src.m_Back), m_Count(src.m_Count)
{
	src.m_Front = src.m_Back = 0;
	src.m_Count = 0;
}

template<typename ItemTypeTraits>
VmaIntrusiveLinkedList<ItemTypeTraits>& VmaIntrusiveLinkedList<ItemTypeTraits>::operator=(VmaIntrusiveLinkedList&& src)
{
	if (&src != this)
	{
		VMA_HEAVY_ASSERT(IsEmpty());
		m_Front = src.m_Front;
		m_Back = src.m_Back;
		m_Count = src.m_Count;
		src.m_Front = src.m_Back = 0;
		src.m_Count = 0;
	}
	return *this;
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::PushBack(ItemType* item)
{
	VMA_HEAVY_ASSERT(ItemTypeTraits::GetPrev(item) == 0 && ItemTypeTraits::GetNext(item) == 0);
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
	VMA_HEAVY_ASSERT(ItemTypeTraits::GetPrev(item) == 0 && ItemTypeTraits::GetNext(item) == 0);
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
	VMA_HEAVY_ASSERT(m_Count > 0);
	ItemType* const backItem = m_Back;
	ItemType* const prevItem = ItemTypeTraits::GetPrev(backItem);
	if (prevItem != 0)
	{
		ItemTypeTraits::AccessNext(prevItem) = 0;
	}
	m_Back = prevItem;
	--m_Count;
	ItemTypeTraits::AccessPrev(backItem) = 0;
	ItemTypeTraits::AccessNext(backItem) = 0;
	return backItem;
}

template<typename ItemTypeTraits>
typename VmaIntrusiveLinkedList<ItemTypeTraits>::ItemType* VmaIntrusiveLinkedList<ItemTypeTraits>::PopFront()
{
	VMA_HEAVY_ASSERT(m_Count > 0);
	ItemType* const frontItem = m_Front;
	ItemType* const nextItem = ItemTypeTraits::GetNext(frontItem);
	if (nextItem != 0)
	{
		ItemTypeTraits::AccessPrev(nextItem) = 0;
	}
	m_Front = nextItem;
	--m_Count;
	ItemTypeTraits::AccessPrev(frontItem) = 0;
	ItemTypeTraits::AccessNext(frontItem) = 0;
	return frontItem;
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::InsertBefore(ItemType* existingItem, ItemType* newItem)
{
	VMA_HEAVY_ASSERT(newItem != 0 && ItemTypeTraits::GetPrev(newItem) == 0 && ItemTypeTraits::GetNext(newItem) == 0);
	if (existingItem != 0)
	{
		ItemType* const prevItem = ItemTypeTraits::GetPrev(existingItem);
		ItemTypeTraits::AccessPrev(newItem) = prevItem;
		ItemTypeTraits::AccessNext(newItem) = existingItem;
		ItemTypeTraits::AccessPrev(existingItem) = newItem;
		if (prevItem != 0)
		{
			ItemTypeTraits::AccessNext(prevItem) = newItem;
		}
		else
		{
			VMA_HEAVY_ASSERT(m_Front == existingItem);
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
	VMA_HEAVY_ASSERT(newItem != 0 && ItemTypeTraits::GetPrev(newItem) == 0 && ItemTypeTraits::GetNext(newItem) == 0);
	if (existingItem != 0)
	{
		ItemType* const nextItem = ItemTypeTraits::GetNext(existingItem);
		ItemTypeTraits::AccessNext(newItem) = nextItem;
		ItemTypeTraits::AccessPrev(newItem) = existingItem;
		ItemTypeTraits::AccessNext(existingItem) = newItem;
		if (nextItem != 0)
		{
			ItemTypeTraits::AccessPrev(nextItem) = newItem;
		}
		else
		{
			VMA_HEAVY_ASSERT(m_Back == existingItem);
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
	VMA_HEAVY_ASSERT(item != 0 && m_Count > 0);
	if (ItemTypeTraits::GetPrev(item) != 0)
	{
		ItemTypeTraits::AccessNext(ItemTypeTraits::AccessPrev(item)) = ItemTypeTraits::GetNext(item);
	}
	else
	{
		VMA_HEAVY_ASSERT(m_Front == item);
		m_Front = ItemTypeTraits::GetNext(item);
	}

	if (ItemTypeTraits::GetNext(item) != 0)
	{
		ItemTypeTraits::AccessPrev(ItemTypeTraits::AccessNext(item)) = ItemTypeTraits::GetPrev(item);
	}
	else
	{
		VMA_HEAVY_ASSERT(m_Back == item);
		m_Back = ItemTypeTraits::GetPrev(item);
	}
	ItemTypeTraits::AccessPrev(item) = 0;
	ItemTypeTraits::AccessNext(item) = 0;
	--m_Count;
}

template<typename ItemTypeTraits>
void VmaIntrusiveLinkedList<ItemTypeTraits>::RemoveAll()
{
	if (!IsEmpty())
	{
		ItemType* item = m_Back;
		while (item != 0)
		{
			ItemType* const prevItem = ItemTypeTraits::AccessPrev(item);
			ItemTypeTraits::AccessPrev(item) = 0;
			ItemTypeTraits::AccessNext(item) = 0;
			item = prevItem;
		}
		m_Front = 0;
		m_Back = 0;
		m_Count = 0;
	}
}
#endif // _VMA_INTRUSIVE_LINKED_LIST_FUNCTIONS
#endif // _VMA_INTRUSIVE_LINKED_LIST

struct VmaMappingHysteresis
{
	static constexpr int32_t COUNTER_MIN_EXTRA_MAPPING = 7;

	bool PostMap()
	{
		if(m_ExtraMapping == 0)
		{
			++m_MajorCounter;
			if(m_MajorCounter >= COUNTER_MIN_EXTRA_MAPPING)
			{
				m_ExtraMapping = 1;
				m_MajorCounter = 0;
				m_MinorCounter = 0;
				return true;
			}
		}
		else // m_ExtraMapping == 1
			PostMinorCounter();
		return false;
	}

	// Call when Unmap was called.
	void PostUnmap()
	{
		if(m_ExtraMapping == 0)
			++m_MajorCounter;
		else // m_ExtraMapping == 1
			PostMinorCounter();
	}

	// Call when allocation was made from the memory block.
	void PostAlloc()
	{
		if(m_ExtraMapping == 1)
			++m_MajorCounter;
		else // m_ExtraMapping == 0
			PostMinorCounter();
	}

	// Call when allocation was freed from the memory block.
	// Returns true if switched to extra -1 mapping reference count.
	bool PostFree()
	{
		if(m_ExtraMapping == 1)
		{
			++m_MajorCounter;
			if(m_MajorCounter >= COUNTER_MIN_EXTRA_MAPPING &&
				m_MajorCounter > m_MinorCounter + 1)
			{
				m_ExtraMapping = 0;
				m_MajorCounter = 0;
				m_MinorCounter = 0;
				return true;
			}
		}
		else // m_ExtraMapping == 0
			PostMinorCounter();
		return false;
	}

	u32 m_MinorCounter = 0;
	u32 m_MajorCounter = 0;
	u32 m_ExtraMapping = 0; // 0 or 1.

	void PostMinorCounter()
	{
		if(m_MinorCounter < m_MajorCounter)
		{
			++m_MinorCounter;
		}
		else if(m_MajorCounter > 0)
		{
			--m_MajorCounter;
			--m_MinorCounter;
		}
	}
};

struct VmaDedicatedAllocationListItemTraits {
	typedef Allocation ItemType;
	static ItemType* GetPrev(const ItemType* item) { return item->dedicatedAllocation.prev; }
	static ItemType* GetNext(const ItemType* item) { return item->dedicatedAllocation.next; }
	static ItemType*& AccessPrev(ItemType* item)   { return item->dedicatedAllocation.prev; }
	static ItemType*& AccessNext(ItemType* item)   { return item->dedicatedAllocation.next; }
};

class VmaDedicatedAllocationList {
public:
	VmaDedicatedAllocationList() {}
	~VmaDedicatedAllocationList();

	void Init(bool) { }

	bool IsEmpty();
	void Register(Allocation* alloc);
	void Unregister(Allocation* alloc);

private:
	typedef VmaIntrusiveLinkedList<VmaDedicatedAllocationListItemTraits> DedicatedAllocationLinkedList;

	DedicatedAllocationLinkedList m_AllocationList;
};

#ifndef _VMA_DEDICATED_ALLOCATION_LIST_FUNCTIONS

VmaDedicatedAllocationList::~VmaDedicatedAllocationList()
{
	if (!m_AllocationList.IsEmpty())
	{
		VMA_ASSERT_LEAK(false && "Unfreed dedicated allocations found!");
	}
}

bool VmaDedicatedAllocationList::IsEmpty()
{
	return m_AllocationList.IsEmpty();
}

void VmaDedicatedAllocationList::Register(Allocation* alloc)
{
	m_AllocationList.PushBack(alloc);
}

void VmaDedicatedAllocationList::Unregister(Allocation* alloc)
{
	m_AllocationList.Remove(alloc);
}
#endif // _VMA_DEDICATED_ALLOCATION_LIST_FUNCTIONS

#ifndef _VMA_SUBALLOCATION
/*
Represents a region of MemBlock that is either assigned and returned as
allocated memory block or free.
*/
struct VmaSuballocation
{
	VkDeviceSize offset;
	VkDeviceSize size;
	SuballocationType type;
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
	bool operator()(const VmaSuballocationList::iterator lhs,const VmaSuballocationList::iterator rhs) const
	{
		return lhs->size < rhs->size;
	}

	bool operator()(const VmaSuballocationList::iterator lhs, VkDeviceSize rhsSize) const
	{
		return lhs->size < rhsSize;
	}
};
#endif // _VMA_SUBALLOCATION

typedef VmaList<VmaSuballocation> VmaSuballocationList;
struct VmaAllocationRequest
{
	AllocHandle allocHandle;
	VkDeviceSize size;
	VmaSuballocationList::iterator item;
	void* customData;
	u64 algorithmData;
	VmaAllocationRequestType type;
};



#ifndef _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY
// Before deleting object of this class remember to call 'Destroy()'
class VmaBlockBufferImageGranularity final
{
public:
	struct ValidationContext
	{
		uint16_t* pageAllocs;
	};

	VmaBlockBufferImageGranularity(VkDeviceSize bufferImageGranularity);
	~VmaBlockBufferImageGranularity();

	bool IsEnabled() const { return bufferImageGranularity > MAX_LOW_BUFFER_IMAGE_GRANULARITY; }

	void Init(VkDeviceSize size);
	// Before destroying object you must call free it's memory
	void Destroy();

	void RoundupAllocRequest(SuballocationType allocType,
		VkDeviceSize& inOutAllocSize,
		VkDeviceSize& inOutAllocAlignment) const;

	bool CheckConflictAndAlignUp(VkDeviceSize& inOutAllocOffset,
		VkDeviceSize allocSize,
		VkDeviceSize blockOffset,
		VkDeviceSize blockSize,
		SuballocationType allocType) const;

	void AllocPages(u8 allocType, VkDeviceSize offset, VkDeviceSize size);
	void FreePages(VkDeviceSize offset, VkDeviceSize size);
	void Clear();

	ValidationContext StartValidation() const;
	bool Validate(ValidationContext& ctx, VkDeviceSize offset, VkDeviceSize size) const;
	bool FinishValidation(ValidationContext& ctx) const;

private:
	static const uint16_t MAX_LOW_BUFFER_IMAGE_GRANULARITY = 256;

	struct RegionInfo
	{
		u8 allocType;
		uint16_t allocCount;
	};

	VkDeviceSize bufferImageGranularity;
	u32 m_RegionCount;
	RegionInfo* m_RegionInfo;

	u32 GetStartPage(VkDeviceSize offset) const { return OffsetToPageIndex(offset & ~(bufferImageGranularity - 1)); }
	u32 GetEndPage(VkDeviceSize offset, VkDeviceSize size) const { return OffsetToPageIndex((offset + size - 1) & ~(bufferImageGranularity - 1)); }

	u32 OffsetToPageIndex(VkDeviceSize offset) const;
	void AllocPage(RegionInfo& page, u8 allocType);
};

#ifndef _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY_FUNCTIONS
VmaBlockBufferImageGranularity::VmaBlockBufferImageGranularity(VkDeviceSize bufferImageGranularity)
	: bufferImageGranularity(bufferImageGranularity),
	m_RegionCount(0),
	m_RegionInfo(0) {}

VmaBlockBufferImageGranularity::~VmaBlockBufferImageGranularity()
{
	Assert(m_RegionInfo == 0 && "Free not called before destroying object!");
}

void VmaBlockBufferImageGranularity::Init(VkDeviceSize size)
{
	if (IsEnabled())
	{
		m_RegionCount = static_cast<u32>(VmaDivideRoundingUp(size, bufferImageGranularity));
		m_RegionInfo = vma_new_array(RegionInfo, m_RegionCount);
		memset(m_RegionInfo, 0, m_RegionCount * sizeof(RegionInfo));
	}
}

void VmaBlockBufferImageGranularity::Destroy()
{
	if (m_RegionInfo)
	{
		vma_delete_array(m_RegionInfo, m_RegionCount);
		m_RegionInfo = 0;
	}
}

void VmaBlockBufferImageGranularity::RoundupAllocRequest(SuballocationType allocType,
	VkDeviceSize& inOutAllocSize,
	VkDeviceSize& inOutAllocAlignment) const
{
	if (bufferImageGranularity > 1 &&
		bufferImageGranularity <= MAX_LOW_BUFFER_IMAGE_GRANULARITY)
	{
		if (allocType == SuballocationType::Unknown ||
			allocType == SuballocationType::ImageUnknown ||
			allocType == SuballocationType::ImageOptimal)
		{
			inOutAllocAlignment = Max(inOutAllocAlignment, bufferImageGranularity);
			inOutAllocSize = VmaAlignUp(inOutAllocSize, bufferImageGranularity);
		}
	}
}

bool VmaBlockBufferImageGranularity::CheckConflictAndAlignUp(VkDeviceSize& inOutAllocOffset,
	VkDeviceSize allocSize,
	VkDeviceSize blockOffset,
	VkDeviceSize blockSize,
	SuballocationType allocType) const
{
	if (IsEnabled())
	{
		u32 startPage = GetStartPage(inOutAllocOffset);
		if (m_RegionInfo[startPage].allocCount > 0 && VmaIsBufferImageGranularityConflict(static_cast<SuballocationType>(m_RegionInfo[startPage].allocType), allocType)) {
			inOutAllocOffset = VmaAlignUp(inOutAllocOffset, bufferImageGranularity);
			if (blockSize < allocSize + inOutAllocOffset - blockOffset)
				return true;
			++startPage;
		}
		u32 endPage = GetEndPage(inOutAllocOffset, allocSize);
		if (endPage != startPage &&
			m_RegionInfo[endPage].allocCount > 0 &&
			VmaIsBufferImageGranularityConflict(static_cast<SuballocationType>(m_RegionInfo[endPage].allocType), allocType))
		{
			return true;
		}
	}
	return false;
}

void VmaBlockBufferImageGranularity::AllocPages(u8 allocType, VkDeviceSize offset, VkDeviceSize size)
{
	if (IsEnabled())
	{
		u32 startPage = GetStartPage(offset);
		AllocPage(m_RegionInfo[startPage], allocType);

		u32 endPage = GetEndPage(offset, size);
		if (startPage != endPage)
			AllocPage(m_RegionInfo[endPage], allocType);
	}
}

void VmaBlockBufferImageGranularity::FreePages(VkDeviceSize offset, VkDeviceSize size)
{
	if (IsEnabled())
	{
		u32 startPage = GetStartPage(offset);
		--m_RegionInfo[startPage].allocCount;
		if (m_RegionInfo[startPage].allocCount == 0)
			m_RegionInfo[startPage].allocType = SuballocationType::Free;
		u32 endPage = GetEndPage(offset, size);
		if (startPage != endPage)
		{
			--m_RegionInfo[endPage].allocCount;
			if (m_RegionInfo[endPage].allocCount == 0)
				m_RegionInfo[endPage].allocType = SuballocationType::Free;
		}
	}
}

void VmaBlockBufferImageGranularity::Clear()
{
	if (m_RegionInfo)
		memset(m_RegionInfo, 0, m_RegionCount * sizeof(RegionInfo));
}

VmaBlockBufferImageGranularity::ValidationContext VmaBlockBufferImageGranularity::StartValidation() const
{
	ValidationContext ctx{ 0 };
	if (IsEnabled())
	{
		ctx.pageAllocs = vma_new_array(uint16_t, m_RegionCount);
		memset(ctx.pageAllocs, 0, m_RegionCount * sizeof(uint16_t));
	}
	return ctx;
}

bool VmaBlockBufferImageGranularity::Validate(ValidationContext& ctx,
	VkDeviceSize offset, VkDeviceSize size) const
{
	if (IsEnabled())
	{
		u32 start = GetStartPage(offset);
		++ctx.pageAllocs[start];
		VMA_VALIDATE(m_RegionInfo[start].allocCount > 0);

		u32 end = GetEndPage(offset, size);
		if (start != end)
		{
			++ctx.pageAllocs[end];
			VMA_VALIDATE(m_RegionInfo[end].allocCount > 0);
		}
	}
	return true;
}

bool VmaBlockBufferImageGranularity::FinishValidation(ValidationContext& ctx) const
{
	// Check proper page structure
	if (IsEnabled())
	{
		Assert(ctx.pageAllocs != 0 && "Validation context not initialized!");

		for (u32 page = 0; page < m_RegionCount; ++page)
		{
			VMA_VALIDATE(ctx.pageAllocs[page] == m_RegionInfo[page].allocCount);
		}
		vma_delete_array(ctx.pageAllocs, m_RegionCount);
		ctx.pageAllocs = 0;
	}
	return true;
}

u32 VmaBlockBufferImageGranularity::OffsetToPageIndex(VkDeviceSize offset) const
{
	return static_cast<u32>(offset >> VmaBitScanMSB(bufferImageGranularity));
}

void VmaBlockBufferImageGranularity::AllocPage(RegionInfo& page, u8 allocType)
{
	// When current alloc type is free then it can be overridden by new type
	if (page.allocCount == 0 || (page.allocCount > 0 && page.allocType == SuballocationType::Free))
		page.allocType = allocType;

	++page.allocCount;
}
#endif // _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY_FUNCTIONS
#endif // _VMA_BLOCK_BUFFER_IMAGE_GRANULARITY

// Main allocator object.
struct VmaAllocatorT
{
	~VmaAllocatorT();

	void GetImageMemoryRequirements(
		VkImage hImage,
		VkMemoryRequirements& vkMemoryRequirements,
		bool& requiresDedicatedAllocation,
		bool& prefersDedicatedAllocation) const;
	VkResult FindMemoryTypeIndex(
		u32 memoryTypeBits,
		const VmaAllocationCreateInfo* pAllocationCreateInfo,
		VmaBufferImageUsage bufImgUsage,
		u32* pMemoryTypeIndex) const;

	// Main allocation function.
	VkResult AllocateMemory(
		const VkMemoryRequirements& vkMemoryRequirements,
		bool requiresDedicatedAllocation,
		bool prefersDedicatedAllocation,
		VkBuffer dedicatedBuffer,
		VkImage dedicatedImage,
		VmaBufferImageUsage dedicatedBufferImageUsage,
		const VmaAllocationCreateInfo& createInfo,
		SuballocationType suballocType,
		u64 allocationCount,
		Allocation** pAllocations);

	// Main deallocation function.
	void FreeMemory(
		u64 allocationCount,
		const Allocation** pAllocations);

	void GetHeapBudgets(
		VmaBudget* outBudgets, u32 firstHeap, u32 heapCount);


	void GetAllocationInfo(Allocation* hAllocation, AllocationInfo* pAllocationInfo);

	VkResult AllocateVulkanMemory(const VkMemoryAllocateInfo* pAllocateInfo, VkDeviceMemory* pMemory);
	// Call to Vulkan function vkFreeMemory with accompanying bookkeeping.
	void FreeVulkanMemory(u32 memoryType, VkDeviceSize size, VkDeviceMemory hMemory);
	// Call to Vulkan function vkBindBufferMemory or vkBindBufferMemory2KHR.
	VkResult BindVulkanBuffer(
		VkDeviceMemory memory,
		VkDeviceSize memoryOffset,
		VkBuffer buffer,
		const void* pNext);
	// Call to Vulkan function vkBindImageMemory or vkBindImageMemory2KHR.
	VkResult BindVulkanImage(
		VkDeviceMemory memory,
		VkDeviceSize memoryOffset,
		VkImage image,
		const void* pNext);

	VkResult Map(Allocation* hAllocation, void** ppData);
	void Unmap(Allocation* hAllocation);

	VkResult BindBufferMemory(
		Allocation* hAllocation,
		VkDeviceSize allocationLocalOffset,
		VkBuffer hBuffer,
		const void* pNext);
	VkResult BindImageMemory(
		Allocation* hAllocation,
		VkDeviceSize allocationLocalOffset,
		VkImage hImage,
		const void* pNext);

	VkResult FlushOrInvalidateAllocation(
		Allocation* hAllocation,
		VkDeviceSize offset, VkDeviceSize size,
		VMA_CACHE_OPERATION op);
	VkResult FlushOrInvalidateAllocations(
		u32 allocationCount,
		const Allocation** allocations,
		const VkDeviceSize* offsets, const VkDeviceSize* sizes,
		VMA_CACHE_OPERATION op);

	VkResult CopyMemoryToAllocation(
		const void* pSrcHostPointer,
		Allocation* dstAllocation,
		VkDeviceSize dstAllocationLocalOffset,
		VkDeviceSize size);
	VkResult CopyAllocationToMemory(
		Allocation* srcAllocation,
		VkDeviceSize srcAllocationLocalOffset,
		void* pDstHostPointer,
		VkDeviceSize size);

	/*
	Returns bit mask of memory types that can support defragmentation on GPU as
	they support creation of required buffer for copy operations.
	*/
	u32 GetGpuDefragmentationMemoryTypeBits();

private:

	VkResult AllocateMemoryOfType(
		VkDeviceSize size,
		VkDeviceSize alignment,
		bool dedicatedPreferred,
		VkBuffer dedicatedBuffer,
		VkImage dedicatedImage,
		VmaBufferImageUsage dedicatedBufferImageUsage,
		const VmaAllocationCreateInfo& createInfo,
		u32 memTypeIndex,
		SuballocationType suballocType,
		VmaDedicatedAllocationList& dedicatedAllocations,
		MemBlockVector& blockVector,
		u64 allocationCount,
		Allocation** pAllocations);

	// Helper function only to be used inside AllocateDedicatedMemory.
	VkResult AllocateDedicatedMemoryPage(
		VkDeviceSize size,
		SuballocationType suballocType,
		u32 memTypeIndex,
		const VkMemoryAllocateInfo& allocInfo,
		bool map,
		bool isMappingAllowed,
		Allocation** pAllocation);

	// Allocates and registers new VkDeviceMemory specifically for dedicated allocations.
	VkResult AllocateDedicatedMemory(
		VkDeviceSize size,
		SuballocationType suballocType,
		VmaDedicatedAllocationList& dedicatedAllocations,
		u32 memTypeIndex,
		bool map,
		bool isMappingAllowed,
		bool canAliasMemory,
		float priority,
		VkBuffer dedicatedBuffer,
		VkImage dedicatedImage,
		VmaBufferImageUsage dedicatedBufferImageUsage,
		u64 allocationCount,
		Allocation** pAllocations,
		const void* pNextChain = 0);

	void FreeDedicatedMemory(const Allocation* allocation);

	VkResult CalcMemTypeParams(
		VmaAllocationCreateInfo& outCreateInfo,
		u32 memTypeIndex,
		VkDeviceSize size,
		u64 allocationCount);
	VkResult CalcAllocationParams(
		VmaAllocationCreateInfo& outCreateInfo,
		bool dedicatedRequired,
		bool dedicatedPreferred);

	bool GetFlushOrInvalidateRange(
		Allocation* allocation,
		VkDeviceSize offset, VkDeviceSize size,
		VkMappedMemoryRange& outRange) const;

	void UpdateVulkanBudget();
};



VkResult VmaAllocatorT::AllocateMemoryOfType(
	VkDeviceSize size,
	VkDeviceSize alignment,
	bool dedicatedPreferred,
	VkBuffer dedicatedBuffer,
	VkImage dedicatedImage,
	VmaBufferImageUsage dedicatedBufferImageUsage,
	const VmaAllocationCreateInfo& createInfo,
	u32 memTypeIndex,
	SuballocationType suballocType,
	VmaDedicatedAllocationList& dedicatedAllocations,
	MemBlockVector& blockVector,
	u64 allocationCount,
	Allocation** pAllocations
) {
	Assert(pAllocations != 0);
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
			(finalCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0,
			(finalCreateInfo.flags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0,
			(finalCreateInfo.flags & VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT) != 0,
			finalCreateInfo.priority,
			dedicatedBuffer,
			dedicatedImage,
			dedicatedBufferImageUsage,
			allocationCount,
			pAllocations,
			0
		);
	}
	else
	{
		const bool canAllocateDedicated = (finalCreateInfo.flags & VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT) == 0;

		if(canAllocateDedicated)
		{
			// Heuristics: Allocate dedicated memory if requested size if greater than half of preferred block size.
			if(size > blockVector.preferredBlockSize / 2)
			{
				dedicatedPreferred = true;
			}
			// Protection against creating each allocation as dedicated when we reach or exceed heap size/budget,
			// which can quickly deplete maxMemoryAllocationCount: Don't prefer dedicated allocations when above
			// 3/4 of the maximum allocation count.
			if(vkPhysicalDeviceProperties.limits.maxMemoryAllocationCount < UINT32_MAX / 4 &&
				deviceMemoryCount.load() > vkPhysicalDeviceProperties.limits.maxMemoryAllocationCount * 3 / 4)
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
					(finalCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0,
					(finalCreateInfo.flags &
						(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0,
					(finalCreateInfo.flags & VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT) != 0,
					finalCreateInfo.priority,
					dedicatedBuffer,
					dedicatedImage,
					dedicatedBufferImageUsage,
					allocationCount,
					pAllocations,
					0
				);
				if(res == VK_SUCCESS)
				{
					// Succeeded: AllocateDedicatedMemory function already filled pMemory, nothing more to do here.
					return VK_SUCCESS;
				}
			}
		}

		res = blockVector.Allocate(
			size,
			alignment,
			finalCreateInfo,
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
				(finalCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0,
				(finalCreateInfo.flags &
					(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0,
				(finalCreateInfo.flags & VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT) != 0,
				finalCreateInfo.priority,
				dedicatedBuffer,
				dedicatedImage,
				dedicatedBufferImageUsage,
				allocationCount,
				pAllocations,
				0
			);
			if(res == VK_SUCCESS)
			{
				// Succeeded: AllocateDedicatedMemory function already filled pMemory, nothing more to do here.
				return VK_SUCCESS;
			}
		}
		// Everything failed: Return error code.
		return res;
	}
}

VkResult VmaAllocatorT::AllocateDedicatedMemory(
	VkDeviceSize size,
	SuballocationType suballocType,
	VmaDedicatedAllocationList& dedicatedAllocations,
	u32 memTypeIndex,
	bool map,
	bool isMappingAllowed,
	bool canAliasMemory,
	float priority,
	VkBuffer dedicatedBuffer,
	VkImage dedicatedImage,
	VmaBufferImageUsage dedicatedBufferImageUsage,
	u64 allocationCount,
	Allocation** pAllocations,
	const void* pNextChain)
{
	Assert(allocationCount > 0 && pAllocations);

	VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocInfo.memoryType = memTypeIndex;
	allocInfo.allocationSize = size;
	allocInfo.pNext = pNextChain;

	VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR };
	if(!canAliasMemory)
	{
		if(dedicatedBuffer != VK_NULL_HANDLE)
		{
			Assert(dedicatedImage == VK_NULL_HANDLE);
			dedicatedAllocInfo.buffer = dedicatedBuffer;
			VmaPnextChainPushFront(&allocInfo, &dedicatedAllocInfo);
		}
		else if(dedicatedImage != VK_NULL_HANDLE)
		{
			dedicatedAllocInfo.image = dedicatedImage;
			VmaPnextChainPushFront(&allocInfo, &dedicatedAllocInfo);
		}
	}

	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR };
	VkMemoryPriorityAllocateInfoEXT priorityInfo = { VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT };

	u64 allocIndex;
	VkResult res = VK_SUCCESS;
	for(allocIndex = 0; allocIndex < allocationCount; ++allocIndex)
	{
		res = AllocateDedicatedMemoryPage(
			size,
			suballocType,
			memTypeIndex,
			allocInfo,
			map,
			isMappingAllowed,
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
			Allocation* currAlloc = pAllocations[allocIndex];
			VkDeviceMemory hMemory = currAlloc->GetMemory();

			/*
			There is no need to call this, because Vulkan spec allows to skip vkUnmapMemory
			before vkFreeMemory.

			if(currAlloc->GetMappedData() != 0)
			{
				vkUnmapMemory(vkDevice, hMemory);
			}
			*/

			FreeVulkanMemory(memTypeIndex, currAlloc->GetSize(), hMemory);
			budget.RemoveAllocation(MemoryTypeToHeapIndex(memTypeIndex), currAlloc->GetSize());
			delete currAlloc;
		}

		memset(pAllocations, 0, sizeof(Allocation*) * allocationCount);
	}

	return res;
}

VkResult VmaAllocatorT::AllocateDedicatedMemoryPage(
	VkDeviceSize size,
	SuballocationType suballocType,
	u32 memTypeIndex,
	const VkMemoryAllocateInfo& allocInfo,
	bool map,
	bool isMappingAllowed,
	Allocation** pAllocation)
{
	VkDeviceMemory hMemory = VK_NULL_HANDLE;
	VkResult res = AllocateVulkanMemory(&allocInfo, &hMemory);
	if(res < 0)
	{
		return res;
	}

	void* mappedData = 0;
	if(map)
	{
		res = vkMapMemory(vkDevice, hMemory, 0, VK_WHOLE_SIZE, 0, &mappedData);
		if(res < 0)
		{
			FreeVulkanMemory(memTypeIndex, size, hMemory);
			return res;
		}
	}

	*pAllocation = new Allocation(isMappingAllowed);
	(*pAllocation)->InitDedicatedAllocation(this, memTypeIndex, hMemory, suballocType, mappedData, size);
	budget.AddAllocation(MemoryTypeToHeapIndex(memTypeIndex), size);

	return VK_SUCCESS;
}

void VmaAllocatorT::GetImageMemoryRequirements(
	VkImage hImage,
	VkMemoryRequirements& vkMemoryRequirements,
	bool& requiresDedicatedAllocation,
	bool& prefersDedicatedAllocation) const
{
#if 1 || 1003000 >= 1001000
		VkImageMemoryRequirementsInfo2KHR vkMemoryRequirementsInfo = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR };
		vkMemoryRequirementsInfo.image = hImage;

		VkMemoryDedicatedRequirementsKHR memDedicatedReq = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };

		VkMemoryRequirements2KHR vkMemoryRequirements2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
		VmaPnextChainPushFront(&vkMemoryRequirements2, &memDedicatedReq);

		vkGetImageMemoryRequirements2KHR(vkDevice, &vkMemoryRequirementsInfo, &vkMemoryRequirements2);

		vkMemoryRequirements = vkMemoryRequirements2.memoryRequirements;
		requiresDedicatedAllocation = (memDedicatedReq.requiresDedicatedAllocation != VK_FALSE);
		prefersDedicatedAllocation  = (memDedicatedReq.prefersDedicatedAllocation  != VK_FALSE);
#endif // #if 1 || 1003000 >= 1001000
}

VkResult VmaAllocatorT::FindMemoryTypeIndex(
	u32 memoryTypeBits,
	const VmaAllocationCreateInfo* pAllocationCreateInfo,
	VmaBufferImageUsage bufImgUsage,
	u32* pMemoryTypeIndex) const
{
	memoryTypeBits &= allowedMemoryTypes;

	if(pAllocationCreateInfo->memoryTypeBits != 0)
	{
		memoryTypeBits &= pAllocationCreateInfo->memoryTypeBits;
	}

	VkMemoryPropertyFlags requiredFlags = 0, preferredFlags = 0, notPreferredFlags = 0;
	if(!FindMemoryPreferences(
		vkPhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
		*pAllocationCreateInfo,
		bufImgUsage,
		requiredFlags, preferredFlags, notPreferredFlags))
	{
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	*pMemoryTypeIndex = UINT32_MAX;
	u32 minCost = UINT32_MAX;
	for(u32 memTypeIndex = 0, memTypeBit = 1;
		memTypeIndex < vkPhysicalDeviceMemoryProperties.memoryTypeCount;
		++memTypeIndex, memTypeBit <<= 1)
	{
		// This memory type is acceptable according to memoryTypeBits bitmask.
		if((memTypeBit & memoryTypeBits) != 0)
		{
			const VkMemoryPropertyFlags currFlags =
				vkPhysicalDeviceMemoryProperties.memoryTypes[memTypeIndex].propertyFlags;
			// This memory type contains requiredFlags.
			if((requiredFlags & ~currFlags) == 0)
			{
				// Calculate cost as number of bits from preferredFlags not present in this memory type.
				u32 currCost = VmaCountBitsSet(preferredFlags & ~currFlags) +
					VmaCountBitsSet(currFlags & notPreferredFlags);
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

VkResult VmaAllocatorT::CalcMemTypeParams(
	VmaAllocationCreateInfo& inoutCreateInfo,
	u32 memTypeIndex,
	VkDeviceSize size,
	u64 allocationCount)
{
	// If memory type is not HOST_VISIBLE, disable MAPPED.
	if((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0 &&
		(vkPhysicalDeviceMemoryProperties.memoryTypes[memTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
	{
		inoutCreateInfo.flags &= ~VMA_ALLOCATION_CREATE_MAPPED_BIT;
	}

	if((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) != 0 &&
		(inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT) != 0)
	{
		const u32 heapIndex = MemoryTypeToHeapIndex(memTypeIndex);
		VmaBudget heapBudget = {};
		GetHeapBudgets(&heapBudget, heapIndex, 1);
		if(heapBudget.usage + size * allocationCount > heapBudget.budget)
		{
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}
	}
	return VK_SUCCESS;
}

VkResult VmaAllocatorT::CalcAllocationParams(
	VmaAllocationCreateInfo& inoutCreateInfo,
	bool dedicatedRequired,
	bool dedicatedPreferred)
{
	Assert((inoutCreateInfo.flags &
		(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) !=
		(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT) &&
		"Specifying both flags VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT and VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT is incorrect.");
	Assert((((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT) == 0 ||
		(inoutCreateInfo.flags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0)) &&
		"Specifying VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT requires also VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.");
	if(inoutCreateInfo.usage == VMA_MEMORY_USAGE_AUTO || inoutCreateInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE || inoutCreateInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST)
	{
		if((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0)
		{
			Assert((inoutCreateInfo.flags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0 &&
				"When using VMA_ALLOCATION_CREATE_MAPPED_BIT and usage = VMA_MEMORY_USAGE_AUTO*, you must also specify VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.");
		}
	}

	// If memory is lazily allocated, it should be always dedicated.
	if(dedicatedRequired ||
		inoutCreateInfo.usage == VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED)
	{
		inoutCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	}

	if((inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) != 0 &&
		(inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT) != 0)
	{
		Assert(0 && "Specifying VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT together with VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT makes no sense.");
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	if(0 &&
		(inoutCreateInfo.flags & VMA_ALLOCATION_CREATE_NEVER_ALLOCATE_BIT) != 0)
	{
		inoutCreateInfo.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	}

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

VkResult VmaAllocatorT::AllocateMemory(
	const VkMemoryRequirements& vkMemoryRequirements,
	bool requiresDedicatedAllocation,
	bool prefersDedicatedAllocation,
	VkBuffer dedicatedBuffer,
	VkImage dedicatedImage,
	VmaBufferImageUsage dedicatedBufferImageUsage,
	const VmaAllocationCreateInfo& createInfo,
	SuballocationType suballocType,
	u64 allocationCount,
	Allocation** pAllocations)
{
	memset(pAllocations, 0, sizeof(Allocation*) * allocationCount);

	Assert(VmaIsPow2(vkMemoryRequirements.alignment));

	if(vkMemoryRequirements.size == 0)
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	VmaAllocationCreateInfo createInfoFinal = createInfo;
	VkResult res = CalcAllocationParams(createInfoFinal, requiresDedicatedAllocation, prefersDedicatedAllocation);
	if(res != VK_SUCCESS)
		return res;

	// Bit mask of memory Vulkan types acceptable for this allocation.
	u32 memoryTypeBits = vkMemoryRequirements.memoryTypeBits;
	u32 memTypeIndex = UINT32_MAX;
	res = FindMemoryTypeIndex(memoryTypeBits, &createInfoFinal, dedicatedBufferImageUsage, &memTypeIndex);
	// Can't find any single memory type matching requirements. res is VK_ERROR_FEATURE_NOT_PRESENT.
	if(res != VK_SUCCESS)
		return res;
	do
	{
		MemBlockVector* blockVector = blockVectors[memTypeIndex];
		Assert(blockVector && "Trying to use unsupported memory type!");
		res = AllocateMemoryOfType(
			VK_NULL_HANDLE,
			vkMemoryRequirements.size,
			vkMemoryRequirements.alignment,
			requiresDedicatedAllocation || prefersDedicatedAllocation,
			dedicatedBuffer,
			dedicatedImage,
			dedicatedBufferImageUsage,
			createInfoFinal,
			memTypeIndex,
			suballocType,
			dedicatedAllocations[memTypeIndex],
			*blockVector,
			allocationCount,
			pAllocations);
		// Allocation succeeded
		if(res == VK_SUCCESS)
			return VK_SUCCESS;

		// Remove old memTypeIndex from list of possibilities.
		memoryTypeBits &= ~(1u << memTypeIndex);
		// Find alternative memTypeIndex.
		res = FindMemoryTypeIndex(memoryTypeBits, &createInfoFinal, dedicatedBufferImageUsage, &memTypeIndex);
	} while(res == VK_SUCCESS);

	// No other matching memory type index could be found.
	// Not returning res, which is VK_ERROR_FEATURE_NOT_PRESENT, because we already failed to allocate once.
	return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

void VmaAllocatorT::FreeMemory(u64 allocationCount, const Allocation** pAllocations) {
	Assert(pAllocations);

	for(u64 allocIndex = allocationCount; allocIndex--; ) {
		Allocation* allocation = pAllocations[allocIndex];

		if(allocation != VK_NULL_HANDLE) {
			switch(allocation->allocationType)
			{
			case Allocation::AllocationType::Block:
				{
					MemBlockVector* pBlockVector = 0;
					const u32 memTypeIndex = allocation->memoryType;
					pBlockVector = blockVectors[memTypeIndex];
					Assert(pBlockVector && "Trying to free memory of unsupported type!");
					pBlockVector->Free(allocation);
				}
				break;
			case Allocation::AllocationType::Dedicated:
				FreeDedicatedMemory(allocation);
				break;
			default:
				Assert(0);
			}
		}
	}
}

void VmaAllocatorT::GetHeapBudgets(VmaBudget* outBudgets, u32 firstHeap, u32 heapCount) {
	if(budget.m_OperationsSinceBudgetFetch < 30) {
		for(u32 i = 0; i < heapCount; ++i, ++outBudgets) {
			const u32 heapIndex = firstHeap + i;
			outBudgets->statistics.blockCount      = budget.m_BlockCount[heapIndex];
			outBudgets->statistics.allocationCount = budget.m_AllocationCount[heapIndex];
			outBudgets->statistics.blockBytes      = budget.m_BlockBytes[heapIndex];
			outBudgets->statistics.allocationBytes = budget.m_AllocationBytes[heapIndex];
			if(budget.m_VulkanUsage[heapIndex] + outBudgets->statistics.blockBytes > budget.m_BlockBytesAtBudgetFetch[heapIndex]) {
				outBudgets->usage = budget.m_VulkanUsage[heapIndex] + outBudgets->statistics.blockBytes - budget.m_BlockBytesAtBudgetFetch[heapIndex];
			} else {
				outBudgets->usage = 0;
			}

			// Have to take MIN with heap size because explicit HeapSizeLimit is included in it.
			outBudgets->budget = std::min(budget.m_VulkanBudget[heapIndex], vkPhysicalDeviceMemoryProperties.memoryHeaps[heapIndex].size);
		}
	} else {
		UpdateVulkanBudget();
		GetHeapBudgets(outBudgets, firstHeap, heapCount); // Recursion
	}
}

void VmaAllocatorT::GetAllocationInfo(Allocation* hAllocation, AllocationInfo* pAllocationInfo)
{
	pAllocationInfo->memoryType = hAllocation->memoryType;
	pAllocationInfo->vkDeviceMemory = hAllocation->GetMemory();
	pAllocationInfo->offset = hAllocation->GetOffset();
	pAllocationInfo->size = hAllocation->GetSize();
	pAllocationInfo->mappedData = hAllocation->GetMappedData();
}

VkResult VmaAllocatorT::AllocateVulkanMemory(const VkMemoryAllocateInfo* pAllocateInfo, VkDeviceMemory* pMemory)
{
	const u32 heapIndex = MemoryTypeToHeapIndex(pAllocateInfo->memoryType);

	budget.m_BlockBytes[heapIndex] += pAllocateInfo->allocationSize;
	++budget.m_BlockCount[heapIndex];

	// VULKAN CALL vkAllocateMemory.
	VkResult res = vkAllocateMemory(vkDevice, pAllocateInfo, vkAllocationCallbacks, pMemory);

	if(res == VK_SUCCESS)
	{
		++budget.m_OperationsSinceBudgetFetch;
	}
	else
	{
		--budget.m_BlockCount[heapIndex];
		budget.m_BlockBytes[heapIndex] -= pAllocateInfo->allocationSize;
	}

	return res;
}

void VmaAllocatorT::FreeVulkanMemory(u32 memoryType, VkDeviceSize size, VkDeviceMemory hMemory)
{
	vkFreeMemory(vkDevice, hMemory, vkAllocationCallbacks);

	const u32 heapIndex = MemoryTypeToHeapIndex(memoryType);
	--budget.m_BlockCount[heapIndex];
	budget.m_BlockBytes[heapIndex] -= size;

	--deviceMemoryCount;
}

VkResult VmaAllocatorT::BindVulkanBuffer(
	VkDeviceMemory memory,
	VkDeviceSize memoryOffset,
	VkBuffer buffer,
	const void* pNext)
{
	if(pNext != 0)
	{
		VkBindBufferMemoryInfoKHR bindBufferMemoryInfo = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR };
		bindBufferMemoryInfo.pNext = pNext;
		bindBufferMemoryInfo.buffer = buffer;
		bindBufferMemoryInfo.memory = memory;
		bindBufferMemoryInfo.memoryOffset = memoryOffset;
		return vkBindBufferMemory2KHR(vkDevice, 1, &bindBufferMemoryInfo);
	}
	else
	{
		return vkBindBufferMemory(vkDevice, buffer, memory, memoryOffset);
	}
}

VkResult VmaAllocatorT::BindVulkanImage(
	VkDeviceMemory memory,
	VkDeviceSize memoryOffset,
	VkImage image,
	const void* pNext)
{
	if(pNext != 0)
	{
		VkBindImageMemoryInfoKHR bindBufferMemoryInfo = { VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO_KHR };
		bindBufferMemoryInfo.pNext = pNext;
		bindBufferMemoryInfo.image = image;
		bindBufferMemoryInfo.memory = memory;
		bindBufferMemoryInfo.memoryOffset = memoryOffset;
		return vkBindImageMemory2KHR(vkDevice, 1, &bindBufferMemoryInfo);
	}
	else
	{
		return vkBindImageMemory(vkDevice, image, memory, memoryOffset);
	}
}

VkResult VmaAllocatorT::Map(Allocation* hAllocation, void** ppData)
{
	switch(hAllocation->allocationType)
	{
	case Allocation::AllocationType::Block:
		{
			MemBlock* const pBlock = hAllocation->blockAllocation.block;
			char *pBytes = 0;
			VkResult res = pBlock->Map(this, 1, (void**)&pBytes);
			if(res == VK_SUCCESS)
			{
				*ppData = pBytes + (ptrdiff_t)hAllocation->GetOffset();
				hAllocation->BlockAllocMap();
			}
			return res;
		}
	case Allocation::AllocationType::Dedicated:
		return hAllocation->DedicatedAllocMap(this, ppData);
	default:
		Assert(0);
		return VK_ERROR_MEMORY_MAP_FAILED;
	}
}

void VmaAllocatorT::Unmap(Allocation* hAllocation)
{
	switch(hAllocation->allocationType)
	{
	case Allocation::AllocationType::Block:
		{
			MemBlock* const pBlock = hAllocation->blockAllocation.block;
			hAllocation->BlockAllocUnmap();
			pBlock->Unmap(this, 1);
		}
		break;
	case Allocation::AllocationType::Dedicated:
		hAllocation->DedicatedAllocUnmap(this);
		break;
	default:
		Assert(0);
	}
}

VkResult VmaAllocatorT::BindBufferMemory(
	Allocation* hAllocation,
	VkDeviceSize allocationLocalOffset,
	VkBuffer hBuffer,
	const void* pNext)
{
	VkResult res = VK_ERROR_UNKNOWN_COPY;
	switch(hAllocation->allocationType)
	{
	case Allocation::AllocationType::Dedicated:
		res = BindVulkanBuffer(hAllocation->GetMemory(), allocationLocalOffset, hBuffer, pNext);
		break;
	case Allocation::AllocationType::Block:
	{
		MemBlock* const pBlock = hAllocation->blockAllocation.block;
		Assert(pBlock && "Binding buffer to allocation that doesn't belong to any block.");
		res = pBlock->BindBufferMemory(this, hAllocation, allocationLocalOffset, hBuffer, pNext);
		break;
	}
	default:
		Assert(0);
	}
	return res;
}

VkResult VmaAllocatorT::BindImageMemory(
	Allocation* hAllocation,
	VkDeviceSize allocationLocalOffset,
	VkImage hImage,
	const void* pNext)
{
	VkResult res = VK_ERROR_UNKNOWN_COPY;
	switch(hAllocation->allocationType)
	{
	case Allocation::AllocationType::Dedicated:
		res = BindVulkanImage(hAllocation->GetMemory(), allocationLocalOffset, hImage, pNext);
		break;
	case Allocation::AllocationType::Block:
	{
		MemBlock* pBlock = hAllocation->blockAllocation.block;
		Assert(pBlock && "Binding image to allocation that doesn't belong to any block.");
		res = pBlock->BindImageMemory(this, hAllocation, allocationLocalOffset, hImage, pNext);
		break;
	}
	default:
		Assert(0);
	}
	return res;
}

VkResult VmaAllocatorT::FlushOrInvalidateAllocation(
	Allocation* hAllocation,
	VkDeviceSize offset, VkDeviceSize size,
	VMA_CACHE_OPERATION op)
{
	VkResult res = VK_SUCCESS;

	VkMappedMemoryRange memRange = {};
	if(GetFlushOrInvalidateRange(hAllocation, offset, size, memRange))
	{
		switch(op)
		{
		case VMA_CACHE_FLUSH:
			res = vkFlushMappedMemoryRanges(vkDevice, 1, &memRange);
			break;
		case VMA_CACHE_INVALIDATE:
			res = vkInvalidateMappedMemoryRanges(vkDevice, 1, &memRange);
			break;
		default:
			Assert(0);
		}
	}
	// else: Just ignore this call.
	return res;
}

VkResult VmaAllocatorT::FlushOrInvalidateAllocations(
	u32 allocationCount,
	const Allocation** allocations,
	const VkDeviceSize* offsets, const VkDeviceSize* sizes,
	VMA_CACHE_OPERATION op)
{
	typedef VmaStlAllocator<VkMappedMemoryRange> RangeAllocator;
	typedef VmaSmallVector<VkMappedMemoryRange, RangeAllocator, 16> RangeVector;
	RangeVector ranges = RangeVector(RangeAllocator());

	for(u32 allocIndex = 0; allocIndex < allocationCount; ++allocIndex)
	{
		const Allocation* alloc = allocations[allocIndex];
		const VkDeviceSize offset = offsets != 0 ? offsets[allocIndex] : 0;
		const VkDeviceSize size = sizes != 0 ? sizes[allocIndex] : VK_WHOLE_SIZE;
		VkMappedMemoryRange newRange;
		if(GetFlushOrInvalidateRange(alloc, offset, size, newRange))
		{
			ranges.push_back(newRange);
		}
	}

	VkResult res = VK_SUCCESS;
	if(!ranges.empty())
	{
		switch(op)
		{
		case VMA_CACHE_FLUSH:
			res = vkFlushMappedMemoryRanges(vkDevice, (u32)ranges.size(), ranges.data());
			break;
		case VMA_CACHE_INVALIDATE:
			res = vkInvalidateMappedMemoryRanges(vkDevice, (u32)ranges.size(), ranges.data());
			break;
		default:
			Assert(0);
		}
	}
	// else: Just ignore this call.
	return res;
}

VkResult VmaAllocatorT::CopyMemoryToAllocation(
	const void* pSrcHostPointer,
	Allocation* dstAllocation,
	VkDeviceSize dstAllocationLocalOffset,
	VkDeviceSize size)
{
	void* dstMappedData = 0;
	VkResult res = Map(dstAllocation, &dstMappedData);
	if(res == VK_SUCCESS)
	{
		memcpy((char*)dstMappedData + dstAllocationLocalOffset, pSrcHostPointer, (u64)size);
		Unmap(dstAllocation);
		res = FlushOrInvalidateAllocation(dstAllocation, dstAllocationLocalOffset, size, VMA_CACHE_FLUSH);
	}
	return res;
}

VkResult VmaAllocatorT::CopyAllocationToMemory(
	Allocation* srcAllocation,
	VkDeviceSize srcAllocationLocalOffset,
	void* pDstHostPointer,
	VkDeviceSize size)
{
	void* srcMappedData = 0;
	VkResult res = Map(srcAllocation, &srcMappedData);
	if(res == VK_SUCCESS)
	{
		res = FlushOrInvalidateAllocation(srcAllocation, srcAllocationLocalOffset, size, VMA_CACHE_INVALIDATE);
		if(res == VK_SUCCESS)
		{
			memcpy(pDstHostPointer, (const char*)srcMappedData + srcAllocationLocalOffset, (u64)size);
			Unmap(srcAllocation);
		}
	}
	return res;
}

void VmaAllocatorT::FreeDedicatedMemory(const Allocation* allocation)
{
	Assert(allocation && allocation->allocationType == Allocation::AllocationType::Dedicated);

	const u32 memTypeIndex = allocation->memoryType;
	dedicatedAllocations[memTypeIndex].Unregister(allocation);
	VkDeviceMemory hMemory = allocation->GetMemory();

	/*
	There is no need to call this, because Vulkan spec allows to skip vkUnmapMemory
	before vkFreeMemory.

	if(allocation->GetMappedData() != 0)
	{
		vkUnmapMemory(vkDevice, hMemory);
	}
	*/

	FreeVulkanMemory(memTypeIndex, allocation->GetSize(), hMemory);

	budget.RemoveAllocation(MemoryTypeToHeapIndex(allocation->memoryType), allocation->GetSize());
	allocation->Destroy(this);
	delete allocation;
}

bool VmaAllocatorT::GetFlushOrInvalidateRange(
	Allocation* allocation,
	VkDeviceSize offset, VkDeviceSize size,
	VkMappedMemoryRange& outRange) const
{
	const u32 memTypeIndex = allocation->memoryType;
	if(size > 0 && IsMemoryTypeNonCoherent(memTypeIndex))
	{
		const VkDeviceSize nonCoherentAtomSize = vkPhysicalDeviceProperties.limits.nonCoherentAtomSize;
		const VkDeviceSize allocationSize = allocation->GetSize();
		Assert(offset <= allocationSize);

		outRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		outRange.pNext = 0;
		outRange.memory = allocation->GetMemory();

		switch(allocation->allocationType)
		{
		case Allocation::AllocationType::Dedicated:
			outRange.offset = VmaAlignDown(offset, nonCoherentAtomSize);
			if(size == VK_WHOLE_SIZE)
			{
				outRange.size = allocationSize - outRange.offset;
			}
			else
			{
				Assert(offset + size <= allocationSize);
				outRange.size = std::min(
					VmaAlignUp(size + (offset - outRange.offset), nonCoherentAtomSize),
					allocationSize - outRange.offset);
			}
			break;
		case Allocation::AllocationType::Block:
		{
			// 1. Still within this allocation.
			outRange.offset = VmaAlignDown(offset, nonCoherentAtomSize);
			if(size == VK_WHOLE_SIZE)
			{
				size = allocationSize - offset;
			}
			else
			{
				Assert(offset + size <= allocationSize);
			}
			outRange.size = VmaAlignUp(size + (offset - outRange.offset), nonCoherentAtomSize);

			// 2. Adjust to whole block.
			const VkDeviceSize allocationOffset = allocation->GetOffset();
			Assert(allocationOffset % nonCoherentAtomSize == 0);
			const VkDeviceSize blockSize = allocation->blockAllocation.block->meta->size;
			outRange.offset += allocationOffset;
			outRange.size = std::min(outRange.size, blockSize - outRange.offset);

			break;
		}
		default:
			Assert(0);
		}
		return true;
	}
	return false;
}


VkResult vmaFindMemoryTypeIndex(
	VmaAllocator allocator,
	u32 memoryTypeBits,
	const VmaAllocationCreateInfo* pAllocationCreateInfo,
	u32* pMemoryTypeIndex)
{
	Assert(allocator != VK_NULL_HANDLE);
	Assert(pAllocationCreateInfo != 0);
	Assert(pMemoryTypeIndex != 0);

	return allocator->FindMemoryTypeIndex(memoryTypeBits, pAllocationCreateInfo, VmaBufferImageUsage::UNKNOWN, pMemoryTypeIndex);
}

VkResult vmaFindMemoryTypeIndexForBufferInfo(
	VmaAllocator allocator,
	const VkBufferCreateInfo* pBufferCreateInfo,
	const VmaAllocationCreateInfo* pAllocationCreateInfo,
	u32* pMemoryTypeIndex)
{
	Assert(allocator != VK_NULL_HANDLE);
	Assert(pBufferCreateInfo != 0);
	Assert(pAllocationCreateInfo != 0);
	Assert(pMemoryTypeIndex != 0);

	const VkDevice hDev = allocator->vkDevice;
	VkResult res;

	// Can query straight from VkBufferCreateInfo :)
	VkDeviceBufferMemoryRequirementsKHR devBufvkMemoryRequirements = {VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS_KHR};
	devBufvkMemoryRequirements.createInfo = pBufferCreateInfo;

	VkMemoryRequirements2 vkMemoryRequirements = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
	vkGetDeviceBufferMemoryRequirements(hDev, &devBufvkMemoryRequirements, &vkMemoryRequirements);

	res = allocator->FindMemoryTypeIndex(vkMemoryRequirements.memoryRequirements.memoryTypeBits, pAllocationCreateInfo, VmaBufferImageUsage(*pBufferCreateInfo), pMemoryTypeIndex);
return res;
}

VkResult vmaFindMemoryTypeIndexForImageInfo(
	VmaAllocator allocator,
	const VkImageCreateInfo* pImageCreateInfo,
	const VmaAllocationCreateInfo* pAllocationCreateInfo,
	u32* pMemoryTypeIndex)
{
	Assert(allocator != VK_NULL_HANDLE);
	Assert(pImageCreateInfo != 0);
	Assert(pAllocationCreateInfo != 0);
	Assert(pMemoryTypeIndex != 0);

	const VkDevice hDev = allocator->vkDevice;
	VkResult res;

	VkDeviceImageMemoryRequirementsKHR devImgvkMemoryRequirements = {VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS_KHR};
	devImgvkMemoryRequirements.createInfo = pImageCreateInfo;
	Assert(pImageCreateInfo->tiling != VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT && (pImageCreateInfo->flags & VK_IMAGE_CREATE_DISJOINT_BIT) == 0 &&
		"Cannot use this VkImageCreateInfo with vmaFindMemoryTypeIndexForImageInfo as I don't know what to pass as VkDeviceImageMemoryRequirements::planeAspect.");

	VkMemoryRequirements2 vkMemoryRequirements = {VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};
	vkGetDeviceImageMemoryRequirements(hDev, &devImgvkMemoryRequirements, &vkMemoryRequirements);

	res = allocator->FindMemoryTypeIndex(
		vkMemoryRequirements.memoryRequirements.memoryTypeBits, pAllocationCreateInfo,
		VmaBufferImageUsage(*pImageCreateInfo), pMemoryTypeIndex);
}

VkResult vmaAllocateMemory(
	VmaAllocator allocator,
	const VkMemoryRequirements* pVkMemoryRequirements,
	const VmaAllocationCreateInfo* createInfo,
	Allocation** pAllocation,
	AllocationInfo* pAllocationInfo)
{
	Assert(allocator && pVkMemoryRequirements && createInfo && pAllocation);

	VkResult result = allocator->AllocateMemory(
		*pVkMemoryRequirements,
		false, // requiresDedicatedAllocation
		false, // prefersDedicatedAllocation
		VK_NULL_HANDLE, // dedicatedBuffer
		VK_NULL_HANDLE, // dedicatedImage
		VmaBufferImageUsage::UNKNOWN, // dedicatedBufferImageUsage
		*createInfo,
		SuballocationType::Unknown,
		1, // allocationCount
		pAllocation);

	if(pAllocationInfo != 0 && result == VK_SUCCESS)
	{
		allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
	}

	return result;
}

VkResult vmaAllocateMemoryPages(
	VmaAllocator allocator,
	const VkMemoryRequirements* pVkMemoryRequirements,
	const VmaAllocationCreateInfo* createInfo,
	u64 allocationCount,
	Allocation** pAllocations,
	AllocationInfo* pAllocationInfo)
{
	if(allocationCount == 0)
	{
		return VK_SUCCESS;
	}

	Assert(allocator && pVkMemoryRequirements && createInfo && pAllocations);

	VkResult result = allocator->AllocateMemory(
		*pVkMemoryRequirements,
		false, // requiresDedicatedAllocation
		false, // prefersDedicatedAllocation
		VK_NULL_HANDLE, // dedicatedBuffer
		VK_NULL_HANDLE, // dedicatedImage
		VmaBufferImageUsage::UNKNOWN, // dedicatedBufferImageUsage
		*createInfo,
		SuballocationType::Unknown,
		allocationCount,
		pAllocations);

	if(pAllocationInfo != 0 && result == VK_SUCCESS)
	{
		for(u64 i = 0; i < allocationCount; ++i)
		{
			allocator->GetAllocationInfo(pAllocations[i], pAllocationInfo + i);
		}
	}

	return result;
}

VkResult vmaAllocateMemoryForBuffer(
	VmaAllocator allocator,
	VkBuffer buffer,
	const VmaAllocationCreateInfo* createInfo,
	Allocation** pAllocation,
	AllocationInfo* pAllocationInfo)
{
	Assert(allocator && buffer != VK_NULL_HANDLE && createInfo && pAllocation);

	VkMemoryRequirements vkMemoryRequirements = {};
	bool requiresDedicatedAllocation = false;
	bool prefersDedicatedAllocation = false;
	allocator->GetBufferMemoryRequirements(buffer, vkMemoryRequirements,
		requiresDedicatedAllocation,
		prefersDedicatedAllocation);

	VkResult result = allocator->AllocateMemory(
		vkMemoryRequirements,
		requiresDedicatedAllocation,
		prefersDedicatedAllocation,
		buffer, // dedicatedBuffer
		VK_NULL_HANDLE, // dedicatedImage
		VmaBufferImageUsage::UNKNOWN, // dedicatedBufferImageUsage
		*createInfo,
		SuballocationType::Buffer,
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
	const VmaAllocationCreateInfo* createInfo,
	Allocation** pAllocation,
	AllocationInfo* pAllocationInfo)
{
	Assert(allocator && image != VK_NULL_HANDLE && createInfo && pAllocation);

	VkMemoryRequirements vkMemoryRequirements = {};
	bool requiresDedicatedAllocation = false;
	bool prefersDedicatedAllocation  = false;
	allocator->GetImageMemoryRequirements(image, vkMemoryRequirements,
		requiresDedicatedAllocation, prefersDedicatedAllocation);

	VkResult result = allocator->AllocateMemory(
		vkMemoryRequirements,
		requiresDedicatedAllocation,
		prefersDedicatedAllocation,
		VK_NULL_HANDLE, // dedicatedBuffer
		image, // dedicatedImage
		VmaBufferImageUsage::UNKNOWN, // dedicatedBufferImageUsage
		*createInfo,
		SuballocationType::ImageUnknown,
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
	Allocation* allocation)
{
	Assert(allocator);

	if(allocation == VK_NULL_HANDLE)
	{
		return;
	}

	allocator->FreeMemory(
		1, // allocationCount
		&allocation);
}

void vmaFreeMemoryPages(
	VmaAllocator allocator,
	u64 allocationCount,
	const Allocation** pAllocations)
{
	if(allocationCount == 0)
	{
		return;
	}

	Assert(allocator);

	allocator->FreeMemory(allocationCount, pAllocations);
}

void vmaGetAllocationInfo(
	VmaAllocator allocator,
	Allocation* allocation,
	AllocationInfo* pAllocationInfo)
{
	Assert(allocator && allocation && pAllocationInfo);

	allocator->GetAllocationInfo(allocation, pAllocationInfo);
}

void vmaGetAllocationMemoryProperties(
	VmaAllocator allocator,
	Allocation* allocation,
	VkMemoryPropertyFlags* pFlags)
{
	Assert(allocator && allocation && pFlags);
	const u32 memTypeIndex = allocation->memoryType;
	*pFlags = allocator->vkPhysicalDeviceMemoryProperties.memoryTypes[memTypeIndex].propertyFlags;
}

VkResult vmaMapMemory(
	VmaAllocator allocator,
	Allocation* allocation,
	void** ppData)
{
	Assert(allocator && allocation && ppData);

	return allocator->Map(allocation, ppData);
}

void vmaUnmapMemory(
	VmaAllocator allocator,
	Allocation* allocation)
{
	Assert(allocator && allocation);

	allocator->Unmap(allocation);
}

VkResult vmaFlushAllocation(
	VmaAllocator allocator,
	Allocation* allocation,
	VkDeviceSize offset,
	VkDeviceSize size)
{
	Assert(allocator && allocation);

	return allocator->FlushOrInvalidateAllocation(allocation, offset, size, VMA_CACHE_FLUSH);
}

VkResult vmaInvalidateAllocation(
	VmaAllocator allocator,
	Allocation* allocation,
	VkDeviceSize offset,
	VkDeviceSize size)
{
	Assert(allocator && allocation);

	return allocator->FlushOrInvalidateAllocation(allocation, offset, size, VMA_CACHE_INVALIDATE);
}

VkResult vmaFlushAllocations(
	VmaAllocator allocator,
	u32 allocationCount,
	const Allocation** allocations,
	const VkDeviceSize* offsets,
	const VkDeviceSize* sizes)
{
	Assert(allocator);

	if(allocationCount == 0)
	{
		return VK_SUCCESS;
	}

	Assert(allocations);

	return allocator->FlushOrInvalidateAllocations(allocationCount, allocations, offsets, sizes, VMA_CACHE_FLUSH);
}

VkResult vmaInvalidateAllocations(
	VmaAllocator allocator,
	u32 allocationCount,
	const Allocation** allocations,
	const VkDeviceSize* offsets,
	const VkDeviceSize* sizes)
{
	Assert(allocator);

	if(allocationCount == 0)
	{
		return VK_SUCCESS;
	}

	Assert(allocations);

	return allocator->FlushOrInvalidateAllocations(allocationCount, allocations, offsets, sizes, VMA_CACHE_INVALIDATE);
}

VkResult vmaCopyMemoryToAllocation(
	VmaAllocator allocator,
	const void* pSrcHostPointer,
	Allocation* dstAllocation,
	VkDeviceSize dstAllocationLocalOffset,
	VkDeviceSize size)
{
	Assert(allocator && pSrcHostPointer && dstAllocation);

	if(size == 0)
	{
		return VK_SUCCESS;
	}

	return allocator->CopyMemoryToAllocation(pSrcHostPointer, dstAllocation, dstAllocationLocalOffset, size);
}

VkResult vmaCopyAllocationToMemory(
	VmaAllocator allocator,
	Allocation* srcAllocation,
	VkDeviceSize srcAllocationLocalOffset,
	void* pDstHostPointer,
	VkDeviceSize size)
{
	Assert(allocator && srcAllocation && pDstHostPointer);

	if(size == 0)
	{
		return VK_SUCCESS;
	}

	return allocator->CopyAllocationToMemory(srcAllocation, srcAllocationLocalOffset, pDstHostPointer, size);
}

VkResult vmaBindBufferMemory(
	VmaAllocator allocator,
	Allocation* allocation,
	VkBuffer buffer)
{
	Assert(allocator && allocation && buffer);

	return allocator->BindBufferMemory(allocation, 0, buffer, 0);
}

VkResult vmaBindBufferMemory2(
	VmaAllocator allocator,
	Allocation* allocation,
	VkDeviceSize allocationLocalOffset,
	VkBuffer buffer,
	const void* pNext)
{
	Assert(allocator && allocation && buffer);

	return allocator->BindBufferMemory(allocation, allocationLocalOffset, buffer, pNext);
}

VkResult vmaBindImageMemory(
	VmaAllocator allocator,
	Allocation* allocation,
	VkImage image)
{
	Assert(allocator && allocation && image);

	return allocator->BindImageMemory(allocation, 0, image, 0);
}

VkResult vmaBindImageMemory2(
	VmaAllocator allocator,
	Allocation* allocation,
	VkDeviceSize allocationLocalOffset,
	VkImage image,
	const void* pNext)
{
	Assert(allocator && allocation && image);

	return allocator->BindImageMemory(allocation, allocationLocalOffset, image, pNext);
}

VkResult vmaCreateBuffer(
	VmaAllocator allocator,
	const VkBufferCreateInfo* pBufferCreateInfo,
	const VmaAllocationCreateInfo* pAllocationCreateInfo,
	VkBuffer* pBuffer,
	Allocation** pAllocation,
	AllocationInfo* pAllocationInfo)
{
	// 3. Allocate memory using allocator.
	res = allocator->AllocateMemory(
		vkMemoryRequirements,
		requiresDedicatedAllocation,
		prefersDedicatedAllocation,
		*pBuffer, // dedicatedBuffer
		VK_NULL_HANDLE, // dedicatedImage
		VmaBufferImageUsage(*pBufferCreateInfo), // dedicatedBufferImageUsage
		*pAllocationCreateInfo,
		SuballocationType::Buffer,
		1, // allocationCount
		pAllocation);

	if(res >= 0)
	{
		// 3. Bind buffer with memory.
		if((pAllocationCreateInfo->flags & VMA_ALLOCATION_CREATE_DONT_BIND_BIT) == 0)
		{
			res = allocator->BindBufferMemory(*pAllocation, 0, *pBuffer, 0);
		}
		if(res >= 0)
		{
			// All steps succeeded.
			#if 1
				(*pAllocation)->InitBufferUsage(*pBufferCreateInfo);
			#endif
			if(pAllocationInfo != 0)
			{
				allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
			}

			return VK_SUCCESS;
		}
		allocator->FreeMemory(
			1, // allocationCount
			pAllocation);
		*pAllocation = VK_NULL_HANDLE;
		vkDestroyBuffer(allocator->vkDevice, *pBuffer, vkAllocationCallbacks);
		*pBuffer = VK_NULL_HANDLE;
		return res;
	}
	vkDestroyBuffer(allocator->vkDevice, *pBuffer, vkAllocationCallbacks);
	*pBuffer = VK_NULL_HANDLE;
	return res;
}

VkResult vmaCreateBufferWithAlignment(
	VmaAllocator allocator,
	const VkBufferCreateInfo* pBufferCreateInfo,
	const VmaAllocationCreateInfo* pAllocationCreateInfo,
	VkDeviceSize minAlignment,
	VkBuffer* pBuffer,
	Allocation** pAllocation,
	AllocationInfo* pAllocationInfo)
{
	Assert(allocator && pBufferCreateInfo && pAllocationCreateInfo && VmaIsPow2(minAlignment) && pBuffer && pAllocation);

	if(pBufferCreateInfo->size == 0)
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	*pBuffer = VK_NULL_HANDLE;
	*pAllocation = VK_NULL_HANDLE;

	// 1. Create VkBuffer.
	VkResult res = vkCreateBuffer(allocator->vkDevice, pBufferCreateInfo, vkAllocationCallbacks, pBuffer);
	if(res >= 0)
	{
		// 2. vkGetBufferMemoryRequirements.
		VkMemoryRequirements vkMemoryRequirements = {};
		bool requiresDedicatedAllocation = false;
		bool prefersDedicatedAllocation  = false;
		allocator->GetBufferMemoryRequirements(*pBuffer, vkMemoryRequirements,
			requiresDedicatedAllocation, prefersDedicatedAllocation);

		// 2a. Include minAlignment
		vkMemoryRequirements.alignment = Max(vkMemoryRequirements.alignment, minAlignment);

		// 3. Allocate memory using allocator.
		res = allocator->AllocateMemory(
			vkMemoryRequirements,
			requiresDedicatedAllocation,
			prefersDedicatedAllocation,
			*pBuffer, // dedicatedBuffer
			VK_NULL_HANDLE, // dedicatedImage
			VmaBufferImageUsage(*pBufferCreateInfo), // dedicatedBufferImageUsage
			*pAllocationCreateInfo,
			SuballocationType::Buffer,
			1, // allocationCount
			pAllocation);

		if(res >= 0)
		{
			// 3. Bind buffer with memory.
			if((pAllocationCreateInfo->flags & VMA_ALLOCATION_CREATE_DONT_BIND_BIT) == 0)
			{
				res = allocator->BindBufferMemory(*pAllocation, 0, *pBuffer, 0);
			}
			if(res >= 0)
			{
				// All steps succeeded.
				#if 1
					(*pAllocation)->InitBufferUsage(*pBufferCreateInfo);
				#endif
				if(pAllocationInfo != 0)
				{
					allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
				}

				return VK_SUCCESS;
			}
			allocator->FreeMemory(
				1, // allocationCount
				pAllocation);
			*pAllocation = VK_NULL_HANDLE;
			vkDestroyBuffer(allocator->vkDevice, *pBuffer, vkAllocationCallbacks);
			*pBuffer = VK_NULL_HANDLE;
			return res;
		}
		vkDestroyBuffer(allocator->vkDevice, *pBuffer, vkAllocationCallbacks);
		*pBuffer = VK_NULL_HANDLE;
		return res;
	}
	return res;
}

VkResult vmaCreateAliasingBuffer(
	VmaAllocator allocator,
	Allocation* allocation,
	const VkBufferCreateInfo* pBufferCreateInfo,
	VkBuffer* pBuffer)
{
	return vmaCreateAliasingBuffer2(allocator, allocation, 0, pBufferCreateInfo, pBuffer);
}

VkResult vmaCreateAliasingBuffer2(
	VmaAllocator allocator,
	Allocation* allocation,
	VkDeviceSize allocationLocalOffset,
	const VkBufferCreateInfo* pBufferCreateInfo,
	VkBuffer* pBuffer)
{
	Assert(allocator && pBufferCreateInfo && pBuffer && allocation);
	Assert(allocationLocalOffset + pBufferCreateInfo->size <= allocation->GetSize());

	*pBuffer = VK_NULL_HANDLE;

	if (pBufferCreateInfo->size == 0)
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	// 1. Create VkBuffer.
	VkResult res = vkCreateBuffer(allocator->vkDevice, pBufferCreateInfo, vkAllocationCallbacks, pBuffer);
	if (res >= 0)
	{
		// 2. Bind buffer with memory.
		res = allocator->BindBufferMemory(allocation, allocationLocalOffset, *pBuffer, 0);
		if (res >= 0)
		{
			return VK_SUCCESS;
		}
		vkDestroyBuffer(allocator->vkDevice, *pBuffer, vkAllocationCallbacks);
	}
	return res;
}

void vmaDestroyBuffer(
	VmaAllocator allocator,
	VkBuffer buffer,
	Allocation* allocation)
{
	Assert(allocator);

	if(buffer == VK_NULL_HANDLE && allocation == VK_NULL_HANDLE)
	{
		return;
	}

	if(buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(allocator->vkDevice, buffer, vkAllocationCallbacks);
	}

	if(allocation != VK_NULL_HANDLE)
	{
		allocator->FreeMemory(
			1, // allocationCount
			&allocation);
	}
}

VkResult vmaCreateImage(
	VmaAllocator allocator,
	const VkImageCreateInfo* pImageCreateInfo,
	const VmaAllocationCreateInfo* pAllocationCreateInfo,
	VkImage* pImage,
	Allocation** pAllocation,
	AllocationInfo* pAllocationInfo)
{
	Assert(allocator && pImageCreateInfo && pAllocationCreateInfo && pImage && pAllocation);

	if(pImageCreateInfo->extent.width == 0 ||
		pImageCreateInfo->extent.height == 0 ||
		pImageCreateInfo->extent.depth == 0 ||
		pImageCreateInfo->mipLevels == 0 ||
		pImageCreateInfo->arrayLayers == 0)
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	*pImage = VK_NULL_HANDLE;
	*pAllocation = VK_NULL_HANDLE;

	// 1. Create VkImage.
	VkResult res = vkCreateImage(allocator->vkDevice, pImageCreateInfo, vkAllocationCallbacks, pImage);
	if(res == VK_SUCCESS)
	{
		SuballocationType suballocType = pImageCreateInfo->tiling == VK_IMAGE_TILING_OPTIMAL ?
			SuballocationType::ImageOptimal :
			SuballocationType::ImageLinear;

		// 2. Allocate memory using allocator.
		VkMemoryRequirements vkMemoryRequirements = {};
		bool requiresDedicatedAllocation = false;
		bool prefersDedicatedAllocation  = false;
		allocator->GetImageMemoryRequirements(*pImage, vkMemoryRequirements,
			requiresDedicatedAllocation, prefersDedicatedAllocation);

		res = allocator->AllocateMemory(
			vkMemoryRequirements,
			requiresDedicatedAllocation,
			prefersDedicatedAllocation,
			VK_NULL_HANDLE, // dedicatedBuffer
			*pImage, // dedicatedImage
			VmaBufferImageUsage(*pImageCreateInfo), // dedicatedBufferImageUsage
			*pAllocationCreateInfo,
			suballocType,
			1, // allocationCount
			pAllocation);

		if(res == VK_SUCCESS)
		{
			// 3. Bind image with memory.
			if((pAllocationCreateInfo->flags & VMA_ALLOCATION_CREATE_DONT_BIND_BIT) == 0)
			{
				res = allocator->BindImageMemory(*pAllocation, 0, *pImage, 0);
			}
			if(res == VK_SUCCESS)
			{
				// All steps succeeded.
				#if 1
					(*pAllocation)->InitImageUsage(*pImageCreateInfo);
				#endif
				if(pAllocationInfo != 0)
				{
					allocator->GetAllocationInfo(*pAllocation, pAllocationInfo);
				}

				return VK_SUCCESS;
			}
			allocator->FreeMemory(
				1, // allocationCount
				pAllocation);
			*pAllocation = VK_NULL_HANDLE;
			vkDestroyImage(allocator->vkDevice, *pImage, vkAllocationCallbacks);
			*pImage = VK_NULL_HANDLE;
			return res;
		}
		vkDestroyImage(allocator->vkDevice, *pImage, vkAllocationCallbacks);
		*pImage = VK_NULL_HANDLE;
		return res;
	}
	return res;
}

VkResult vmaCreateAliasingImage(
	VmaAllocator allocator,
	Allocation* allocation,
	const VkImageCreateInfo* pImageCreateInfo,
	VkImage* pImage)
{
	return vmaCreateAliasingImage2(allocator, allocation, 0, pImageCreateInfo, pImage);
}

VkResult vmaCreateAliasingImage2(
	VmaAllocator allocator,
	Allocation* allocation,
	VkDeviceSize allocationLocalOffset,
	const VkImageCreateInfo* pImageCreateInfo,
	VkImage* pImage)
{
	Assert(allocator && pImageCreateInfo && pImage && allocation);

	*pImage = VK_NULL_HANDLE;

	if (pImageCreateInfo->extent.width == 0 ||
		pImageCreateInfo->extent.height == 0 ||
		pImageCreateInfo->extent.depth == 0 ||
		pImageCreateInfo->mipLevels == 0 ||
		pImageCreateInfo->arrayLayers == 0)
	{
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	// 1. Create VkImage.
	VkResult res = vkCreateImage(allocator->vkDevice, pImageCreateInfo, vkAllocationCallbacks, pImage);
	if (res >= 0)
	{
		// 2. Bind image with memory.
		res = allocator->BindImageMemory(allocation, allocationLocalOffset, *pImage, 0);
		if (res >= 0)
		{
			return VK_SUCCESS;
		}
		vkDestroyImage(allocator->vkDevice, *pImage, vkAllocationCallbacks);
	}
	return res;
}

void vmaDestroyImage(
	VmaAllocator allocator,
	VkImage image,
	Allocation* allocation)
{
	Assert(allocator);

	if(image == VK_NULL_HANDLE && allocation == VK_NULL_HANDLE)
	{
		return;
	}

	if(image != VK_NULL_HANDLE)
	{
		vkDestroyImage(allocator->vkDevice, image, vkAllocationCallbacks);
	}
	if(allocation != VK_NULL_HANDLE)
	{
		allocator->FreeMemory(
			1, // allocationCount
			&allocation);
	}
}

//--------------------------------------------------------------------------------------------------

}	// namespace JC