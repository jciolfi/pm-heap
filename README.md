# Build a Custom Heap

## How to run
- Compile the code with `make`.
    - There will be two binaries: pm_heap_multi and pm_heap_single.
- To test the multithreaded version, run the code with `./pm_heap_multi`.
- To test the singlethreaded version, run the code with `./pm_heap_single`.

## Assumptions
- Any call to pm_malloc that tries to allocate more than one page will be rejected (NULL is returned).
- A single page cannot hold multiple allocations. Even if a page would have enough room to hold multiple allocations, this will not happen.
- The total number of allocations can fit in a standard int.

## Approach
- There is a `page_t` structure that tracks the state of an allocation. The fields are:
    - unsigned int alloc_idx: the index of the allocation; also the unique identifier for the allocation.
    - int page_idx: the index of the page in memory. If page_idx < 0, then we know the page isn't in memory.
    - bool dirty: whether or not the page has been written to since it was last saved.
    - unsigned long lru_count: a relative counter representing how recently the page was used.
- The physical `pm_heap` is split up into four regions: pages, allocation structures, available pages, and available allocations.
    - pages: represents the actual pages whose individual bytes can be set.
    - allocation structures: the `page_t` structures that track the state of each allocation.
    - available pages: a simple array representing which pages in memory are in use and not in use.
    - availabe allocations: a simple array representing which allocations are in use and not in use.
- The number of pages in memory and on disk are configurable. The sum of these two values are treated as the number of available allocations since each allocation is assumed to be a single page. 
- For a call to `pm_malloc`, we first look to see if there is an available allocation. 
    - If there isn't, NULL is returned. 
    - If there is, we check to see if there is an open page in memory. 
        - If there is, that open page is associated with the allocation entry. 
        - If there isn't, then the least recently used (LRU) page is selected by finding the minimum lru_count for the pages currently in memory. This LRU page is evicted by getting written to disk if the dirty bit is set. This eviction makes room for the new allocation. A pointer to the new allocation is returned.
- For a call to `pm_free`, we first look to see if the requested ptr is valid. 
    - If it's not valid, we return from `pm_free`.
    - If it is, we remove the associated disk page if it exists. If the allocation is in memory, we reset the bytes for that page and mark that page as available. We finally mark the allocation space as available.
- For a call to `pm_put`, we first look to see if the requested ptr is valid.
    - If it's not valid, we return from `pm_put`.
    - If it's valid, we check to see if the page associate with the allocation is in memory.
        - If it's not in memory, we check to see if there is an open page in memory.
            - If there's no open page, we must evict a page currently in memory. We choose the LRU page based on the lowest lru_count value of the pages in memory.
        - Now that the page is in memory, we update the lru_count and set the byte at the relative position for the page to the given value. We set the allocation as dirty.
- For a call to `pm_access`, we first look to see if the requested ptr is valid.
    - If it's not valid, we return from `pm_put`.
    - If it's valid, we check to see if the page associate with the allocation is in memory.
        - If it's not in memory, we check to see if there is an open page in memory.
            - If there's no open page, we must evict a page currently in memory. We choose the LRU page based on the lowest lru_count value of the pages in memory.
        - Now that the page is in memory, we update the lru_count and return the byte at the relative position for the page to the given value.
- There is a lock defined at the same scope as the `pm_heap` to make sure that any threads that use `pm_heap` will not interfere with each other. Since only one thread can hold the lock when it allocates/frees pages in `pm_heap`, the heap will never get corrupted by multiple threads trying to access it at once.

## Notes
- To test the multithreaded version, I created 8 threads that will try to allocate a small amount of memory (4 bytes or sizeof(int)), sleep for 1 microsecond, and then free the memory. I print out the action of the command (allocate or free) and the thread name as well as the internal state of the program-managed heap.
    - I found that sleeping for 1 microsecond resulted in slightly more variation of thread ordering when run multiple times.