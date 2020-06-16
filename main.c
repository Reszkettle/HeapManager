#include <stdio.h>
#include "heap.h"
#include <assert.h>

#define PAGE_SIZE 4096

int main()
{
    int* check = heap_malloc(123);
    assert(check == NULL); // log: heap doesn't exist

    check = heap_realloc(check, 12);
    assert(check == NULL); // log: heap doesn't exist

    check = heap_realloc_aligned(check, 12);
    assert(check == NULL); // log: heap doesn't exist

    check = heap_malloc_aligned(12);
    assert(check == NULL); // log: heap doesn't exist

    printf("\n\n\nTESTS AFTER SETUPING HEAP\n");

    int status = heap_setup();
    assert(status == 0); // must be true

    size_t usedSpace = heap_get_used_space();
    size_t freeSpace = heap_get_free_space();
    assert(usedSpace == 3 * sizeof(Chunk)); // 2 for guards, 1 for free block -> must be true
    assert(heap_get_free_space() == 4096 - usedSpace); // usedSpace + freeSpace == totalSpace
    assert(heap_get_free_gaps_count() == 1);
    assert(heap_get_used_blocks_count() == 2); // 2 for guards

    int* firstBlock = heap_malloc(13);
    assert(firstBlock != NULL); // must be true
    assert(heap_get_largest_used_block_size() % sizeof(void*) == 0); // firstBlock size must be divisible by sizeof(void*)
    assert(heap_get_used_blocks_count() == 3); // 2 guards + firstBlock

    int* secondBlock = heap_malloc(4200); // must be True
    assert(secondBlock != NULL);
    assert(heap_get_free_space() + heap_get_used_space() == 2 * PAGE_SIZE); // malloc should ask for additional page
    assert(heap_get_used_blocks_count() == 4); // 2 guards + firstBlock + secondBlock
    assert(heap_get_free_gaps_count() == 1);

    // firstBlock should be before secondBlock
    Chunk* firstBlockChunk = (Chunk*)firstBlock - 1;
    Chunk* secondBlockChunk = (Chunk*)secondBlock - 1;
    assert(firstBlockChunk->next == secondBlockChunk); // There shouldn't be any gaps between first and second block

    int* thirdBlock = heap_malloc(10000000000); // log: Couldn't get enough space from OS
    assert(thirdBlock == NULL);


    thirdBlock = heap_malloc(3656);
    assert(heap_get_free_gaps_count() == 0); // true because all blocks are used
    assert(heap_get_used_blocks_count() == 5); // as above

    int* failBlock = heap_malloc(0); // log: Invalid count
    assert(failBlock == NULL); // true because count isn't greater 0

    int* newMemory = heap_realloc(thirdBlock, 2000);
    assert(newMemory == thirdBlock); // must be true because we are decreasing the size

    newMemory = heap_realloc(thirdBlock, 3000);
    assert(newMemory == thirdBlock); // must be true because next block is free and there's enough space

    int* newFirstBlock = heap_realloc(firstBlock, 23);
    assert(newFirstBlock != firstBlock); // must be true because there's not enough space for resizing
    firstBlock = newFirstBlock; // just to not make mess with other pointers (easier for testing)

    newMemory = heap_realloc(firstBlock, 10000000000); // log: Couldn't get space from OS
    assert(newMemory == NULL); // as above

    newMemory = heap_realloc(firstBlock, 0);
    assert(newMemory == NULL); // should work as Free
    assert(heap_get_used_blocks_count() == 4); // 2 for guards + secondBlock + thirdBlock
    firstBlock = newMemory;

    firstBlock = heap_realloc(firstBlock, 3 * PAGE_SIZE); // ask for additional Pages
    assert(firstBlock != NULL); // success so firstBlock shouldn't be NULL

    assert(heap_get_used_blocks_count() == 5 && heap_get_free_gaps_count() == 2); // again we have 5 used blocks (2 guards + 3 blocks)
    heap_free(firstBlock);
    heap_free(secondBlock);
    heap_free(thirdBlock); // Clear heap 2 used, 1 free block
    heap_free(thirdBlock); // log: Invalid chunk <not exists>
    assert(heap_get_used_blocks_count() == 2 && heap_get_free_gaps_count() == 1); // back to normal


    firstBlock = heap_malloc_aligned(10);
    assert(firstBlock != NULL); // enough space so true
    assert(((intptr_t)firstBlock & (PAGE_SIZE)-1 )== 0); // true because it's aligned

    secondBlock = heap_malloc_aligned(100000);
    assert(secondBlock != NULL); // enough space but we have to ask for pages
    assert(((intptr_t)secondBlock & (PAGE_SIZE) - 1 )== 0); // true because it's aligned


    thirdBlock = heap_malloc_aligned(1000000000000); // way too much so failure, custom_sbrk failed
    assert(thirdBlock == NULL); // as above

    thirdBlock = heap_malloc_aligned(10);
    assert(thirdBlock != NULL); // enough space
    assert((heap_get_used_space() + heap_get_free_space()) % PAGE_SIZE == 0);

    newMemory = heap_realloc_aligned(firstBlock, 1002); // enough space didn't need to call malloc
    assert(newMemory == firstBlock); // as above
    firstBlock = newMemory;

    newMemory = heap_realloc_aligned(firstBlock, 5000); // not enough space after block so we have to call malloc
    assert(newMemory != firstBlock); // as above
    firstBlock = newMemory;
    assert((heap_get_used_space() + heap_get_free_space()) % PAGE_SIZE == 0); // size must be divisible by PAGE_SIZE

    heap_dump_debug_information();

    heap_validate();
    heap_free(firstBlock);
    heap_free(secondBlock);
    heap_free(thirdBlock);
    heap_validate();

    assert((heap_get_used_space() + heap_get_free_space()) % PAGE_SIZE == 0); // size must be divisible by PAGE_SIZE
    assert(heap_get_used_blocks_count() == 2 && heap_get_free_gaps_count() == 1);

    return 0;
}
