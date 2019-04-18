#include "../include/budmm.h"

// Helper methods that are used in budmm.c are
// declared and described in this header file


/*
 * Converts size to order.
 * @param size
 * @return order
 */
char sizeToOrder(uint32_t size);


/*
 * Converts size to order.
 * @param  request Size in bytes
 * @return size of the block that is just enough
 *         to serve the request
 */
uint32_t roundRequestSize(uint32_t request);


/*
 * Converts size to order.
 * @param  request Size in bytes
 * @return on success:
 *             order of the block that is just enough
 *             to serve the request
 *
 *         on failure: -1
 */
char appropriateBlockOrder(uint32_t request);


/*
 * Converts size to order.
 * @freeListHead the sentinel of the circular linked list
 * @return 1 if empty, 0 if non-empty
 */
int isEmpty(bud_free_block* block);


/*
 * Combines previous functions to find appropriate memory block.
 * @size Requested size
 * @return on success a pointer to memory block, NULL on failure
 */
bud_free_block* findFreeBlock(uint32_t size);


/*
 * Adds the free block to the corresponding list of free blocks
 * @freeBlock pointer to the free memory block
 */
void addBlock(bud_free_block* freeBlock);


/*
 * Removes the block from the list of free blocks
 * @freeBlock Pointer to the memory block
 */

void removeBlock(bud_free_block* block);


/*
 * Prepares the header for the free block
 * @block Pointer to the location, where the header will
 *        be created
 * @order Writes this order in the header of the block
 */

bud_free_block* makeFreeHeader(bud_free_block* block, int _order);


/*
 * Splits the block in half
 * @block Pointer to the free block
 */
void splitBlock(bud_free_block* block);



/*
 * Finds the block and removes it from the free list
 * @block Pointer to the block
 * @return 0 on success, -1 on failure
 */

int findAndRemove(bud_free_block* block);


/*
 * Finds the address of the buddy of the block
 * @block Pointer to the block
 * @return Pointer to the buddy
 */
bud_free_block* buddyAddress(bud_free_block* block);


/*
 * Checks if the block can be coalesced, and if so,
 * it coalesces it with it's buddy
 * @block Pointer to the block
 */
void koala(bud_free_block* block);


