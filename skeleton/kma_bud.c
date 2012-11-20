/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.2 $
 *    Last Modification: $Date: 2009/10/31 21:28:52 $
 *    File: $RCSfile: kma_bud.c,v $
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: kma_bud.c,v $
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.1  2004/11/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *
 ***************************************************************************/
#ifdef KMA_BUD
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h> // for debug logging, remove in final hand-in version
/************Private include**********************************************/
#include "kpage.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define MIN_BLOCKSIZE 16

typedef struct
{
  void* minaddr;
  void* maxaddr;
  int allocs;
  int bufsizes[10];
  void* lists[10];
} freelist_t;

typedef struct
{
  kpage_t* me;
  void* nextpage;
  //char bitmap[64];
} page_t;

typedef enum
{
  NORMAL,
  BIG,
  HUGE
} page_size_t;

typedef enum 
{
  MEM_FREE = 0,
  MEM_USED = 1
} mem_status_t;

/************Global Variables*********************************************/
static kpage_t* pages = NULL;
/************Function Prototypes******************************************/
// get the first page and add freelist struct
void initializepages();

// add a buffer to the free list
void addtofreelist(void*, int); 

// try to alloc by using the free list
void* get_free_block(kma_size_t);

// get another page and add buffers to the free lists
void allocate_new_page();

// free all the kpages we've gotten
void freekpages();

// update the bitmap representing used/free memory regions
void update_bitmap(void*,kma_size_t,mem_status_t);

// coalesce adjacent memory regions, if possible
void coalesce_blocks();
/************External Declaration*****************************************/

/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{
  printf("handling malloc request for %d bytes\n",(unsigned int)size);
  if (!pages) {
    initializepages();
  }
  size = size + sizeof(int);
  
  void* addr = get_free_block(size);
  
  if (addr != NULL) {
    update_bitmap(addr, size, MEM_USED);
    return addr;
  }
  allocate_new_page();
  
  addr = get_free_block(size);
  if (addr != NULL) {
    update_bitmap(addr, size, MEM_USED);
    return addr;
  }
  return NULL;
}

void 
kma_free(void* ptr, kma_size_t size)
{
  freelist_t* list = (freelist_t *)(pages->ptr + sizeof(page_t));
  ptr = (ptr - sizeof(int));
  int mysize = *((int *) ptr); // size INCLUDES header ptr
  update_bitmap(ptr, mysize, MEM_FREE);
  coalesce_blocks();
  addtofreelist(ptr, mysize);
  
  list->allocs--;
  if (list->allocs <= 0)
    freekpages();
}

void
freekpages()
{
  page_t* p = pages->ptr;
  page_t* next_p;
  while (p != NULL) {
    next_p = p->nextpage;
    free_page(p->me);
    p = next_p;
  }
}

void* 
get_free_block(kma_size_t size) //size INCLUDES header ptr
{
  freelist_t* list = (freelist_t*)(pages->ptr + sizeof(page_t));
  int i=0;
  while (list->bufsizes[i] < size) {
    i++;
  }
  int idx = i;
  while (list->lists[i] == NULL) {
    i++;
    if (i == 10) {
      return NULL; //no more space
    }
  }
  while (i > idx) {
    void* addr = list->lists[i];
    void* nextaddr = *((void**)addr);
    list->lists[i] = nextaddr;
    printf("addr = %p\n",addr);
    
    nextaddr = list->lists[i-1];
    list->lists[i-1] = addr;
    if (i == 9) { // special case, because bufsizes[9] != bufsizes[8]*2
      *((void**)addr) = nextaddr;
    } else {
      *((void**)addr) = addr + list->bufsizes[i-1];
      addr = *((void**)addr);
      *((void**)addr) = nextaddr;
    }
    
    i--;
  }
  void* returnaddr = list->lists[idx];
  list->lists[idx] = *((void**)returnaddr);
  *((int*)returnaddr) = list->bufsizes[idx];
  list->allocs++;
  return returnaddr+sizeof(int);
}

void initializepages()
{
  int effectivePagesize = (unsigned int)PAGESIZE - sizeof(kpage_t) - sizeof(page_t) - sizeof(freelist_t);
  kpage_t* new_kpage = get_page();
  page_t* new_page = (page_t *)(new_kpage->ptr);
  
  new_page->me = new_kpage;
  new_page->nextpage = NULL;
  pages = new_kpage;
  
  freelist_t* list = (freelist_t*)((void *)new_page + sizeof(page_t));
  list->allocs = 0;
  
  int i;
  int size = 16;
  for(i = 0; i < 10; i++) {
    list->bufsizes[i] = size;
    list->lists[i] = NULL;
    size *= 2;
  }
  list->bufsizes[9] = effectivePagesize;
  void* nextaddr = (void*)new_page + sizeof(page_t) + sizeof(freelist_t);
  addtofreelist(nextaddr,list->bufsizes[9]);
}

void allocate_new_page()
{
  int effectivePagesize = (unsigned int)PAGESIZE - sizeof(kpage_t) - sizeof(page_t) - sizeof(freelist_t);
  
  kpage_t* new_kpage = get_page();
  page_t* new_page = (page_t *)(new_kpage->ptr);
  new_kpage->ptr = (void*)new_page;
  
  new_page->me = new_kpage;
  new_page->nextpage = NULL;
  
  page_t* old_page = (page_t*)(pages->ptr);
  while (old_page->nextpage != NULL) {
    old_page = old_page->nextpage;
  }
  old_page->nextpage = new_page;
  
  // don't really need to add sizeof(freelist_t), but max. buffer size will be bufsizes[9] regardless so might as well do it for consistency
  void* effectiveStartAddr = (void*)(new_page) + sizeof(page_t) + sizeof(freelist_t);
  addtofreelist(effectiveStartAddr,effectivePagesize);
}

void addtofreelist(void* addr, int size) // size INCLUDES head ptr
{
  freelist_t* list = (freelist_t*)(pages->ptr + sizeof(page_t));
  int i;
  for (i = 0; i < 10; i ++) {
    printf("i = %d, bufsize = %d\n",i,list->bufsizes[i]);
    if (size == list->bufsizes[i]) {
      *((void **)addr) = list->lists[i];
      list->lists[i] = addr;
      break;
    }
  }
}

// update the bitmap representing used/free memory regions
void update_bitmap(void* ptr, kma_size_t size, mem_status_t status) {

}

// coalesce memory regions around ptr, if possible
// updates ptr and returns size of coalesced free region
void coalesce_blocks() {
}

#endif // KMA_BUD
