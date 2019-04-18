#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "../include/budmm.h"
#include "../include/budhelp.h"

#define ORDER_TO_INDEX(x) (x - ORDER_MIN)

extern bud_free_block free_list_heads[NUM_FREE_LIST];


/*    _____     _____  ____ ___________   _
//   /     \   / ___ \/_   /_   \   _  \ |  | __
//  /  \ /  \ / / ._\ \|   ||   /  /_\  \|  |/ /
// /    Y    <  \_____/|   ||   \  \_/   \    <
// \____|__  /\_____\  |___||___|\_____  /__|_ \
        \/    m a r k k o s h k i n    \/     \/
*/


char sizeToOrder(uint32_t size) {
    // log base 2
    char order = 0;
    while (size > 1) {
        size /= 2;
        order++;
    }
    return order;
}


uint32_t roundRequestSize(uint32_t request) {
    request += sizeof(bud_header); // account for header
    uint32_t rounded = 1 << ORDER_MIN;

    while (rounded < request) {
        rounded <<= 1;
    }
    return rounded;
}

char appropriateBlockOrder(uint32_t request) {

    request = roundRequestSize(request);
    char order = sizeToOrder(request);
    if (order < ORDER_MIN) return ORDER_MIN;
    if (order >= ORDER_MAX) return -1;
    return order;
}



int isEmpty(bud_free_block * freeListHead) {


    if (freeListHead == (freeListHead->next)) return 1;

    return 0;
}

bud_free_block* findFreeBlock(uint32_t size) {
    int minOrder = appropriateBlockOrder(size);
    int freeListIndex = ORDER_TO_INDEX(minOrder);

    for (int i = freeListIndex; i < ORDER_TO_INDEX(ORDER_MAX); i++) {

        if (!isEmpty(&free_list_heads[i])) {

            return free_list_heads[i].next;
        }
    }
    return NULL;
}

/*
 * Adds the free block to the corresponding list of free blocks
 * @freeBlock pointer to the free memory block
 */

void addBlock(bud_free_block* freeBlock) {
    int listIndex = ORDER_TO_INDEX((freeBlock->header).order);

    ((free_list_heads[listIndex].next)->prev) = freeBlock;
    freeBlock->next = free_list_heads[listIndex].next;
    free_list_heads[listIndex].next = freeBlock;
    freeBlock->prev = &free_list_heads[listIndex];

}


void removeBlock(bud_free_block* block) {
    (block->next)->prev = block->prev;
    (block->prev)->next = block->next;
    block->prev = NULL;
    block->next = NULL;
}

bud_free_block* makeFreeHeader(bud_free_block* block, int _order) {
    (block->header).order = _order;
    (block->header).allocated = 0;
    return block;
}


void splitBlock(bud_free_block* block) {
    if ((block->header).order <= ORDER_MIN){
        return; // don't ever split min
    }
    int tempOrder = (block->header).order;
    uint32_t size  = ORDER_TO_BLOCK_SIZE((block->header).order);
    removeBlock(block);
    makeFreeHeader(block, tempOrder-1);

    char* blockp = (char*)block;

    blockp += (size/2);
    makeFreeHeader((bud_free_block*)blockp, tempOrder-1);
    addBlock((bud_free_block*)blockp);
    addBlock(block);
    return;
}



void *bud_malloc(uint32_t rsize) {

    char order;
    if ( ((int)rsize <= 0) || ((order = appropriateBlockOrder(rsize)) == -1)) {
        errno = EINVAL;
        return NULL;
    }

    // if request is valid, try finding the block
    bud_free_block* freeBlock;

    if ((freeBlock = findFreeBlock(rsize)) != NULL) {

        int startOrder = (freeBlock->header).order;


        for (int k = startOrder; k > order; k--) {
            splitBlock(freeBlock);
        }


        if  ( (rsize+sizeof(bud_header)) == roundRequestSize(rsize)) {
            (freeBlock->header).padded = 0;
        } else (freeBlock->header).padded = 1;
        removeBlock(freeBlock);
        (freeBlock->header).rsize = rsize;
        (freeBlock->header).order = order;
        (freeBlock->header).allocated = 1;


        char* payload = (char*)&(freeBlock->header);
        payload += sizeof(bud_header);


        return (void*)payload;


    }
    else {
        void* old_brk;
        if ((old_brk = bud_sbrk()) == (void *)-1) {
            errno = ENOMEM;
            return NULL;
        }
        else {

            bud_free_block* block = makeFreeHeader(old_brk, ORDER_MAX-1);
            addBlock(block);
            return bud_malloc(rsize);

        }


    }

    return NULL;
}



int findAndRemove(bud_free_block* block) {
    int li = ORDER_TO_INDEX(block->header.order);
    bud_free_block* temp = free_list_heads[li].next;
    while ( temp != &free_list_heads[li]) {
        if (temp == block) {

            (temp->next)->prev = temp->prev;
            (temp->prev)->next = temp->next;
            temp->next = NULL;
            temp->prev = NULL;
            return 1;
        }
        temp = temp->next;
    }
    return 0;
}

bud_free_block* buddyAddress(bud_free_block* block) {
bud_header* temp = &(block->header);
bud_free_block* buddy = ((bud_free_block *)(((uintptr_t)temp)^(ORDER_TO_BLOCK_SIZE(temp->order))));
return buddy;


}



void koala(bud_free_block* block) {
    bud_free_block* buddy = buddyAddress(block);


    if ((buddy->header).allocated == 0
        && (buddy->header).order == (block->header).order
        && ((block->header).order < ORDER_MAX-1)
        && ((char*)block < (char*)bud_heap_end())
    ) {
        //entering the coalesce
        findAndRemove(buddy);
        findAndRemove(block);

        // pick bottom one to write header at
        if ((char*)block < (char*)buddy) {
            (block->header).order++;
            addBlock(block);
            koala(block);
        }
        else {
            (buddy->header).order++;
            addBlock(buddy);
            koala(buddy);

        }


    }
}

void bud_free(void *ptr) {

    if  (ptr < bud_heap_start() ||
         ptr > bud_heap_end() ||
         ((((uintptr_t) ptr) & 7)!=0)
    ) abort();

    // move to the header
    bud_header* nowHeader =  ((bud_header*)(((char*)ptr)-sizeof(bud_header)));


    if( ((nowHeader->allocated) == 0) ||
       ((nowHeader->order) < ORDER_MIN) ||
        (nowHeader->order >= ORDER_MAX) ||
        ((nowHeader->padded) == 0 &&
         (nowHeader->rsize + sizeof(bud_header)) != roundRequestSize(nowHeader->rsize)) ||
        ((nowHeader->padded) == 1 &&
         (nowHeader->rsize + sizeof(bud_header)) == roundRequestSize(nowHeader->rsize)) ||
       (appropriateBlockOrder(nowHeader->rsize) !=  (nowHeader->order))) abort(); 

    bud_free_block* nowBlock = (bud_free_block*) nowHeader;


    makeFreeHeader(nowBlock, nowHeader->order);
    addBlock(nowBlock);

    koala(nowBlock);
}


void *bud_realloc(void *ptr, uint32_t rsize) {
    if (rsize == 0) {
        bud_free((char*)ptr);
        return NULL;
    }
    else if (ptr == NULL) return bud_malloc(rsize);

    if  (ptr < bud_heap_start() ||
         ptr > bud_heap_end() ||
         ((((uintptr_t) ptr) & 7)!=0)
    ) abort();

    bud_header* nowHeader =  ((bud_header*)(((char*)ptr)-sizeof(bud_header)));
    bud_free_block* nowBlock =  (bud_free_block*) nowHeader;

    if( ((nowHeader->allocated) == 0) ||
       ((nowHeader->order) < ORDER_MIN) ||
       (nowHeader->order >= ORDER_MAX) ||
       ((nowHeader->padded) == 0 &&
        (nowHeader->rsize + sizeof(bud_header)) != roundRequestSize(nowHeader->rsize)) ||
       ((nowHeader->padded) == 1 &&
        (nowHeader->rsize + sizeof(bud_header)) == roundRequestSize(nowHeader->rsize)) ||
       (appropriateBlockOrder(nowHeader->rsize) !=  (nowHeader->order))) abort();
    
    int order;
    if ( ((int)rsize <= 0) || ((order = appropriateBlockOrder(rsize)) == -1)) {
        errno = EINVAL;
        return NULL;
    }
    if (order == (nowHeader->order)) {
        return ptr;
    }
    else if (order > (nowHeader->order)) {
        char* mal = bud_malloc(rsize);
        memcpy(mal, ptr, nowHeader->rsize);
        bud_free((char*)ptr);
        return (void*)mal;
    }
    else  {
        nowHeader->allocated = 0;
        addBlock(nowBlock);
        for (int j = (nowHeader->order); j > order; j--) {
            splitBlock(nowBlock);
        }
        nowHeader->allocated = 1;
        nowHeader->rsize = rsize;
        removeBlock(nowBlock);
        return ptr;

    }

}
