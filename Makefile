CFLAGS = -Wall -Wextra

both: single multi

single: pm_heap.h pm_heap.c pm_heaptest_single.c
	gcc $(CFLAGS) -o pm_heap_single pm_heap.c pm_heaptest_single.c

multi: pm_heap.h pm_heap.c pm_heaptest_multi.c
	gcc $(CFLAGS) -o pm_heap_multi pm_heap.c pm_heaptest_multi.c -lpthread

clean:
	rm pm_heap_single pm_heap_multi
	rm -r disk