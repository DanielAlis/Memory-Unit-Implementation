//
// Created by Alis Daniel.
//
#include <unistd.h>

#define MAX_SIZE_ALLOC 100000000

void* smalloc (size_t size){
    if(size == 0 || size > MAX_SIZE_ALLOC)
    {
        return nullptr;
    }
    void* result = sbrk(0);
    if(sbrk(size) == (void*)-1)
    {
        return nullptr;
    }
    return result;
}