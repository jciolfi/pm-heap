/*
*  pm_heap.c / Assignment: Practicum 1
*
*  James Florez and John Ciolfi / CS5600 / Northeastern University
*  Spring 2023 / Mar 17, 2023
*/

#include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "pm_heap.h"

// print internal state of heap
void pm_print_debug(debug_t* debug_info, void* ptr);
void pm_print_allocations();

// lock access to shared variables (allocations, pm_heap_pages, pm_heap)
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// split physical memory into four regions: pages, allocation structures, available pages, available allocations
static char pm_heap[(HEAP_PAGES * PAGE_SIZE) + ((HEAP_PAGES + DISK_PAGES) * sizeof(page_t)) + HEAP_PAGES + (HEAP_PAGES + DISK_PAGES)];

// Counter used to keep track of page access.
static unsigned long lru_counter;

//------------ Helper functions not declared in pm_heap.h ---------------------//

/**
 * Find open page in page region of memory. NOTE: does not mark as used.
 * 
 * @return the page index for an unused page in memory or -1 if can't be found.
*/
int pm_find_page() {
    // search for unused page (denoted with '\0').
    for (int p = 0; p < HEAP_PAGES; p++) {
        if (pm_heap[AVAIL_PAGE_OFFSET + p] == '\0') {
            return p;
        }
    }

    return -1;
}

/**
 * Find open page in the allocation region of memory. NOTE: does not mark as used.
 * 
 * @return the index of the first page in the contiguous set of pages or -1 if can't be found.
*/
int pm_find_alloc() {
    // search for unused allocation space (denoted with '\0').
    for (int p = 0; p < HEAP_PAGES + DISK_PAGES; p++) {
        if (pm_heap[AVAIL_ALLOC_OFFSET + p] == '\0') {
            return p;
        }
    }

    return -1;
}

/**
 * Record the current value of the lru_counter to this page_t and then increment the lru_counter.
 *
 * @param page the page to record to.
 */
void pm_record_lru_counter(page_t* page) {
    page->lru_count = lru_counter++;
}

/**
 * Choose a page to page out based on the least-recently-used (LRU) strategy.
 *
 * @return the pointer of the page that should be saved to disk.
 */
page_t* pm_lru_page() {
    // Find the page in memory with the lowest lru_count value.
    unsigned long min_lru_count = lru_counter + 1;
    page_t* page_to_remove = NULL;
    for (int i = 0; i < HEAP_PAGES + DISK_PAGES; i++) {
        // Check if there is a page in memory at this index.
        if (pm_heap[AVAIL_ALLOC_OFFSET + i] != '\0') {
            page_t* current_page = (page_t*) (&pm_heap[ALLOC_OFFSET + (i * sizeof(page_t))]);
            if (current_page->page_idx >= 0 && current_page->lru_count < min_lru_count) {
                min_lru_count = current_page->lru_count;
                page_to_remove = current_page;
            }
        }
    }

    return page_to_remove;
}

/**
 * Save page to disk.
 *
 * @param page_to_evict the page to save.
 */
void pm_page_out(page_t* page_to_evict) {
    // if page isn't dirty, don't need to rewrite to memory.
    if (!page_to_evict->dirty) {
        memset(&pm_heap[page_to_evict->page_idx * PAGE_SIZE], '\0', PAGE_SIZE);
        page_to_evict->page_idx = -1;
        return;
    }

    // naming strategy is always pgX where X is the Xth page in allocation section.
    char filename[16];
    sprintf(filename, "%s/pg%d.bin", DISK_DIR, page_to_evict->alloc_idx);

    // open file to write to
    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Error pm_page_out(): couldn't open file, unable to write contents to disk for page %d\n", page_to_evict->alloc_idx);
        return;
    }

    // write page contents to disk.
    fwrite(&pm_heap[page_to_evict->page_idx * PAGE_SIZE], sizeof(char), PAGE_SIZE, file);
    fclose(file);

    // reset page in memory and fields for this allocation.
    memset(&pm_heap[page_to_evict->page_idx * PAGE_SIZE], '\0', PAGE_SIZE);
    page_to_evict->dirty = false;
    page_to_evict->page_idx = -1;
}

/**
 * Bring page from memory back into heap.
 *
 * @param page the page to bring into memory.
 * @param page_idx the index of where in memory the page should be placed.
 */
void pm_load_from_disk(page_t* page, int page_idx) {
    // update page_idx (now in memory)
    page->page_idx = page_idx;

    // naming strategy is always pgX where X is the Xth page in allocation section.
    char filename[16];
    sprintf(filename, "%s/pg%d.bin", DISK_DIR, page->alloc_idx);

    // open file to read from.
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        // reset page contents to '\0' by default.
        memset(&pm_heap[page_idx * PAGE_SIZE], '\0', PAGE_SIZE);
        printf("Error pm_load_from_disk(): Couldn't open file, unable to read contents from disk into memory for alloc %d\n", page->alloc_idx);
        return;
    }

    // see how large file is (should be size of a page).
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size != PAGE_SIZE) {
        // reset page contents to '\0' by default.
        memset(&pm_heap[page_idx * PAGE_SIZE], '\0', PAGE_SIZE);
        printf("Error pm_load_from_disk(): Size of page on disk is not equal to configured page size for alloc %d\n", page->alloc_idx);
        return;
    }
    fseek(file, 0, SEEK_SET);

    // read disk contents into memory.
    fread(&pm_heap[page_idx * PAGE_SIZE], PAGE_SIZE, 1, file);

    fclose(file);
}

/**
 * If page is not current in memory, bring into memory by finding open page or evicting a page in memory.
 * 
 * @param page the page to potentially bring into memory from disk.
*/
void pm_load_alloc(page_t* page) {
    if (page->page_idx < 0) {
        // check if open page in memory.
        int open_page_idx = pm_find_page();

        // if no open page, evict a page currently in memory.
        if (open_page_idx < 0) {
            page_t *page_to_evict = pm_lru_page();
            open_page_idx = page_to_evict->page_idx;
            pm_page_out(page_to_evict);
        }

        // mark page as in use
        pm_heap[AVAIL_PAGE_OFFSET + open_page_idx] = '1';

        // load contents back into memory
        pm_load_from_disk(page, open_page_idx);
    }
}

/**
 * Print internal state of the heap.
 *
 * @param debug_info the debug info to print with.
 * @param ptr the pointer to print with.
 */
void pm_print_debug(debug_t* debug_info, void* ptr) {
    if (debug_info) {
        printf("%s (%p)\n", debug_info->completionMsg, ptr);
        pm_print_heap();
        pm_print_allocations();
    }
}

/**
 * Determine if the given ptr is a valid page_t structure.
 * 
 * @param ptr the pointer to evaluate.
 * @return true if valid pointer, false otherwise.
*/
bool pm_invalid_alloc(page_t* ptr) {
    if (!ptr) {
        return false;
    }

    unsigned long heap_idx = ((void*) ptr) - ((void*) pm_heap);

    return heap_idx < ALLOC_OFFSET
        || heap_idx > AVAIL_PAGE_OFFSET
        || (heap_idx - ALLOC_OFFSET) % sizeof(page_t) != 0
        || ptr->lru_count == 0;
}

//------------ Functions declared in pm_heap.h ---------------------//

page_t* pm_malloc(unsigned long bytes, debug_t* debug_info) {
    // Validate the size of the request.
    if (bytes > PAGE_SIZE) {
        printf("Error pm_malloc(): requested allocation size is invalid (%lu).\n", bytes);
        return NULL;
    }

    // request lock - will be released on every code path
    pthread_mutex_lock(&lock);

    // find allocation in memory. If none left, return NULL.
    int alloc_idx = pm_find_alloc();
    if (alloc_idx < 0) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    // find page in memory. If none left, evict an existing page in memory.
    int page_idx = pm_find_page();
    if (page_idx < 0) {
        // select which page to evict using the LRU strategy.
        page_t* page_to_evict = pm_lru_page();
        page_idx = page_to_evict->page_idx;
        pm_page_out(page_to_evict);
    }

    // create new page_num in pm_heap - set dirty initially
    page_t new_page = { alloc_idx, page_idx, true, lru_counter++ };
    page_t* new_page_ptr = (page_t*) (&pm_heap[ALLOC_OFFSET + (alloc_idx * sizeof(page_t))]);
    *new_page_ptr = new_page;

    // mark page as allocated
    pm_heap[AVAIL_ALLOC_OFFSET + alloc_idx] = '1';
    pm_heap[AVAIL_PAGE_OFFSET + page_idx] = '1';

    // unlock, print debug info
    pm_print_debug(debug_info, (void*) new_page_ptr);
    pthread_mutex_unlock(&lock);
    return new_page_ptr;
}

void pm_free(page_t* ptr, debug_t* debug_info) {
    // request lock - will be released on every code path.
    pthread_mutex_lock(&lock);

    // Validate that ptr points to a correct page_t address.
    if (pm_invalid_alloc(ptr)) {
        printf("Error pm_free(): page_t* arg does not point to a valid address.\n");
        pthread_mutex_unlock(&lock);
        return;
    }

    // remove disk page.
    char filename[16];
    sprintf(filename, "%s/pg%d.bin", DISK_DIR, ptr->alloc_idx);
    remove(filename);

    // reset any calls to pm_put and mark page in memory as available.
    if (ptr->page_idx >= 0) {
        memset(&pm_heap[ptr->page_idx * PAGE_SIZE], '\0', PAGE_SIZE);
        pm_heap[AVAIL_PAGE_OFFSET + ptr->page_idx] = '\0';
    }

    // mark allocation space as available.
    unsigned int alloc_idx = ptr->alloc_idx;
    pm_heap[AVAIL_ALLOC_OFFSET + alloc_idx] = '\0';

    pm_print_debug(debug_info, ptr);

    // reset page_t pointed to by ptr.
    memset(&pm_heap[ALLOC_OFFSET + (alloc_idx * sizeof(page_t))], '\0', sizeof(page_t));

    pthread_mutex_unlock(&lock);
}

char pm_access(page_t* ptr, int pos, debug_t* debug_info) {
    pthread_mutex_lock(&lock);

    // check for an invalid page_t ptr.
    if (pm_invalid_alloc(ptr)) {
        printf("Error pm_access(): page_t* arg does not point to a valid address.\n");
        pthread_mutex_unlock(&lock);
        return '\0';
    }

    // check for invalid position.
    if (pos < 0 || pos >= PAGE_SIZE) {
        printf("Error pm_access(): pos arg is invalid (%d).\n", pos);
        pthread_mutex_unlock(&lock);
        return '\0';
    }

    // load allocation into memory if not already present in memory.
    pm_load_alloc(ptr);

    // increment counter, get char at position.
    pm_record_lru_counter(ptr);
    char result = pm_heap[ptr->page_idx * PAGE_SIZE + pos];
    
    pm_print_debug(debug_info, ptr);
    pthread_mutex_unlock(&lock);
    return result;
}

void pm_put(page_t* ptr, int pos, char val, debug_t* debug_info) {
    pthread_mutex_lock(&lock);

    // check for an invalid page_t ptr.
    if (pm_invalid_alloc(ptr)) {
        printf("Error pm_put(): page_t* arg does not point to a valid address.\n");
        pthread_mutex_unlock(&lock);
        return;
    }

    // check for invalid position.
    if (pos < 0 || pos >= PAGE_SIZE) {
        printf("Error pm_put(): pos arg is invalid (%d).\n", pos);
        pthread_mutex_unlock(&lock);
        return;
    }

    // load allocation into memory if not already present in memory.
    pm_load_alloc(ptr);

    // increment lru counter, update char at pos, set dirty.
    pm_record_lru_counter(ptr);
    pm_heap[ptr->page_idx * PAGE_SIZE + pos] = val;
    ptr->dirty = true;

    pm_print_debug(debug_info, ptr);
    pthread_mutex_unlock(&lock);
}

void pm_init() {
    pthread_mutex_lock(&lock);

    // create directory for pages on disk.
    struct stat st = {0};

    if (stat(DISK_DIR, &st) == -1) {
        mkdir(DISK_DIR, 0700);
    }

    // set initial lru counter value
    lru_counter = 1;

    pthread_mutex_unlock(&lock);
}

void pm_print_heap() {
    // print actual page bytes in the heap.
    printf("  ⓘ heap:        [ ");
    for (unsigned long i = 0; i < HEAP_PAGES * PAGE_SIZE; i++) {
        if (i > 0 && i % PAGE_SIZE == 0) {
            printf("| ");
        }

        printf("%d ", pm_heap[i] != '\0');
    }
    printf("]\n");

    // print which pages are available / not.
    printf("    avail pages: [ ");
    for (int i = 0; i < HEAP_PAGES; i++) {
        printf("%d ", pm_heap[AVAIL_PAGE_OFFSET + i] != '\0');
    }
    printf("]\n");
}

void pm_print_allocations() {
    // print available allocations.
    printf("    avail alloc: [ ");
    for (int i = 0; i < HEAP_PAGES + DISK_PAGES; i++) {
        printf("%d ", pm_heap[AVAIL_ALLOC_OFFSET + i] != '\0');
    }
    printf("]\n");

    // print allocations themselves.
    printf("    allocations: ");
    bool first = true;
    for (int i = 0; i < HEAP_PAGES + DISK_PAGES; i++) {
        if (pm_heap[AVAIL_ALLOC_OFFSET + i] != '\0') {
            page_t* current_page = (page_t*) (&pm_heap[ALLOC_OFFSET + (i * sizeof(page_t))]);
            
            if (first) {
                first = false;
            } else {
                printf(", ");
            }

            printf("{alloc_idx=%d, page_idx=%d, dirty=%d, lru_count=%ld}",
                current_page->alloc_idx, current_page->page_idx, current_page->dirty, current_page->lru_count);
        }
    }
    // print None if no allocations yet.
    if (first) {
        printf("None");
    }
    printf("\n");
}

void pm_cleanup(bool rm_disk) {
    // option to remove disk directory.
    if (rm_disk) {
        rmdir(DISK_DIR);
    }
    
    pthread_mutex_destroy(&lock);
}