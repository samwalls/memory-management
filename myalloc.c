#include <sys/mman.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include "myalloc.h"

/*--- MACROS ---*/
/*
 * Memory Layout Example (arbitrary N and M):
 *
 * |--------------N * PAGESIZE---------------|
 * |HEADER|---REGION---|HEADER|--REG--|HEADER|
 *                                           |   page end of one allocation has
 * /-----------------------------------------/   pointer to next page (if any)
 * |
 * V
 * |--------------M * PAGESIZE---------------|
 * |HEADER|---------------------------|HEADER|
 */
#define HEADER_DATA unsigned long
#define HEADER_FROM_REGION(region_p) ((block_header*)((void*)region_p - sizeof(block_header)))
#define REGION_FROM_HEADER(header_p) ((void*)((void*)header_p + sizeof(block_header)))

//the minimum allocation size
#define ALLOCATION_MINIMUM sizeof(char)
//maximum allocation size
#define ALLOCATION_MAXIMUM ~(((HEADER_DATA)FLAG_COUNT) << HEADER_FREE_BIT)
//number of flags at the start of the data segment
#define FLAG_COUNT 2
//mask which hides the flags from the data segment, leaving behind the size
//i.e. if HEADER_DATA were char, and FLAG_COUNT were 2, FLAG_MASK would be 00111111
#define FLAG_MASK ~(((1 << FLAG_COUNT) - 1) << (sizeof(HEADER_DATA) * CHAR_BIT - FLAG_COUNT))

/*-------------------------------------------*/
/*--- HEADER/FOOTER/FUNCTION DECLARATIONS ---*/
/*-------------------------------------------*/
typedef struct block_header {
    HEADER_DATA data;
    struct block_header *next;
} block_header;

/*
 * Header Data Segment:
 * |f|e|--------------...
 *  ^ ^ ^
 *  | | |
 *  | | |
 *  | | |
 *  | | remaining bits are treated as a positive integral number
 *  | |
 *  | page-end-bit
 *  |
 *  free bit
 */

//position of the free-bit, in the number of bits from the lsb (little endian) to it
const int HEADER_FREE_BIT = sizeof(HEADER_DATA) * CHAR_BIT - 1;

const int HEADER_PAGE_END_BIT = sizeof(HEADER_DATA) * CHAR_BIT - 2;

void init();

/*
 * Allocate n pages-worth of heap space, return a pointer to the beginning of the
 * page as a block header. The block header in question initially represents the
 * whole page as a free region.
 * A header is also placed at the end of the page, with page-end-bit set to 1, and a size of 0
 * this header will always be "in-use" internally; eventually the header's next pointer
 * will point to the base of the next page(s) that get allocated.
 * If n is 0, NULL is returned.
 */
block_header *allocatePage(unsigned int n);

//allocate new heap space with an added header, size is clamped between
//ALLOCATION_MINIMUM and ALLOCATION_MAXIMUM inclusive
block_header *allocate(int size);

//return pointer to heap space in the first free region that can support a new
//block allocation of the specified size
block_header *first_fit(int size);

//append a new region to the end of the memory, creates a new page if needed
block_header *append_region(int size);

//divide a region into two, based on size, updating the necessary header pointers
block_header *divide(block_header *header, int size);

//coalesce all regions left-wards
void coalesce_left(block_header *header);

//return the corresponding region size from the header's data segment
int header_getsize(block_header *header);

//set the size value for the header to the value of size
//warning, the msb of the size field is ignored!
void header_setsize(block_header *header, int size);

//return true if the region this header corresponds to is marked as free
bool header_isfree(block_header *header);

//set the header's free-flag to the value of free
void header_setfree(block_header *header, bool free);

//return true if the header's page-end-bit is set
bool header_isend(block_header *header);

//set the header's page-eng-flag to the value of end
void header_setend(block_header *header, bool end);

//we store an entry point to the memory (essentially the head to a linked list)
static block_header *ROOT = NULL;

//we will update END as blocks get added
static block_header *END = NULL;

/*------------------------------*/
/*--- MYALLOC IMPLEMENTATION ---*/
/*------------------------------*/
void *myalloc(int size) {
    if (ROOT == NULL)
        init();
    block_header *header = first_fit(size);
    return REGION_FROM_HEADER(header);
}

void myfree(void *ptr) {
    //mark the region as "not being used", but leave deallocation up to the coalescing function
    block_header *header = HEADER_FROM_REGION(ptr);
    header_setfree(header, true);
    //todo coalesce
}

/*--- OTHER FUNCTIONS ---*/

void init() {
    ROOT = allocatePage(1);
    END = ROOT->next;
}

/*
 * Allocate n pages-worth of heap space, return a pointer to the beginning of the
 * page as a block header. The block header in question initially represents the
 * whole page as a free region.
 * A header is also placed at the end of the page, with page-end-bit set to 1, and a size of 0
 * this header will always be "in-use" internally; eventually the header's next pointer
 * will point to the base of the next page(s) that get allocated.
 * If n is 0, NULL is returned.
 */
block_header *allocatePage(unsigned int n) {
    if (n == 0)
        return NULL;
    size_t size = n * getpagesize();
    void *alloc = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (alloc == MAP_FAILED)
        perror("MMAP error:");
    block_header *pageRoot = (block_header*) alloc;
    block_header *pageFooter = (void *)pageRoot + (size - sizeof(block_header));
    header_setend(pageFooter, true);
    pageRoot->next = pageFooter;
    pageRoot->data = 0;
    header_setfree(pageRoot, true);
    header_setsize(pageRoot, size - 2 * sizeof(block_header));
    return pageRoot;
}

//return pointer to heap space in the first free region that can support a new
//block allocation of the specified size
block_header *first_fit(int size) {
    if (size < ALLOCATION_MINIMUM)
        size = ALLOCATION_MINIMUM;
    block_header *header = ROOT;
    while (header == ROOT || header->next != NULL) {
        int regionSize = header_getsize(header);
        //We should make sure that there would be at least enough space to
        //allocate a minimum unit in the block ahead of this one.
        if (header_isfree(header) && regionSize >= size) {
            if (regionSize >= size + sizeof(block_header)) {
                //the region in question is big enough to be split into two
                return divide(header, size);
            } else {
                //the region is not big enough to be split, but is still big
                //enough to hold the requested size
                //return the header, with no alterations to size
                header_setfree(header, false);
                return header;
            }
        }
        header = header->next;
    }
    //if we reach here, no fit could be found
    return append_region(size);
}

//append a new region after the last region (hence, in a new page)
block_header *append_region(int size) {
    block_header *oldEnd = END;
    block_header *page = allocatePage((size / getpagesize()) + 1);
    //point end to the cap of the new page
    END = page->next;
    //the cap of the old page contains a pointer to the header of the new page
    oldEnd->next = page;
    assert(divide(page, size) != NULL);
    return page;
}

//divide a region into two, based on size, updating the necessary header pointers
//return a the passed header, or NULL if the region could not be divided
block_header *divide(block_header *header, int size) {
    //if the passed header was null, was not free (therefore including page-ends),
    //or doesn't have sufficient size; return null
    if (header == NULL || !header_isfree(header) || header_getsize(header) < size - sizeof(block_header))
        return NULL;
    else {
        block_header *middle = REGION_FROM_HEADER(header) + size;
        block_header *next = header->next;
        header_setsize(header, ((void*)middle) - ((void*)REGION_FROM_HEADER(header)));
        header_setsize(middle, ((void*)next) - ((void*)REGION_FROM_HEADER(middle)));
        header_setfree(header, false);
        header_setfree(middle, true);
        header->next = middle;
        middle->next = next;
        return header;
    }
}

/*--- BIT-TWIDDLING ---*/

//return the corresponding region size from the header's data segment
int header_getsize(block_header *header) {
    int d = header->data;
    //MSB is reserved for free-bit, the rest is size data; mask everything except free-bit
    return d & ~((HEADER_DATA)1 << HEADER_FREE_BIT);
}

//set the size value for the header to the value of size
//warning, the msb of the size field is ignored!
void header_setsize(block_header *header, int size) {
    bool free = header_isfree(header);
    header->data = size;
    header_setfree(header, free);
}

//return true if the region this header corresponds to is marked as free
bool header_isfree(block_header *header) {
    //since the msb is also the sign-bit, we can simply check if the data segment is < 0
    return (header->data & ((HEADER_DATA)1 << HEADER_FREE_BIT)) != 0;
}

//set the header's free-flag to the value of free
void header_setfree(block_header *header, bool free) {
    //set the MSB to 1 if true, 0 otherwise
    if (free)
        header->data |= ((HEADER_DATA)1 << HEADER_FREE_BIT);
    else
        header->data &= ~((HEADER_DATA)1 << HEADER_FREE_BIT);
}

//return true if the header's page-end-bit is set
bool header_isend(block_header *header) {
    return (header->data & ((HEADER_DATA)1 << HEADER_PAGE_END_BIT)) != 0;
}

//set the header's page-eng-flag to the value of end
void header_setend(block_header *header, bool end) {
    //set the MSB to 1 if true, 0 otherwise
    if (end)
        header->data |= ((HEADER_DATA)1 << HEADER_PAGE_END_BIT);
    else
        header->data &= ~((HEADER_DATA)1 << HEADER_PAGE_END_BIT);
}
