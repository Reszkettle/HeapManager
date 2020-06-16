#ifndef MYHEAP_HEAP_H
#define MYHEAP_HEAP_H


#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>

#define __f __FUNCTION__

typedef struct DebugParams{
    const char* fileName;
    uint8_t lineNumber;
}DebugParams;

typedef struct Chunk{
    int32_t firstFence;
    int32_t size;
    bool isFree;
    struct Chunk* next;
    struct Chunk* prev;
    int32_t sumOfBytes;
    DebugParams debugParams;
    int32_t secondFence;
}Chunk;

typedef struct Boundaries{
    Chunk* leftBound;
    Chunk* rightBound;
}Boundaries;

typedef struct ChunkCount{
    uint32_t free;
    uint32_t used;
}ChunkCount;


typedef struct Heap{
    int32_t firstFence;
    bool isInitialized;
    ChunkCount chunksCount;
    int32_t sumOfBytes;
    Chunk* head;
    Chunk* tail;
    Boundaries boundaries;
    int32_t secondFence;
}Heap;

enum pointer_type_t
{
    pointer_null,
    pointer_out_of_heap,
    pointer_control_block,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

void ConsoleLog(char*, char*);
void setBoundaries(void*);
void setFences(int, ...);
void setSum(int, ...);
void heapSetSum();
size_t ceilWord(size_t);
int getSpace(intptr_t);
void splitChunk(Chunk* firstChunk, size_t count);
void mergeChunks(Chunk* firstChunk, Chunk* secondChunk);
bool chunkExists(Chunk*);
Chunk* findAligned(size_t);
intptr_t alignedMemory(intptr_t, intptr_t);
void updateChunksCount();


int heap_setup(void);

size_t heap_get_used_space(void);
size_t heap_get_largest_used_block_size(void);
uint64_t heap_get_used_blocks_count(void);
size_t heap_get_free_space(void);
size_t heap_get_largest_free_area(void);
uint64_t heap_get_free_gaps_count(void);


void *heap_malloc(size_t count);
void *heap_calloc(size_t number, size_t size);
void *heap_realloc(void* memblock, size_t size);
void  heap_free(void* memblock);
void  heap_free_nts(void* memblock);

void* heap_malloc_nts_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_nts_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_nts_debug(void* memblock, size_t size, int fileline, const char* filename);
void* heap_calloc_aligned_nts_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_malloc_aligned_nts_debug(size_t count, int fileline, const char* filename);
void* heap_realloc_aligned_nts_debug(void* memblock, size_t size, int fileline, const char* filename);


void* heap_malloc_aligned(size_t count);
void* heap_realloc_aligned(void* memblock, size_t size);
void* heap_calloc_aligned(size_t number, size_t size);


enum pointer_type_t get_pointer_type(const void* pointer);
void* heap_get_data_block_start(const void* pointer);
size_t heap_get_block_size(const void* memblock);
int heap_validate(void);
void heap_dump_debug_information(void);

void* heap_malloc_ts_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_ts_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_ts_debug(void* memblock, size_t size, int fileline, const char* filename);
void* heap_calloc_aligned_ts_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_malloc_aligned_ts_debug(size_t count, int fileline, const char* filename);
void* heap_realloc_aligned_ts_debug(void* memblock, size_t size, int fileline, const char* filename);



#endif //MYHEAP_HEAP_H
