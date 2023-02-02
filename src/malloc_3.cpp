//
// Created by Alis Daniel.
//


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <sys/mman.h>

#define MAX_SIZE_ALLOC 100000000
#define MMAPALLOC 131072
#define SPLIT_SIZE 128



struct MallocMetadata {
    int cookie;
    bool mmap_alloc;
    size_t size;
    bool is_free;
    MallocMetadata *next_free;
    MallocMetadata *prev_free;
};

size_t MetaAlignLen = sizeof(MallocMetadata);
MallocMetadata *freeBlocksList = nullptr;
MallocMetadata *wildernessBlock = nullptr;
static int randomCookies = rand();


size_t num_of_free_bytes = 0;
size_t num_of_free_blocks = 0;
size_t num_of_alloc_blocks = 0;
size_t num_of_alloc_bytes = 0;

void sfree(void *p);


MallocMetadata *earliestFreeBlock(MallocMetadata **head, size_t size) {
    if (*head == nullptr) {
        return nullptr;
    }
    MallocMetadata *iter = *head;
    MallocMetadata *to_return = nullptr;

    while (iter->next_free != nullptr) {
        if(iter->cookie != randomCookies){
            exit(0xdeadbeef);
        }
        if (size <= iter->size) {

            to_return = iter;
            break;
        }
        iter = iter->next_free;
    }
    if (to_return != nullptr) {
        return to_return;
    } else {
        return iter;
    }

}

MallocMetadata *listInsert(MallocMetadata **head, MallocMetadata *meta, size_t size) {
    meta->is_free = true;
    meta->mmap_alloc = false;
    meta->size = size;
    if (*head == nullptr) {
        meta->next_free = nullptr;
        meta->prev_free = nullptr;
        *head = meta;
        num_of_free_bytes += size;
        num_of_free_blocks++;
        return *head;
    }
    MallocMetadata *iter_curr = *head;
    MallocMetadata *iter_prev = nullptr;
    while (iter_curr != nullptr) {
        if(iter_curr->cookie != randomCookies){
            exit(0xdeadbeef);
        }
        if (iter_curr == meta) {
            return iter_curr;
        }
        if (meta->size <= iter_curr->size) {
            if (meta->size == iter_curr->size && iter_curr < meta) {
                iter_prev = iter_curr;
                iter_curr = iter_curr->next_free;
                continue;
            } else {
                break;
            }
        }
        iter_prev = iter_curr;
        iter_curr = iter_curr->next_free;
    }
    num_of_free_bytes += size;
    num_of_free_blocks++;
    if (iter_curr == nullptr) {
        iter_prev->next_free = meta;
        meta->prev_free = iter_prev;
        meta->next_free = nullptr;
        return meta;
    }
    meta->next_free = iter_curr;
    if (iter_prev == nullptr) {
        iter_curr->prev_free = meta;
        meta->prev_free = nullptr;
        *head = meta;
        return *head;
    } else {
        meta->prev_free= iter_prev;
        iter_curr->prev_free = meta;
        iter_prev->next_free = meta;
        return meta;
    }
}

void listRemove(MallocMetadata **head, MallocMetadata *to_remove) {
    MallocMetadata *iter = *head;
    while (iter != nullptr) {
        if(iter->cookie != randomCookies){
            exit(0xdeadbeef);
        }
        if (iter == to_remove) {
            MallocMetadata *prev = iter->prev_free;
            MallocMetadata *next = iter->next_free;
            if (prev == nullptr) {
                *head = next;
            } else {
                prev->next_free = next;
            }
            if (next != nullptr) {
                next->prev_free = prev;
            }
            break;
        }
        iter = iter->next_free;

    }
    if (iter != nullptr) {
        iter->is_free = false;
        num_of_free_bytes -= iter->size;
        num_of_free_blocks--;
    }
}

MallocMetadata *splitBlocks(MallocMetadata **head, MallocMetadata *to_split, size_t new_size) {
    if (to_split->size - new_size < SPLIT_SIZE + MetaAlignLen) {
        return nullptr;
    }
    num_of_alloc_blocks++;
    num_of_alloc_bytes -= MetaAlignLen;
    MallocMetadata *new_meta = (MallocMetadata *) (void *) ((char *) to_split + new_size + MetaAlignLen);
    new_meta->mmap_alloc = false;
    new_meta->size = to_split->size - new_size - MetaAlignLen;
    new_meta->cookie = randomCookies;
    sfree((void *) ((char *) new_meta + MetaAlignLen));
    listRemove(head, to_split);

    to_split->size = new_size;
    if (to_split == wildernessBlock) {
        wildernessBlock = new_meta;
    }
    return to_split;
}


MallocMetadata *
mergeBlocks(MallocMetadata **head, MallocMetadata *meta1, MallocMetadata *meta2) {

    num_of_alloc_blocks--;
    num_of_alloc_bytes += MetaAlignLen;
    if (meta1->is_free) {
        listRemove(head, meta1);
    }
    if (meta2->is_free) {
        listRemove(head, meta2);
    }
    listInsert(head, meta1, meta1->size + meta2->size + MetaAlignLen);
    if (meta2 == wildernessBlock) {
        wildernessBlock = meta1;
    }
    return meta1;
}

MallocMetadata *findIsPrevChunkFree(MallocMetadata **head, MallocMetadata *meta) {
    MallocMetadata *iter = *head;
    MallocMetadata *prev = nullptr;
    while (iter != nullptr) {
        if(iter->cookie != randomCookies){
            exit(0xdeadbeef);
        }
        if (iter < meta) {
            if (prev == nullptr) {
                prev = iter;
            } else if (prev < iter) {
                prev = iter;
            }
        }
        iter = iter->next_free;
    }
    if (prev != nullptr) {
        if ((MallocMetadata *) ((char *) prev + MetaAlignLen + prev->size) != meta) {
            prev = nullptr;

        }
    }
    return prev;
}

MallocMetadata *findIsNextChunkFree(MallocMetadata **head,MallocMetadata *meta) {
    MallocMetadata *iter = *head;
    MallocMetadata *next = nullptr;
    while (iter != nullptr) {
        if(iter->cookie != randomCookies){
            exit(0xdeadbeef);
        }
        if (iter > meta) {
            if (next == nullptr) {
                next = iter;
            } else if (next > iter) {
                next = iter;
            }
        }
        iter = iter->next_free;
    }
    if (next != nullptr) {
        if ((MallocMetadata *) ((char *) meta + MetaAlignLen + meta->size) != next) {
            next = nullptr;
        }
    }
    return next;

    MallocMetadata *meta_next = (MallocMetadata *) ((char *) meta + MetaAlignLen + meta->size);
    return meta_next->is_free ? meta_next : nullptr;
}


size_t _num_free_blocks() {
    return num_of_free_blocks;
}

size_t _num_free_bytes() {
    return num_of_free_bytes;
}

size_t _num_allocated_blocks() {
    return num_of_alloc_blocks;
}

size_t _num_allocated_bytes() {
    return num_of_alloc_bytes;
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * MetaAlignLen;
}

size_t _size_meta_data() {
    return MetaAlignLen;
}


void *smalloc(size_t size) {
    if (size == 0 || size > MAX_SIZE_ALLOC) {
        return nullptr;
    }
    void *ptr = nullptr;
    MallocMetadata *meta;
    if (size >= MMAPALLOC) {
        ptr = mmap(NULL, size + MetaAlignLen, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == (void *) -1) {
            return nullptr;
        }
        meta = (MallocMetadata *) ptr;
        meta->cookie = randomCookies;
        meta->mmap_alloc = true;
        meta->is_free = false;
        meta->size = size;
        num_of_alloc_bytes += size;
        num_of_alloc_blocks++;
    } else {
        MallocMetadata *iter = earliestFreeBlock(&freeBlocksList, size);
        //use free allocate
        if (iter != nullptr && (iter->is_free && size <= iter->size)) {
            MallocMetadata *to_return = iter;
            listRemove(&freeBlocksList,to_return);
            if (iter->size - size >= SPLIT_SIZE + MetaAlignLen) {
                to_return = splitBlocks(&freeBlocksList, iter, size);
            }
            to_return->mmap_alloc = false;
            return (void *) ((char *) (to_return) + MetaAlignLen);
        }
        if (wildernessBlock != nullptr && wildernessBlock->is_free) {
            if (sbrk(size - wildernessBlock->size) == (void *) -1) {
                return nullptr;
            }
            listRemove(&freeBlocksList,wildernessBlock);
            wildernessBlock->mmap_alloc = false;
            num_of_alloc_bytes += size - wildernessBlock->size;
            wildernessBlock->size = size;
            return (void *) ((char *) (wildernessBlock) + MetaAlignLen);
        }
        //make new allocate
        ptr = sbrk(0);
        if (sbrk(size + MetaAlignLen) == (void *) -1) {
            return nullptr;
        }
        meta = (MallocMetadata *) ptr;
        meta->is_free = false;
        meta->mmap_alloc = false;
        meta->cookie = randomCookies;
        meta->size = size;
        wildernessBlock = meta;
        num_of_alloc_bytes += meta->size;
        num_of_alloc_blocks++;
    }
    return (void *) ((char *) (meta) + MetaAlignLen);
}


void *scalloc(size_t num, size_t size) {
    void *ptr = smalloc(num * size);
    if (ptr == nullptr) {
        return nullptr;
    }
    size_t temp = num * size;
    memset(ptr, 0, temp);
    return ptr;
}

void sfree(void *p) {
    if (p == nullptr) {
        return;
    }
    MallocMetadata *meta = (MallocMetadata *) ((char *) p - MetaAlignLen);
    if(meta->cookie != randomCookies){
        exit(0xdeadbeef);
    }
    if (meta->mmap_alloc) {
        num_of_alloc_bytes-=meta->size;
        num_of_alloc_blocks--;
        munmap(meta, meta->size + MetaAlignLen);
    } else {
        listInsert(&freeBlocksList, meta, meta->size);
        if (meta != wildernessBlock) {
            MallocMetadata *meta_next = findIsNextChunkFree(&freeBlocksList,meta);
            if (meta_next != nullptr) {
                mergeBlocks(&freeBlocksList, meta, meta_next);
            }
        }
        MallocMetadata *meta_prev = findIsPrevChunkFree(&freeBlocksList, meta);
        if (meta_prev != nullptr) {
            mergeBlocks(&freeBlocksList, meta_prev, meta);
        }
    }
}


void *srealloc(void *oldp, size_t size) {
    void *ptr = nullptr;
    if (size == 0 || size > MAX_SIZE_ALLOC) {
        return nullptr;
    }
    if (oldp == nullptr) {
        return smalloc(size);
    }
    MallocMetadata *meta = (MallocMetadata *) ((char *) oldp - MetaAlignLen);
    if(meta->cookie != randomCookies){
        exit(0xdeadbeef);
    }
    listRemove(&freeBlocksList, meta);
    MallocMetadata *meta_prev = findIsPrevChunkFree(&freeBlocksList, meta);
    MallocMetadata *meta_next = findIsNextChunkFree(&freeBlocksList,meta);
    //reuse the current block
    if (size >= MMAPALLOC) {
        if (meta->size == size) {
            ptr = oldp;
        } else {
            ptr = smalloc(size);
            memmove(ptr, oldp, meta->size);
            sfree(oldp);
        }
    } else if (size <= meta->size) {
        splitBlocks(&freeBlocksList, meta, size);

        return oldp;
    }
        //merge with the adjacent block with the lower address
    else if (meta_prev != nullptr && size <= meta->size + meta_prev->size + MetaAlignLen) {
        MallocMetadata *merged_meta = mergeBlocks(&freeBlocksList, meta_prev, meta);
        listRemove(&freeBlocksList,merged_meta);
        ptr = (void *) ((char *) meta_prev + MetaAlignLen);
        memmove(ptr, oldp, meta->size);
        splitBlocks(&freeBlocksList, meta_prev, size);
    }
        //merge with the adjacent block with the lower address & enlarge wilderness
    else if (meta_prev != nullptr && wildernessBlock == meta) {
        MallocMetadata *merged_meta = mergeBlocks(&freeBlocksList, meta_prev, meta);
        listRemove(&freeBlocksList,merged_meta);
        num_of_alloc_bytes += size - wildernessBlock->size;
        if (sbrk(size - wildernessBlock->size) == (void *) -1) {
            return nullptr;
        }
        merged_meta->size = size;
        wildernessBlock->size = size;
        ptr = (void *) ((char *) merged_meta + MetaAlignLen);
        memmove(ptr, oldp, meta->size);
    }
        //enlarge wilderness
    else if (wildernessBlock == meta) {
        if (sbrk(size - wildernessBlock->size) == (void *) -1) {
            return nullptr;
        }
        num_of_alloc_bytes += size - wildernessBlock->size;
        wildernessBlock->size = size;
        ptr = oldp;

    }
        //merge with the adjacent block with the higher address
    else if (meta_next != nullptr && size <= meta->size + meta_next->size + MetaAlignLen) {
        MallocMetadata* merged_meta = mergeBlocks(&freeBlocksList, meta, meta_next);
        listRemove(&freeBlocksList,merged_meta);
        splitBlocks(&freeBlocksList, meta, size);
        ptr = oldp;
    }
        //merge all those three adjacent blocks together
    else if (meta_next != nullptr && meta_prev != nullptr &&
            size <= meta->size + meta_next->size + MetaAlignLen + meta_prev->size + MetaAlignLen) {
        MallocMetadata* merged_meta = mergeBlocks(&freeBlocksList, meta_prev, meta);
        listRemove(&freeBlocksList,merged_meta);
        merged_meta =  mergeBlocks(&freeBlocksList, meta_prev, meta_next);
        listRemove(&freeBlocksList,merged_meta);
        ptr = (void *) ((char *) meta_prev + MetaAlignLen);
        memmove(ptr, oldp, meta->size);
        splitBlocks(&freeBlocksList, meta_prev, size);

    }
        //the wilderness block is the adjacent block with the higher address
    else if (meta_next != nullptr  && wildernessBlock == meta_next) {
        //merge all those three adjacent blocks together & enlarge wilderness
        if (meta_prev != nullptr) {
            MallocMetadata* merged_meta = mergeBlocks(&freeBlocksList, meta_prev, meta);
            listRemove(&freeBlocksList,merged_meta);
            merged_meta =  mergeBlocks(&freeBlocksList, meta_prev, meta_next);
            listRemove(&freeBlocksList,merged_meta);
            ptr = (void *) ((char *) meta_prev + MetaAlignLen);
            memmove(ptr, oldp, meta->size);
        }
            //merge with the adjacent block with the higher address & enlarge wilderness
        else {
            MallocMetadata* merged_meta = mergeBlocks( &freeBlocksList,  meta, meta_next);
            listRemove(&freeBlocksList,merged_meta);
            ptr = oldp;
        }
        if(size > wildernessBlock->size)
        {
            if(sbrk(size - wildernessBlock->size) == (void *) -1)
            {
                return nullptr;
            }
            num_of_alloc_bytes += size - wildernessBlock->size;
            wildernessBlock->size = size;
        }

    } else {
        ptr = smalloc(size);
        if (ptr == nullptr) {
            return nullptr;
        }
        memmove(ptr, oldp, meta->size);
        sfree((void *) ((char *) (meta) + MetaAlignLen));
    }
    return ptr;
}