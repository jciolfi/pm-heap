/*
*  pm_heap.h / Assignment: Practicum 1
*
*  James Florez and John Ciolfi / CS5600 / Northeastern University
*  Spring 2023 / Mar 17, 2023
*/

#ifndef PM_HEAP_H
#define PM_HEAP_H

#include <stdlib.h>
#include <stdbool.h>

// size of a page
#define PAGE_SIZE 8
// available pages in heap
#define HEAP_PAGES 2
// available pages for disk
#define DISK_PAGES 1
// directory name for where pages on disk go
#define DISK_DIR "disk"
// offset for where allocation structures start
#define ALLOC_OFFSET (PAGE_SIZE * HEAP_PAGES)
// offset for where available pages start
#define AVAIL_PAGE_OFFSET (ALLOC_OFFSET + ((HEAP_PAGES + DISK_PAGES) * sizeof(page_t)))
// offset for where available allocations start
#define AVAIL_ALLOC_OFFSET (AVAIL_PAGE_OFFSET + HEAP_PAGES)

// represents an allocated page
struct pm_allocation {
    // unique ID for this allocation.
    unsigned int alloc_idx;
    // index of page in memory. If < 0, allocation is not in memory.
    int page_idx;
    // if true, page modified since last written to disk.
    bool dirty;
    // counter to track recently used pages.
    unsigned long lru_count;
};
typedef struct pm_allocation page_t;

// debug information
struct pm_debug {
    char completionMsg[64];
};
typedef struct pm_debug debug_t;

/**
 * Try to allocate the requested number of bytes.
 *
 * @param bytes the number of bytes to allocate.
 * @param debug_info the debug info to print with successful allocation, or NULL.
 * @return a pointer where bytes were allocated in pm_heap, or NULL if insufficient space. 
*/
page_t* pm_malloc(unsigned long bytes, debug_t* debug_info);

/**
 * Reclaim memory used by a pointer called from pm_malloc.
 * 
 * @param ptr the pointer to free.
 * @param debug_info the debug info to print with successful free, or NULL.
*/
void pm_free(page_t* ptr, debug_t* debug_info);

/**
 * Read the byte at position for the given ptr.
 *
 * @param ptr the pointer to read from.
 * @param pos the position of the byte to read.
 * @param debug_info the debug info to print with, or NULL.
 * @return char at the given position for the page.
 */
char pm_access(page_t* ptr, int pos, debug_t* debug_info);

/**
 * Set the byte at position for the given ptr.
 *
 * @param ptr the pointer to set from.
 * @param pos the position of the byte to set.
 * @param val  the value to set.
 * @param debug_info the debug info to print with, or NULL.
 */
void pm_put(page_t* ptr, int pos, char val, debug_t* debug_info);

/**
 * Initialization. Call before any other function in pm_heap.
 * 
*/
void pm_init();

/**
 * Print the current state of heap pages and available pages. Can be used for debugging.
*/
void pm_print_heap();

/**
 * Print the current state of available allocations and allocations. Can be used for debugging.
*/
void pm_print_allocations();

/**
 * Cleanup when finished. E.g. call before returning from main.
 * 
*/
void pm_cleanup(bool rm_disk);

#endif