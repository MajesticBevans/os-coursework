/*
 * The Buddy Page Allocator
 * SKELETON IMPLEMENTATION TO BE FILLED IN FOR TASK 2
 */

#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

#define MAX_ORDER	18

/**
 * A buddy page allocation algorithm.
 */
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
private:

	/** Given an order, returns the number of pages in a block of that order.
	 * @param order The order.
	 * @return The number of pages.
	*/
	constexpr pfn_t pages_per_block(int order)
	{
		return 1 << order;
	}

	/**
	 * Given a block size, returns the highest order that will fit inside it.
	 * @param block_size The block size.
	 * @return The highest order.
	 */
	int get_max_order(int block_size)
	{
		int order = 0;
		while ((1 << order) < block_size) 
		{
			order++;
		}
		return order;
	}

	/** Given a page descriptor and an order, returns the offset between the start of the nearest
	 * block of that order and the pfn of the page. 
	 * @param pgd The page descriptor to find the offset for.
	 * @param order The order of block from which to check the offset.
	 * @return The offset between the page and the start of the block.
	 */
	constexpr pfn_t page_offset_from_block(PageDescriptor* pgd, int order)
	{
		return sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order);
	}

	/**
	 * Helper function that inserts a block into the _free_areas array at the level specified in order.
	 * @param block The page descriptor of the first page in the block.
	 * @param order The order level into which to insert the block.
	 */
	void insert_block_into_free_areas(PageDescriptor* block, int order)
	{
		PageDescriptor** block_list = &_free_areas[order];

		PageDescriptor* insert_after = *block_list;

		if (!insert_after) 
		{ 
			*block_list = block; 
			block->next_free = nullptr;
			block->prev_free = nullptr;
			return;
		}
		else if (block < insert_after)
		{
			*block_list = block;
			block->next_free = insert_after;
			block->prev_free = nullptr;
			return;
		}

		while (insert_after && block > insert_after) { insert_after = insert_after->next_free; }

		block->prev_free = insert_after;

		if (insert_after)
		{
			block->next_free = insert_after->next_free;
			if (insert_after->next_free) { insert_after->next_free->prev_free = block; }
			insert_after->next_free = block;
		}
		else
		{
			block->next_free = nullptr;
		}
	}

	/**
	 * Helper function that removes a block from a given order level in _free_areas 
	 * @param block The page descriptor of the first page in the block.
	 * @param order The order level into which to insert the block.
	 */
	void remove_block_from_free_areas(PageDescriptor* block, int order)
	{
		assert(block);
		PageDescriptor** block_list = &_free_areas[order];

		if (block->prev_free) { block->prev_free->next_free = block->next_free; }
		if (block->next_free) { block->next_free->prev_free = block->prev_free; }
		if (*block_list == block) { *block_list = block->next_free; }
	}

	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	 * to the left or the right of PGD, in the given order.
	 * @param pgd The page descriptor to find the buddy for.
	 * @param order The order in which the page descriptor lives.
	 * @return Returns the buddy of the given page descriptor, in the given order.
	 */
	PageDescriptor* buddy_of(PageDescriptor* pgd, int order)
	{
		// assertions
		assert(order <= MAX_ORDER);

		// offset setups
		pfn_t order_offset = pages_per_block(order);
		pfn_t offset_from_block_start = page_offset_from_block(pgd, order);
		pfn_t offset_from_higher_block_start = page_offset_from_block(pgd, order + 1);

		// buddy logic
		if (offset_from_block_start == 0 && offset_from_higher_block_start == 0) { return pgd + order_offset; }
		else if (offset_from_block_start == 0) { return pgd - order_offset; }
		else if (offset_from_higher_block_start > offset_from_block_start) { return pgd - offset_from_higher_block_start; }
		else { return pgd + order_offset - offset_from_block_start; }
	}

	/**
	 * Given a pointer to a block of free memory in the order "source_order", this function will
	 * split the block in half, and insert it into the order below.
	 * @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	 * @param source_order The order in which the block of free memory exists.  Naturally,
	 * the split will insert the two new blocks into the order below.
	 * @return Returns the left-hand-side of the new block.
	 */
	PageDescriptor* split_block(PageDescriptor** block_pointer, int source_order)
	{
		// assertions
		assert(source_order <= MAX_ORDER);
		assert(source_order > 0);

		// setup source variables
		PageDescriptor* source_block = *block_pointer;
		PageDescriptor** source_block_list = &_free_areas[source_order];

		// remove source block from source order free area list
		remove_block_from_free_areas(source_block, source_order);

		// setup new variables
		int new_order = source_order - 1;
		PageDescriptor* newBuddy = buddy_of(source_block, new_order);

		//add new block and buddy to new free area list
		insert_block_into_free_areas(source_block, new_order);
		insert_block_into_free_areas(newBuddy, new_order);

		return newBuddy;
	}

	/**
	 * Takes a block in the given source order, and merges it (and its buddy) into the next order.
	 * @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	 * @param source_order The order in which the pair of blocks live.
	 * @return Returns the new slot that points to the merged block.
	 */
	PageDescriptor** merge_block(PageDescriptor** block_pointer, int source_order)
	{
		// assertions
		assert(source_order >= 0 && source_order < MAX_ORDER);

		// setup variables
		PageDescriptor* source_block = *block_pointer;
		PageDescriptor* source_buddy = buddy_of(source_block, source_order);
		int new_order = source_order + 1;
		PageDescriptor *left_block, *right_block;

		if (source_block < source_buddy)
		{
			left_block = source_block;
			right_block = source_buddy;
		}
		else
		{
			left_block = source_buddy;
			right_block = source_block;
		}

		// remove source block and buddy from source order free area list
		PageDescriptor** source_block_list = &_free_areas[source_order];
		remove_block_from_free_areas(left_block, source_order);
		remove_block_from_free_areas(right_block, source_order);

		// add merged block to new free area list
		insert_block_into_free_areas(left_block, new_order);

		return &left_block;
	}

	/**
	 * Merges all free blocks that can be merged with their buddy. 
	 */
	void full_merge()
	{
		for (int i = 0; i < MAX_ORDER; i++)
		{
			PageDescriptor* block = _free_areas[i];
			
			while (block->next_free)
			{
				if (block->next_free == buddy_of(block, i))
				{
					merge_block(&block, i);
				}	
			}
		}
	}

	/**
	 * Helper function that takes a starting pfn, range size and top order level, and removes any blocks containing
	 * pages in that range below the order level
	 * @param start start pfn
	 * @param count size of page range
	 * @param top_order order level under which to remove the blocks
	 */
	void remove_lower_order_blocks_in_range(pfn_t start, int count, int top_order)
	{
		for (int i = 0; i < top_order; i++)
		{
			PageDescriptor* block = _free_areas[i];
			pfn_t pfn = sys.mm().pgalloc().pgd_to_pfn(block);

			while (block)
			{
				PageDescriptor* next = block->next_free;
				if (start <= pfn && pfn < start + count) { remove_block_from_free_areas(block, i); }
				block = next;
			}
		}
		
	}


public:
	/**
	 * Allocates 2^order number of contiguous pages
	 * @param order The power of two, of the number of contiguous pages to allocate.
	 * @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	 * allocation failed.
	 */
	PageDescriptor* allocate_pages(int order) override
	{
		// ensure all memory is in highest order possible
		full_merge();

		int tempOrder = order;

		while (tempOrder >= 0)
		{
			PageDescriptor** block_list = &_free_areas[tempOrder];
			PageDescriptor* head = *block_list;

			if (!head) { --tempOrder; continue; }

			// the number of contiguous blocks in this order needed to satisfy the allocation.
			int blocksNeeded = pages_per_block(order - tempOrder);

			PageDescriptor* startBlock = head;
			int consecutive_count = 1;
			bool pagesFound = true;

			// loop through free blocks to see if enough are contiguous to fill order
			while (consecutive_count < blocksNeeded)
			{
				if (!startBlock->next_free) { pagesFound = false; break; }

				if (startBlock->next_free == startBlock + pages_per_block(tempOrder)) { ++consecutive_count; }
				else { startBlock = startBlock->next_free; }
			}

			if (!pagesFound) { --tempOrder; continue; }
			// if enough contiguous blocks found, remove from free areas and return start
			else
			{
				for (int i = 0; i < consecutive_count; i++)
				{
					remove_block_from_free_areas(startBlock + i * pages_per_block(tempOrder), tempOrder);
				}
				return startBlock;
			}
		}  

		return NULL;
	}

    /**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
    void free_pages(PageDescriptor* pgd, int order) override
    {
		pfn_t block_offset = page_offset_from_block(pgd, order);
		pfn_t pfn = sys.mm().pgalloc().pgd_to_pfn(pgd);

		if (block_offset == 0)
		{
			remove_lower_order_blocks_in_range(pfn, pfn + pages_per_block(order), order);
			insert_block_into_free_areas(pgd, order);
			return;
		}
		else
		{

		}
    }

    /**
     * Marks a range of pages as available for allocation.
     * @param start A pointer to the first page descriptors to be made available.
     * @param count The number of page descriptors to make available.
     */
    virtual void insert_page_range(PageDescriptor* start, uint64_t count) override
    {
        if(!start) { return; }
		PageDescriptor* block = start;
		int start_pfn = sys.mm().pgalloc().pgd_to_pfn(start);
		int final_pfn = sys.mm().pgalloc().pgd_to_pfn(start) + count;

		while(true)
		{
			//find highest order to insert block

			int 
		}
    }

    /**
     * Marks a range of pages as unavailable for allocation.
     * @param start A pointer to the first page descriptors to be made unavailable.
     * @param count The number of page descriptors to make unavailable.
     */
    virtual void remove_page_range(PageDescriptor* start, uint64_t count) override
    {
        // TODO: Implement me!
    }

	/**
	 * Initialises the allocation algorithm.
	 * @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	 */
	bool init(PageDescriptor* page_descriptors, uint64_t nr_page_descriptors) override
	{
		mm_log.messagef(LogLevel::DEBUG, "Buddy Page Allocator online");
        return true;
	}

	/**
	 * Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "buddy"; }

	/**
	 * Dumps out the current state of the buddy system
	 */
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");

		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);

			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg) {
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}

			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
		}
	}

private:
	PageDescriptor* _free_areas[MAX_ORDER+1];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
 * Allocation algorithm registration framework
 */
RegisterPageAllocator(BuddyPageAllocator);