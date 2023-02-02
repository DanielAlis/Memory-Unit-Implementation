//
// Created by Alis Daniel.
//

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#define MAX_SIZE_ALLOC 100000000

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata *next;
    MallocMetadata *prev;
};

MallocMetadata* smallBlocksList = nullptr;
MallocMetadata* earliestFreeBlock(MallocMetadata* head,size_t size)
{
    if(head == nullptr)
    {
        return nullptr;
    }
    MallocMetadata* iter = head;
    MallocMetadata* to_return = nullptr;
    while(iter->next!= nullptr)
    {
        if(iter->is_free && size <= iter->size)
        {

            to_return = iter;
            break;
        }
        iter = iter->next;
    }
    if(to_return != nullptr)
    {
        return to_return;
    }
    else{
        return iter;
    }

}

void* smalloc (size_t size){
    if(size == 0 || size > MAX_SIZE_ALLOC)
    {
        return nullptr;
    }
    MallocMetadata* iter = earliestFreeBlock(smallBlocksList, size);
    //use free allocate
    if(iter != nullptr && (iter->is_free && size <= iter->size)){
        iter->is_free = false;
        return (void*)((char*)(iter) + sizeof(MallocMetadata));
    }
        //make new allocate
    else{
        void* ptr = sbrk(0);
        if(sbrk(size + sizeof(MallocMetadata)) == (void*)-1)
        {
            return nullptr;
        }
        MallocMetadata* meta;
        meta = (MallocMetadata*)ptr;
        meta->size = size;
        meta->is_free = false;
        meta->next = nullptr;
        if(iter!= nullptr)
        {
            iter->next = meta;
            meta->prev = iter;
        }
        else{
            smallBlocksList = meta;
            meta->prev = nullptr;
        }
        return (void*)((char*)(meta) + sizeof(MallocMetadata));
    }
}

void *scalloc(size_t num, size_t size) {
    void* ptr = smalloc(num*size);
    if(ptr == nullptr)
    {
        return nullptr;
    }
    memset(ptr,0,num*size);
    return ptr;
}

void sfree(void *p) {
    if(p == nullptr)
    {
        return;
    }
    MallocMetadata* meta =(MallocMetadata*)((char*)p - sizeof (MallocMetadata));
    meta->is_free = true;
}


void *srealloc(void *oldp, size_t size) {
    if(size == 0 || size > MAX_SIZE_ALLOC)
    {
        return nullptr;
    }
    if(oldp == nullptr)
    {
        return smalloc(size);
    }
    MallocMetadata* meta =(MallocMetadata*)((char*)oldp - sizeof (MallocMetadata));
    if(size <= meta->size){
        return oldp;
    }
    void* ptr = smalloc(size);
    if(ptr == nullptr)
    {
        return nullptr;
    }
    memmove(ptr,oldp,meta->size);
    sfree((void*)((char*)(meta) + sizeof (MallocMetadata)));
    return ptr;
}

size_t _num_free_blocks() {
    size_t curr_size = 0;
    if(smallBlocksList == nullptr)
    {
        return curr_size;
    }
    MallocMetadata* iter = smallBlocksList;
    while(iter != nullptr)
    {
        if(iter->is_free)
        {
            curr_size++;
        }
        iter = iter->next;
    }

    return curr_size;
}

size_t _num_free_bytes() {
    size_t curr_size = 0;
    if(smallBlocksList == nullptr)
    {
        return curr_size;
    }
    MallocMetadata* iter = smallBlocksList;
    while(iter != nullptr)
    {
        if(iter->is_free)
        {
            curr_size+=iter->size;
        }
        iter = iter->next;
    }
    return curr_size;
}

size_t _num_allocated_blocks() {
    size_t curr_size = 0;
    if(smallBlocksList == nullptr)
    {
        return curr_size;
    }
    MallocMetadata* iter = smallBlocksList;
    while(iter != nullptr)
    {
        curr_size++;
        iter = iter->next;
    }
    return curr_size;
}

size_t _num_allocated_bytes() {
    size_t curr_size = 0;
    if(smallBlocksList == nullptr)
    {
        return curr_size;
    }
    MallocMetadata* iter = smallBlocksList;
    while(iter != nullptr)
    {
        curr_size += iter->size;
        iter = iter->next;
    }
    return curr_size;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * sizeof(MallocMetadata);
}

size_t _size_meta_data() {
    return sizeof(MallocMetadata);
}