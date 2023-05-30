/*
*  pm_heaptest2.c / Assignment: Practicum 1
*
*  James Florez and John Ciolfi / CS5600 / Northeastern University
*  Spring 2023 / Mar 17, 2023
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pm_heap.h"

int main() {
    pm_init();

    // print initial state of heap
    printf("-- pm_heap state: %d heap pages and %d disk pages. Page size = %d B --\n", HEAP_PAGES, DISK_PAGES, PAGE_SIZE);
    pm_print_heap();
    pm_print_allocations();
    printf("Starting...\n---------------------------------------------------------\n");

    // pm_malloc with allocation > 1 page will fail.
    puts("✗ pm_malloc >1 page");
    pm_malloc(PAGE_SIZE + 1, NULL);

    // successful pm_malloc.
    puts("\n✔ pm_malloc c0");
    debug_t c0Details = { "c0" };
    page_t* c0 = pm_malloc(PAGE_SIZE, &c0Details);

    puts("\n------------------- Testing malformed inputs -------------------");

    // Test pm_free() with invalid inputs.
    // Page struct address offset below page_t memory segment.
    puts("\n--- Testing pm_free:");
    puts("\n✗ pm_free below page_t address segment");
    pm_free(c0 - PAGE_SIZE, &c0Details);
    // Page struct address offset above page_t memory segment.
    puts("\n✗ pm_free above page_t address segment");
    pm_free((c0 + ((HEAP_PAGES + DISK_PAGES) * sizeof(page_t)) + 1), &c0Details);
    // Page struct address within page_t memory segment but not pointing to the start of a page_t struct.
    puts("\n✗ pm_free within page_t memory segment but not pointing to the start of a page_t struct.");
    void* c0_offset = (void*) c0;
    c0_offset++;
    pm_free(c0_offset, &c0Details);
    // Valid page struct address but it has not yet been allocated.
    puts("\n✗ pm_free with a valid page struct address but it has not yet been allocated.");
    pm_free(c0 + 1, &c0Details);


    // Test pm_put with invalid inputs.
    puts("\n--- Testing pm_put:");
    puts("\n✗ pm_put with pos too low");
    pm_put(c0, -1, 'A', &c0Details);
    puts("\n✗ pm_put with pos too high");
    pm_put(c0, PAGE_SIZE + 1, 'A', &c0Details);
    puts("\n✗ pm_put below page_t address segment");
    pm_put(c0 - PAGE_SIZE, 0, 'A', &c0Details);
    puts("\n✗ pm_put above page_t address segment");
    pm_put((c0 + ((HEAP_PAGES + DISK_PAGES) * sizeof(page_t)) + 1), 0, 'A', &c0Details);
    puts("\n✗ pm_put within page_t memory segment but not pointing to start of page_t struct");
    pm_put(c0_offset, 0, 'A', &c0Details);
    puts("\n✗ pm_put with valid page struct address but not yet allocated");
    pm_put(c0 + 1, 0, 'A', &c0Details);

    // Test pm_access with invalid inputs.
    puts("\n--- Testing pm_access:");
    puts("\n✗ pm_access with pos too low");
    pm_access(c0, -1, &c0Details);
    puts("\n✗ pm_access with pos too high");
    pm_access(c0, PAGE_SIZE + 1, &c0Details);
    puts("\n✗ pm_access below page_t address segment");
    pm_access(c0 - PAGE_SIZE, 0, &c0Details);
    puts("\n✗ pm_access above page_t address segment");
    pm_access((c0 + ((HEAP_PAGES + DISK_PAGES) * sizeof(page_t)) + 1), 0, &c0Details);
    puts("\n✗ pm_access within page_t memory segment but not pointing to start of page_t struct");
    pm_access(c0_offset, 0, &c0Details);
    puts("\n✗ pm_access with valid page struct address but not yet allocated");
    pm_access(c0 + 1, 0, &c0Details);

    puts("\n----------------- Done testing malformed inputs -----------------");

    // successful pm_malloc.
    puts("\n✔ pm_malloc c1");
    debug_t c1Details = { "c1" };
    page_t* c1 = pm_malloc(PAGE_SIZE, &c1Details);

    // set a byte in c1's allocation.
    puts("\n✔ pm_put c1");
    pm_put(c1, 1, 'B', &c1Details);

    // set a byte in c0, making c1 the LRU page.
    puts("\n✔ pm_put c0");
    pm_put(c0, 0, 'A', &c0Details);

    // c1 should be evicted and saved to disk to make room for c2.
    puts("\n✔ pm_malloc c2 - evicts c1");
    debug_t c2Details = { "c2" };
    page_t* c2 = pm_malloc(PAGE_SIZE, &c2Details);
    if (c2 == NULL) {
        puts("Couldn't allocate c2");
    }

    puts("\n✗ pm_malloc with no more pages left");
    if (pm_malloc(PAGE_SIZE, NULL) == NULL) {
        puts("Couldn't allocate another page");
    }

    // access c1, bringing it back into memory from disk.
    puts("\n✔ pm_access c1 - evicts c0");
    char c1Val = pm_access(c1, 1, &c1Details);
    printf("Value of c1 at position 1 = %c\n", c1Val);

    // set a byte in c2, making c1 the LRU page.
    puts("\n✔ pm_put c2");
    pm_put(c2, 2, 'C', &c2Details);

    // access c2 when already in memory to show bytes are still there.
    puts("\n✔ pm_access c2");
    char c2Val = pm_access(c2, 2, &c2Details);
    printf("Value of c2 at position 2 = %c\n", c2Val);

    // access c0, evicting c1 (but c1 is clean so nothing re-written to disk).
    puts("\n✔ pm_access c0 - evicts c1 and c1 is clean");
    char c0Val = pm_access(c0, 0, &c0Details);
    printf("Value of c0 at position 0 = %c\n", c0Val);

    // access c1 when it was clean.
    puts("\n✔ pm_access c1 - after getting evicted but clean");
    c1Val = pm_access(c1, 1, &c1Details);
    printf("Value of c1 at position 1 = %c\n", c1Val);

    // Valid input for pm_free when the page is in memory.
    puts("\n✔ pm_free c0 - in memory");
    pm_free(c0, &c0Details);

    // Valid input for pm_free when the page is on disk.
    puts("\n✔ pm_free c1 - in memory");
    pm_free(c1, &c1Details);

    // Valid input for pm_free when the page is in memory.
    puts("\n✔ pm_free c2 - on disk");
    pm_free(c2, &c2Details);

    // successful pm_malloc and free after c0,c1,c2 freed.
    puts("\n✔ pm_malloc c4 - after c0, c1, and c2 were freed.");
    debug_t c4Details = { "c4" };
    page_t* c4 = pm_malloc(PAGE_SIZE, &c4Details);
    puts("\n✔ pm_free c4 - in memory");
    pm_free(c4, &c4Details);


    pm_cleanup(false);
    return 0;
}