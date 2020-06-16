#include "heap.h"
#include "custom_unistd.h"
#include <stdio.h>
#include <pthread.h>
Heap heap;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


void ConsoleLog(char* function, char* log)
{
    printf("%s : %s\n", function, log);
}

int heap_setup(void)
{
    if(heap.isInitialized){
        ConsoleLog(__f, "Heap exists");
        return 0;
    }
    void* space = custom_sbrk(PAGE_SIZE);
    if(space == (void*)-1) {
        ConsoleLog(__f, "Not enough memory for heap");
        return -1;
    }
    setBoundaries(space);
    heap.head = heap.boundaries.leftBound;
    heap.tail = heap.boundaries.rightBound;
    heap.chunksCount.free = 1;
    heap.chunksCount.used = 2; // boundaries
    heap.isInitialized = true;
    heap.firstFence = heap.secondFence = RANDOM_FENCE_VALUE;
    heapSetSum();
    return 0;
}
void setBoundaries(void* space)
{
    heap.boundaries.leftBound = (Chunk*)space;
    Chunk* freeChunk = heap.boundaries.leftBound + 1;
    heap.boundaries.rightBound = (Chunk*)((uchar*)space + PAGE_SIZE) - 1;
    heap.boundaries.leftBound->prev = heap.boundaries.rightBound->next = NULL;
    heap.boundaries.rightBound->prev = heap.boundaries.leftBound->next = freeChunk;
    heap.boundaries.leftBound->size = heap.boundaries.rightBound->size = EMPTY;
    heap.boundaries.leftBound->isFree = heap.boundaries.rightBound->isFree =  false;
    // setFirstFreeBlock
    freeChunk->next = heap.boundaries.rightBound;
    freeChunk->prev = heap.boundaries.leftBound;
    freeChunk->isFree = true;
    freeChunk->size = PAGE_SIZE - 3 * sizeof(Chunk);
    setFences(3, heap.boundaries.leftBound, heap.boundaries.rightBound, freeChunk);
    setSum(3, heap.boundaries.leftBound, heap.boundaries.rightBound, freeChunk);
}
void setFences(int countOfChunks, ...)
{
    va_list list;
    va_start(list, countOfChunks);
    for(unsigned int i=0; i<countOfChunks; ++i)
    {
        Chunk* chunk = va_arg(list, Chunk*);
        chunk->firstFence = RANDOM_FENCE_VALUE;
        chunk->secondFence = RANDOM_FENCE_VALUE;
    }
    va_end(list);
}
void setSum(int countOfChunks, ...)
{
    va_list list;
    va_start(list, countOfChunks);
    int32_t sum;
    for(unsigned int i=0; i<countOfChunks; ++i)
    {
        Chunk* chunk = va_arg(list, Chunk*);
        sum = chunk->sumOfBytes = 0;
        for(uchar* start = (uchar*)chunk; start != (uchar*)(chunk+1); ++start)
            sum += *start;
        chunk->sumOfBytes = sum;
    }
    va_end(list);
}
void heapSetSum()
{
    uchar* end = (uchar*)((Heap*)&heap + 1);
    int32_t sum;
    sum = heap.sumOfBytes = 0;
    for(uchar* start = (uchar*)&heap; start != end; ++start)
        sum += *start;
    heap.sumOfBytes = sum;
}

size_t ceilWord(size_t amount)
{
    if (amount % sizeof(void*) == 0)
        return amount;
    else
        return amount - (amount % sizeof(void*)) + sizeof(void*);
}
bool chunkExists(Chunk* chunk)

{
    if(chunk == NULL)
        return false;
    if (heap.isInitialized == false)
        return false;
    Chunk* temp = heap.head->next;
    while(temp != heap.tail)
    {
        if(temp == chunk)
            return true;
        temp = temp->next;
    }
    return false;
}
int getSpace(intptr_t countOfPages)
{
    // Overflow
    if (INTPTR_MAX / PAGE_SIZE < countOfPages)
        return -1;
    intptr_t size = countOfPages * PAGE_SIZE;
    void* space = custom_sbrk(size);
    if(space == (void*)-1)
        return -1;
    Chunk* oldTail = heap.tail;
    Chunk* newBoundary = (Chunk*)((uchar*)space+size-sizeof(Chunk));
    newBoundary->size = 0;
    newBoundary->isFree = false;
    newBoundary->prev = oldTail;
    newBoundary->next = NULL;
    oldTail->next = newBoundary;
    oldTail->size = size - sizeof(Chunk);
    oldTail->isFree = true;
    setFences(1, newBoundary);
    setSum(2, oldTail, newBoundary);
    heap.boundaries.rightBound = newBoundary;
    heap.tail = newBoundary;
    if(heap.tail->prev->prev->isFree){
        mergeChunks(heap.tail->prev->prev, heap.tail->prev);
    }
    updateChunksCount();
    heapSetSum();
    return 1;
}
void splitChunk(Chunk* firstChunk, size_t count)
{
    Chunk* secondChunk = (Chunk*)((uchar*)firstChunk + sizeof(Chunk) + count);
    secondChunk->size = firstChunk->size - count - sizeof(Chunk);
    secondChunk->isFree = true;
    secondChunk->prev = firstChunk;
    secondChunk->next = firstChunk->next;
    firstChunk->next->prev = secondChunk;
    firstChunk->next = secondChunk;
    firstChunk->size = count;
    // Update second Chunk
    secondChunk->debugParams.fileName = NULL;
    setFences(1,secondChunk);
    setSum(2, firstChunk, firstChunk->next);
    if(firstChunk->next->next != NULL)
        setSum(1, firstChunk->next->next);
}
void mergeChunks(Chunk* firstChunk, Chunk* secondChunk)
{
    // Merged chunk is Free
    secondChunk->next->prev = firstChunk;
    firstChunk->next = secondChunk->next;
    firstChunk->size += secondChunk->size + sizeof(Chunk);
    firstChunk->isFree = true;
    setSum(1, firstChunk);
}
void updateChunksCount()
{
    uint32_t before = heap.chunksCount.free + heap.chunksCount.used;
    heap.chunksCount.free = 0;
    heap.chunksCount.used = 2;
    Chunk* current = heap.head->next;
    while(current != heap.tail)
    {
        if(current->isFree)
            heap.chunksCount.free += 1;
        else
            heap.chunksCount.used += 1;
        current = current->next;
    }
    uint32_t after = heap.chunksCount.used + heap.chunksCount.free;
    heap.sumOfBytes += (after - before);
    return;
}

void* heap_malloc_nts_debug(size_t count, int fileline, const char* filename)
{
    if(!heap.isInitialized){
        ConsoleLog(__f, "Heap isn't initialized");
        return NULL;
    }
    if(!count)
    {
        ConsoleLog(__f, "Invalid count");
        return NULL;
    }
    size_t allocateSize = ceilWord(count);
    Chunk* temp = heap.head->next;
    while(temp != heap.tail)
    {
        if(temp->isFree && temp->size >= allocateSize)
        {
            //if(temp->size - allocateSize <= sizeof(Chunk))
            if(temp->size - allocateSize > sizeof(Chunk))
                splitChunk(temp, allocateSize);
            temp->debugParams.fileName = filename;
            temp->debugParams.lineNumber = fileline;
            temp->isFree = false;
            setSum(1, temp);
            updateChunksCount();
            return temp+1;
        }
        temp = temp->next;
    }
    int32_t currentSize = 0;
    Chunk *previousBlock = heap.tail->prev;
    if(previousBlock->isFree)
        currentSize += previousBlock->size;
    size_t need_bytes = count - currentSize + sizeof(Chunk);
    intptr_t need_pages = need_bytes / PAGE_SIZE;
    if(need_bytes % PAGE_SIZE != 0) need_pages++;
    int err = getSpace(need_pages);
    if(err == -1) {
        ConsoleLog(__f, "Couldn't get enough space from OS");
        return NULL;
    }
    Chunk* chunkForReturn;
    if(heap.tail->prev->prev->isFree)
        mergeChunks(heap.tail->prev->prev, heap.tail->prev);
    if(allocateSize - heap.tail->prev->size > sizeof(Chunk)) {
        splitChunk(heap.tail->prev, allocateSize);
        heap.tail->prev->prev->debugParams.fileName = filename;
        heap.tail->prev->prev->debugParams.lineNumber = fileline;
        heap.tail->prev->prev->isFree = false;
        setSum(3, heap.tail->prev->prev, heap.tail, heap.tail->prev);
        chunkForReturn = heap.tail->prev->prev;
    } else
    {
        heap.tail->prev->debugParams.fileName = filename;
        heap.tail->prev->debugParams.lineNumber = fileline;
        heap.tail->prev->isFree = false;
        setSum(2, heap.tail->prev, heap.tail);
        chunkForReturn = heap.tail->prev;
    }
    updateChunksCount();
    return chunkForReturn+1;
}
void* heap_calloc_nts_debug(size_t number, size_t size, int fileline, const char* filename)
{
    if(SIZE_MAX / size < number){
        ConsoleLog(__f, "Overflow");
        return NULL;
    }
    void* start = heap_malloc_nts_debug(size * number, fileline, filename);
    if(start == NULL){
        ConsoleLog(__f, "Couldn't allocate memory");
        return NULL;
    }
    memset(start, 0, size*number);
    return start;
}
void* heap_realloc_nts_debug(void* memblock, size_t size, int fileline, const char* filename)
{
    if(heap.isInitialized == false) {
        ConsoleLog(__f, "Heap doesn't exists");
        return NULL;
    }
    if(!memblock)
        return heap_malloc_nts_debug(size, fileline, filename);
    if(size == 0){
        heap_free_nts(memblock);
        return NULL;
    }
    Chunk* current = (Chunk*)((uchar*)memblock - sizeof(Chunk));
    size_t amount = ceilWord(size);
    size_t sizeWithNext = current->size + sizeof(Chunk) + current->next->size;
    intptr_t left = (intptr_t)sizeWithNext-(intptr_t)amount;
    if(current->size == amount)
        {
            current->debugParams.lineNumber = fileline;
            current->debugParams.fileName = filename;
            setSum(1, current);
            return memblock;
        }
    else if(current->size < amount)
    {
        if(current->next->isFree && left==0)
        {
            mergeChunks(current, current->next);
            current->isFree = false;
            current->debugParams.lineNumber = fileline;
            current->debugParams.fileName = filename;
            setSum(1, current);
            updateChunksCount();
            return memblock;
        }
        else if(current->next->isFree && left < 0)
        {
            void* newChunk = heap_malloc_nts_debug(amount, fileline, filename);
            if(newChunk == NULL)
                return NULL;
            memcpy(newChunk, memblock, current->size);
            Chunk* temp = (Chunk*)((uchar*)newChunk - sizeof(Chunk));
            temp->isFree = false;
            temp->debugParams.lineNumber = fileline;
            temp->debugParams.fileName = filename;
            heap_free_nts(memblock);
            setSum(1, temp);
            return newChunk;
        }
        else if(current->next->isFree && left > 0)
        {
            mergeChunks(current, current->next);
            if(left > sizeof(Chunk)){
                splitChunk(current, amount);
            }
            current->isFree = false;
            current->debugParams.lineNumber = fileline;
            current->debugParams.fileName = filename;
            // DELETE
            current->next->debugParams.fileName = NULL;
            setSum(2, current, current->next);
            updateChunksCount();
            return memblock;
        }
        else if(current->next->isFree == false)
        {
            void* newChunk = heap_malloc_nts_debug(amount, fileline, filename);
            if(newChunk == NULL)
            {
                ConsoleLog(__f, "Malloc couldn't allocate memory");
                return NULL;
            }
            memcpy(newChunk, memblock, current->size);
            Chunk* temp = (Chunk*)((uchar*)newChunk - sizeof(Chunk));
            temp->isFree = false;
            temp->debugParams.lineNumber = fileline;
            temp->debugParams.fileName = filename;
            setSum(1, temp);
            heap_free_nts(memblock);
            return newChunk;
        }
    }
    else
    {
        if(current->size - amount <= sizeof(Chunk))
        {
            if(current->next->isFree) {
                int32_t newSize = current->size - amount + current->next->size;
                current->size = amount;
                Chunk* newChunk = (uchar*)current + sizeof(Chunk) + current->size;
                newChunk->next = current->next->next;
                newChunk->next->prev = newChunk;
                newChunk->prev = current;
                newChunk->size = newSize;
                current->next = newChunk;
                newChunk->isFree = true;
                newChunk->debugParams.fileName = NULL;
                current->debugParams.lineNumber = fileline;
                current->debugParams.fileName = filename;
                setFences(1, newChunk);
                setSum(3, current, newChunk, newChunk->next);
                updateChunksCount();
            }
            return memblock;
        } else
        {
            splitChunk(current, amount);
            current->isFree = false;
            if(current->next->next->isFree){
                mergeChunks(current->next, current->next->next);
            }
            current->debugParams.lineNumber = fileline;
            current->debugParams.fileName = filename;
            setSum(1, current);
            updateChunksCount();
            return memblock;
        }

    }
    current->debugParams.lineNumber = fileline;
    current->debugParams.fileName = filename;
    setSum(1, current);
    return memblock;
}
void heap_free_nts(void* memblock)
{
    bool exists = chunkExists((Chunk*)((uchar*)memblock-sizeof(Chunk)));
    if(!exists){
        ConsoleLog(__f, "Invalid chunk <not exists>");
        return;
    }
    Chunk* chunk = (Chunk*)((uchar*)memblock-sizeof(Chunk));
    Chunk* temp = chunk;
    if(chunk->isFree == true)
    {
        ConsoleLog(__f, "Double free deteched");
        return;
    }
    if(chunk->next->isFree){
        mergeChunks(chunk, chunk->next);
        setSum(2, chunk, chunk->next);
    }
    if(chunk->prev->isFree){
        temp = chunk->prev;
        mergeChunks(chunk->prev, chunk);
        setSum(2, temp, temp->next);
    }
    if(!temp->isFree){
        temp->isFree = true;
    }
    setSum(1, chunk);
    updateChunksCount();
}

Chunk* findAligned(size_t size)
{
    if(heap.isInitialized==false)
    {
        ConsoleLog(__f, "Heap isn't initialized");
        return NULL;
    }

    Chunk* current = heap.head->next;

    while(current != heap.tail)
    {
        if(current->isFree == true) {
            intptr_t dataStart = (uchar *) current + sizeof(Chunk);
            if (dataStart % PAGE_SIZE == 0 && size <= current->size) {
                if (size == current->size || current->size - size <= sizeof(Chunk)){
                    current->isFree = false;
                    updateChunksCount();
                    return current;
                }
                else
                {
                    splitChunk(current, size);

                    current->isFree = false;
                    updateChunksCount();
                    return current;
                }
            }
            else if (size <= current->size) {
                intptr_t memoryStart = alignedMemory(dataStart, dataStart + current->size);
                if (memoryStart == 0)
                {
                    current = current->next;
                    continue;
                }
                else if (memoryStart + size + sizeof(Chunk) < dataStart + current->size)
                {
                    splitChunk(current, memoryStart - dataStart - sizeof(Chunk));
                    current->next->isFree = false;
                    if(current->next->size - size > sizeof(Chunk)) {
                        splitChunk(current->next, size);
                        updateChunksCount();
                    }
                    current->next->isFree = false;
                    setSum(1, current->next);
                    updateChunksCount();
                    return current->next;
                }
                else
                {
                    current = current->next;
                    continue;
                }
            }
        }
        current = current->next;
    }
    return NULL;
}
intptr_t alignedMemory(intptr_t start, intptr_t end)
{
    intptr_t memory = start - start % PAGE_SIZE + PAGE_SIZE;
    if(memory >= start && memory < end)
        return memory;
    return 0;
}
void* heap_malloc_aligned_nts_debug(size_t count, int fileline, const char* filename)
{
    if(!heap.isInitialized){
        ConsoleLog(__f, "Heap isn't initialized");
        return NULL;
    }
    if(!count)
    {
        ConsoleLog(__f, "Invalid count");
        return NULL;
    }
    size_t allocateSize = ceilWord(count);
    Chunk* chunk = findAligned(allocateSize);
    intptr_t pagesToReturn = 0;
    if(chunk == NULL)
    {
        while(true)
        {
            int err = getSpace(1);
            if(err != 1)
            {
                ConsoleLog(__f, "custom_sbrk failed");
                heap.tail->prev->size -= pagesToReturn*PAGE_SIZE;
                Chunk* temp = heap.tail->prev;
                heap.tail = heap.boundaries.rightBound = (Chunk*)(heap.tail->prev->size + (uchar*)heap.tail->prev + sizeof(chunk));
                heap.tail->prev = temp;
                heap.tail->next = NULL;
                temp->next = heap.tail;
                custom_sbrk(-pagesToReturn*PAGE_SIZE);
                return NULL;
            }
            pagesToReturn++;
            chunk = findAligned(allocateSize);
            if(chunk != NULL)
            {
                setSum(1, heap.tail);
                break;
            }
        }
    }
    setFences(1, chunk);
    chunk->debugParams.lineNumber = fileline;
    chunk->debugParams.fileName = filename;
    setSum(1, chunk);
    updateChunksCount();
    return (uchar*)chunk + sizeof(Chunk);
}
void* heap_realloc_aligned_nts_debug(void* memblock, size_t size, int fileline, const char* filename)
{
    if(heap.isInitialized == false) {
        ConsoleLog(__f, "Heap doesn't exists");
        return NULL;
    }
    if(memblock == NULL)
        return heap_malloc_aligned_nts_debug(size, fileline, filename);
    if(size == 0)
        return heap_free_nts(memblock), NULL;
    Chunk* current = (Chunk*)((uchar*)memblock - sizeof(Chunk));
     if((intptr_t)memblock % PAGE_SIZE != 0)
    {
        void* newChunk = heap_malloc_aligned_nts_debug(size, fileline, filename);
        if(newChunk == NULL)
        {
            ConsoleLog(__f, "Malloc failed");
            return NULL;
        }
        Chunk* temp = (Chunk*)((uchar*)newChunk - sizeof(Chunk));
        memcpy(newChunk, memblock, temp->size);
        temp->isFree = false;
        heap_free_nts(memblock);
        setSum(1, temp);
        return newChunk;
    }
    size_t amount = ceilWord(size);
    size_t sizeWithNext = current->size + sizeof(Chunk) + current->next->size;
    intptr_t left = (intptr_t)sizeWithNext-(intptr_t)amount;
    if(current->size == amount){
        current->debugParams.lineNumber =fileline;
        current->debugParams.fileName = filename;
        setSum(1, current);
        return memblock;
    }
    else if(current->size < amount)
    {
        if(current->next->isFree && left==0)
        {
            mergeChunks(current, current->next);
            current->isFree = false;
            // dodane
            current->debugParams.lineNumber =fileline;
            current->debugParams.fileName = filename;
            setSum(1, current);
            updateChunksCount();
            return memblock;
        }
        else if(current->next->isFree && left < 0)
        {
            // wprowadzam ;eksperymenty
            void* newChunk = heap_malloc_aligned_nts_debug(amount, fileline, filename);
            if(newChunk == NULL)
            {
                ConsoleLog(__f, "Malloc couldn't allocate memory");
                return NULL;
            }
            memcpy(newChunk, memblock, current->size);
            Chunk* temp = (Chunk*)((uchar*)newChunk - sizeof(Chunk));
            temp->isFree = false;
            heap_free_nts(memblock);
            return newChunk;
        }
        else if(current->next->isFree && left > 0)
        {
            mergeChunks(current, current->next);
            if(left > sizeof(Chunk)){
                splitChunk(current, amount);
            }
            current->isFree = false;
            updateChunksCount();
            current->debugParams.fileName = filename;
            current->debugParams.lineNumber = fileline;
            setSum(1, current);
            return memblock;
        }
        else if(current->next->isFree == false)
        {
            void* newChunk = heap_malloc_aligned_nts_debug(amount, fileline, filename);
            if(newChunk == NULL)
            {
                ConsoleLog(__f, "Malloc couldn't allocate memory");
                return NULL;
            }
            memcpy(newChunk, memblock, current->size);
            Chunk* temp = (Chunk*)((uchar*)newChunk - sizeof(Chunk));
            temp->isFree = false;
            temp->debugParams.fileName = filename;
            temp->debugParams.lineNumber = fileline;
            setSum(1, temp);
            heap_free_nts(memblock);
            return newChunk;
        }
    }
    else
    {
        if(current->size - amount <= sizeof(Chunk))
        {
            if(current->next->isFree) {
                size_t newBlockSize = current->size - amount + current->next->size;
                Chunk *newBlock = (uchar *) current + sizeof(Chunk) + amount;
                current->size = amount;
                newBlock->next = current->next->next;
                newBlock->prev = current;
                newBlock->size = newBlockSize;
                newBlock->isFree = true;
                newBlock->debugParams.fileName = NULL;
                current->next = newBlock;
                setSum(2, current, current->next);
                setFences(1, current->next);
            }
            current->debugParams.lineNumber =fileline;
            current->debugParams.fileName = filename;
            setSum(1, current);
            return memblock;
        } else
        {
            splitChunk(current, amount);
            current->isFree = false;
            if(current->next->next->isFree){
                mergeChunks(current->next, current->next->next);
            }
            updateChunksCount();
            current->debugParams.lineNumber =fileline;
            current->debugParams.fileName = filename;
            setSum(1, current);
            return memblock;
        }

    }
    return memblock;
}
void* heap_calloc_aligned_nts_debug(size_t number, size_t size, int fileline, const char* filename)
{
    if(SIZE_MAX / size < number){
        ConsoleLog(__f, "Overflow");
        return NULL;
    }
    void* start = heap_malloc_aligned_nts_debug(size * number, fileline, filename);
    if(start == NULL){
        ConsoleLog(__f, "Couldn't allocate memory");
        return NULL;
    }
    memset(start, 0, size*number);
    return start;
}

size_t heap_get_used_space(void) {
    pthread_mutex_lock(&mutex);
    if(heap.isInitialized == false)
    {
        pthread_mutex_unlock(&mutex);
        return 0;
    }
    Chunk* current = heap.head->next;
    size_t space = 0;
    while(current != heap.tail)
    {
        if(current->isFree == false)
            space+=current->size;
        space += sizeof(Chunk);
        current = current->next;
    }
    space += 2 * sizeof(Chunk);
    return pthread_mutex_unlock(&mutex), space;
}
size_t heap_get_largest_used_block_size(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.isInitialized == false)
        return pthread_mutex_unlock(&mutex), 0;
    if(heap.chunksCount.used == 2)
        return 0;
    size_t max = 0;
    for(Chunk* current = heap.head->next; current != heap.tail; current=current->next)
    {
        if(current->isFree == false && current->size > max)
            max = current->size;
    }
    return pthread_mutex_unlock(&mutex), max;
}
uint64_t heap_get_used_blocks_count(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.isInitialized == false)
        return pthread_mutex_unlock(&mutex), 0;
    return pthread_mutex_unlock(&mutex), heap.chunksCount.used;
}
size_t heap_get_free_space(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.isInitialized == false)
        return pthread_mutex_unlock(&mutex), 0;
    size_t size = 0;
    for(Chunk* current = heap.head->next; current != heap.tail; current=current->next)
    {
        if(current->isFree)
            size += current->size;
    }
    return pthread_mutex_unlock(&mutex), size;
}
size_t heap_get_largest_free_area(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.isInitialized == false)
        return pthread_mutex_unlock(&mutex), 0;
    size_t max = 0;
    for(Chunk* current = heap.head->next; current != heap.tail; current=current->next)
    {
        if(current->isFree == true && current->size > max)
            max = current->size;
    }
    return pthread_mutex_unlock(&mutex), max;
}
uint64_t heap_get_free_gaps_count(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.isInitialized == false)
        return pthread_mutex_unlock(&mutex), 0;
    return pthread_mutex_unlock(&mutex), heap.chunksCount.free;
}


enum pointer_type_t get_pointer_type(const void* pointer)
{
    pthread_mutex_lock(&mutex);
    if(pointer == NULL)
        return pthread_mutex_unlock(&mutex), pointer_null;
    if(heap.isInitialized == false)
        return pthread_mutex_unlock(&mutex), pointer_out_of_heap;
    intptr_t ptr = (intptr_t)pointer;
    if(ptr < (intptr_t)heap.head || ptr > heap.tail)
        return pthread_mutex_unlock(&mutex), pointer_out_of_heap;
    for(Chunk* temp = heap.head; temp != NULL; temp = temp->next)
    {
        if(ptr - sizeof(Chunk) == temp)
            return pthread_mutex_unlock(&mutex), (temp->isFree == true) ? pointer_unallocated : pointer_valid;
        if(ptr >= (intptr_t)temp && ptr < (intptr_t)(temp+1))
            return pthread_mutex_unlock(&mutex), pointer_control_block;
        if(temp->isFree && ptr >= (intptr_t)(temp+1) && ptr < (intptr_t)((uchar*)temp+temp->size+sizeof(Chunk)))
            return pthread_mutex_unlock(&mutex), pointer_unallocated;
        if(!temp->isFree && ptr >= (intptr_t)(temp+1) && ptr < (intptr_t)((uchar*)temp+temp->size+sizeof(Chunk)))
            return pthread_mutex_unlock(&mutex), pointer_inside_data_block;
    }
    return pthread_mutex_unlock(&mutex), pointer_out_of_heap;
}

size_t heap_get_block_size(const void* memblock)
{
    pthread_mutex_lock(&mutex);
    if(heap.isInitialized == false || memblock == NULL)
        return pthread_mutex_unlock(&mutex), 0;
    enum pointer_type_t ptr = get_pointer_type(memblock);
    Chunk* temp = (Chunk*)((uchar*)memblock-sizeof(Chunk));
    return pthread_mutex_unlock(&mutex), (ptr==pointer_valid) ? temp->size : 0;
}

void* heap_get_data_block_start(const void* pointer)
{
    pthread_mutex_lock(&mutex);
    if(pointer == NULL || heap.isInitialized == false)
        return pthread_mutex_unlock(&mutex), NULL;
    enum pointer_type_t ptr = get_pointer_type(pointer);
    if(ptr == pointer_valid)
        return pthread_mutex_unlock(&mutex), pointer;
    if(ptr != pointer_inside_data_block)
        return pthread_mutex_unlock(&mutex), NULL;
    intptr_t memory = (intptr_t)pointer;
    for(Chunk* temp = heap.head->next; temp != heap.tail; temp = temp->next)
    {
        if(memory >= (intptr_t)(temp+1) && memory < (intptr_t)((uchar*)temp+temp->size+sizeof(Chunk)))
            return pthread_mutex_unlock(&mutex), temp+1;
    }
    return pthread_mutex_unlock(&mutex), NULL;
}

int heap_validate(void)
{
    pthread_mutex_lock(&mutex);
    // HEAP ISN'T INITIALIZED
    if(heap.isInitialized == false)
        return pthread_mutex_unlock(&mutex), -1;
    // MISSING GUARDS
    if(heap.chunksCount.used < 2){
        pthread_mutex_unlock(&mutex);
        return ConsoleLog(__f, "Missing guards in heap"), -1;
    }
    // BOUNDARIES DON'T EQUAL TAIL AND HEAD
    if(heap.boundaries.leftBound != heap.head || heap.boundaries.rightBound != heap.tail)
    {
        pthread_mutex_unlock(&mutex);
        return ConsoleLog(__f, "head != boundary.left || tail != boundary.right"), -1;
    }
    // INVALID FENCES
    if(heap.firstFence != RANDOM_FENCE_VALUE || heap.secondFence != RANDOM_FENCE_VALUE)
    {
        pthread_mutex_unlock(&mutex);
        return ConsoleLog(__f, "heap.fences != RANDOM_FENCE_VALUE"), -1;
    }
    // HEAPSUM INVALID
    int32_t sum = heap.sumOfBytes;
    heapSetSum();
    if(sum != heap.sumOfBytes){
        pthread_mutex_unlock(&mutex);
        return ConsoleLog(__f, "Control sum is invalid"), -1;
    }
    if(heap.boundaries.leftBound != heap.head || heap.boundaries.rightBound != heap.tail)
    {
        pthread_mutex_unlock(&mutex);
        return ConsoleLog(__f, "Boundaries are damaged or badly set <boundaries != head&tail>"), -1;
    }

    // INVALID CHUNKS
    int blockID = 0;
    for(Chunk* current = heap.head; current != NULL; current = current->next, blockID++)
    {
        //PROBLEMS WITH TAIL AND HEAD
        if(current == heap.tail && current->next != NULL)
        {
            pthread_mutex_unlock(&mutex);
            return ConsoleLog(__f, "tail->next != NULL"), -1;
        }
        if(current == heap.head && current->prev != NULL)
        {
            pthread_mutex_unlock(&mutex);
            return ConsoleLog(__f, "head->prev != NULL"), -1;
        }
        if(current != heap.tail && current->next == NULL)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i]->next == NULL\n", __f,  blockID, blockID), -1;
        }
        if(current != heap.head && current->prev == NULL)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i]->prev == NULL\n", __f,  blockID, blockID), -1;
        }
        if(current != heap.tail && current->next->prev != current)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i]->next->prev != Block[%i]\n", __f,  blockID, blockID), -1;
        }
        if(current != heap.head && current->prev->next != current)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i]->prev->next != Block[%i]\n", __f,  blockID, blockID), -1;
        }
        if((intptr_t)current % sizeof(void*) != 0)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i] has invalid address <address mod word>\n", __f, blockID), -1;
        }
        if(current != heap.tail && (intptr_t)current->next % sizeof(void*) != 0)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i] has invalid address <address mod word>\n", __f, blockID+1), -1;
        }
        if(current != heap.head && (intptr_t)current->prev % sizeof(void*) != 0)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i] has invalid address <address mod word>\n", __f, blockID-1), -1;
        }
        // INVALID SIZE
        int32_t size = current->size;
        int32_t check = current->sumOfBytes;
        setSum(1, current);
        if(size != current->size)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i] has invalid size\n", __f, blockID), -1;
        }
        // INVALID FENCES VALUE
        if(current->firstFence != RANDOM_FENCE_VALUE || current->secondFence != RANDOM_FENCE_VALUE)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i] has invalid fences value\n", __f, blockID), -1;
        }
        // INVALID SIZE
        if(current->size % sizeof(void*) != 0)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i] has invalid size <size mod sizeof(void*)>\n", __f, blockID), -1;
        }
        // INVALID CONTROL SUM
        if(check != current->sumOfBytes)
        {
            pthread_mutex_unlock(&mutex);
            return printf("%s : Block[%i] has invalid control sum\n", __f, blockID), -1;
        }
    }
    pthread_mutex_unlock(&mutex);
    ConsoleLog(__f, "Heap is valid!");
    return 0;
}

void heap_dump_debug_information(void)
{

    if(heap.isInitialized == false){
        ConsoleLog(__f, "Heap doesn't exist");
        return;
    }
    printf("\n\n\t\t\t\t\t\tHEAP INFORMATIONS\n");
    printf("INDEX\t\tADDRESS\t\t\t\tSIZE\tFREE\tFILENAME\tFILELINE\n");
    Chunk* current = heap.head;
    for(int i=0; current; ++i)
    {
        printf("%5i", i);
        printf("\t\t%p", current);
        printf("\t\t%4li", current->size);
        printf("\t%4s", current->isFree ? "YES" : "NO");
        if(current->debugParams.fileName && current != heap.head && current != heap.tail)
            printf("\t%8s\t%8i\n", current->debugParams.fileName, current->debugParams.lineNumber);
        else
            printf("\t--------\t--------\n");
        current = current->next;
    }
    printf("\n\t\t\t\t\tHEAP INFORMATIONS\n");
    printf("Used chunks: %i\n", heap.chunksCount.used);
    printf("Free chunks: %i\n", heap.chunksCount.free);
    printf("Fences value: %i\n", heap.secondFence);
    printf("Biggest free chunk size: %i\n", heap_get_largest_free_area());
    printf("Used space size: %li\n", heap_get_used_space());
    printf("Free space size: %li\n", heap_get_free_space());
    printf("Heap size: %li\n", heap_get_used_space() + heap_get_free_space());
    return ;
}


void* heap_malloc_ts_debug(size_t count, int fileline, const char* filename)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_malloc_nts_debug(count, fileline, filename);
    pthread_mutex_unlock(&mutex);
    return memory;
}
void* heap_calloc_ts_debug(size_t number, size_t size, int fileline, const char* filename)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_calloc_nts_debug(number, size, fileline, filename);
    pthread_mutex_unlock(&mutex);
    return memory;
}
void* heap_realloc_ts_debug(void* memblock, size_t size, int fileline, const char* filename)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_realloc_nts_debug(memblock, size, fileline, filename);
    pthread_mutex_unlock(&mutex);
    return memory;
}
void* heap_calloc_aligned_ts_debug(size_t number, size_t size, int fileline, const char* filename)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_calloc_aligned_nts_debug(number, size, fileline, filename);
    pthread_mutex_unlock(&mutex);
    return memory;
}
void* heap_malloc_aligned_ts_debug(size_t count, int fileline, const char* filename)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_malloc_aligned_nts_debug(count, fileline, filename);
    pthread_mutex_unlock(&mutex);
    return memory;
}
void* heap_realloc_aligned_ts_debug(void* memblock, size_t size, int fileline, const char* filename)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_realloc_aligned_nts_debug(memblock, size, fileline, filename);
    pthread_mutex_unlock(&mutex);
    return memory;
}

void *heap_malloc(size_t count)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_malloc_ts_debug(count, 0, NULL);
    pthread_mutex_unlock(&mutex);
    return memory;
}
void *heap_calloc(size_t number, size_t size)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_calloc_ts_debug(number, size, 0, NULL);
    pthread_mutex_unlock(&mutex);
    return memory;
}
void *heap_realloc(void* memblock, size_t size)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_realloc_ts_debug(memblock, size, 0, NULL);
    pthread_mutex_unlock(&mutex);
    return memory;
}

void heap_free(void* memblock)
{
     pthread_mutex_lock(&mutex);
     heap_free_nts(memblock);
     pthread_mutex_unlock(&mutex);
}


void* heap_malloc_aligned(size_t count)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_malloc_aligned_ts_debug(count, 0, NULL);
    pthread_mutex_unlock(&mutex);
    return memory;
}
void* heap_realloc_aligned(void* memblock, size_t size)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_realloc_aligned_ts_debug(memblock, size, 0, NULL);
    pthread_mutex_unlock(&mutex);
    return memory;
}

void* heap_calloc_aligned(size_t number, size_t size)
{
    pthread_mutex_lock(&mutex);
    void* memory = heap_calloc_aligned_ts_debug(number, size, 0, NULL);
    pthread_mutex_unlock(&mutex);
    return memory;
}
