/*
*  pm_heaptest.c / Assignment: Practicum 1
*
*  James Florez and John Ciolfi / CS5600 / Northeastern University
*  Spring 2023 / Mar 17, 2023
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "pm_heap.h"

#define NUM_THREADS 8
#define BYTES 8

void* thread_ptrs[NUM_THREADS];
debug_t thread_details[NUM_THREADS * 2];

// data to free 
struct pm_free_data {
    void* ptr;
    char name[16];
};
typedef struct pm_free_data pm_free_data_t;

/**
 * Thread that allocates memory and then frees it.
 * 
 * @param data the name of the thread.
 * @return NULL always.
*/
void* pm_exercise_thread(void *data) {
    // create message to be printed after successful pm_malloc
    int tNum = *((int*) data);

    sprintf(thread_details[tNum * 2].completionMsg, "✔ Allocated thread \"th%d\"", tNum);

    // try to allocate, print msg if unable to do so
    thread_ptrs[tNum] = pm_malloc(BYTES, &thread_details[tNum * 2]);
    if (!thread_ptrs[tNum]) {
        printf("✗ Could not allocate thread \"th%d\"\n", tNum);
        return NULL;
    }

    // adding usleep here seems to provide better variance in output
    usleep(1);

    char val = tNum + 'A';
    sprintf(thread_details[tNum * 2].completionMsg, "✔ Put value %c in thread \"th%d\"", val, tNum);
    pm_put(thread_ptrs[tNum], tNum, val, &thread_details[tNum * 2]);

    usleep(1);

    sprintf(thread_details[tNum * 2].completionMsg, "✔ Read value in thread \"th%d\"", tNum);
    char read_val = pm_access(thread_ptrs[tNum], tNum, &thread_details[tNum * 2]);

    printf("Value at index %d = %c\n", tNum, read_val);

    usleep(1);

    // free memory that was just allocated with given message
    sprintf(thread_details[tNum * 2 + 1].completionMsg, "✔ Freed thread \"th%d\"", tNum);

    pm_free(thread_ptrs[tNum], &thread_details[tNum * 2 + 1]);

    return NULL;
}


int main() {
    pm_init();

    // print initial state of heap
    printf("-- pm_heap state: %d heap pages and %d disk pages. Page size = %d B --\n", HEAP_PAGES, DISK_PAGES, PAGE_SIZE);
    pm_print_heap();
    pm_print_allocations();
    printf("Starting...\n---------------------------------------------------\n");

    // create threads that call pm_malloc
    pthread_t threads[NUM_THREADS];
    int threadIdx[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        threadIdx[i] = i;
        pthread_create(&threads[i], NULL, pm_exercise_thread, &threadIdx[i]);
    }

    // join pm_malloc threads so they finish
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pm_cleanup(false);
    return 0;
}