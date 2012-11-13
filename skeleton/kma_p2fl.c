/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the power-of-two free list
 *             algorithm
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.2 $
 *    Last Modification: $Date: 2009/10/31 21:28:52 $
 *    File: $RCSfile: kma_p2fl.c,v $
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: kma_p2fl.c,v $
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
#ifdef KMA_P2FL
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

/************Private include**********************************************/
#include "kpage.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */


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
} page_t;

typedef enum
{
  NORMAL,
  BIG,
  HUGE
} page_size_t;


/************Global Variables*********************************************/

static kpage_t* pages = 0;

/************Function Prototypes******************************************/

// get the first page and add freelist struct
void initializepages();

// add a buffer to the free list
void addtofreelist(void*, int); 

// try to alloc by using the free list
void* allocintofreelist(kma_size_t);

// get another page and add buffers to the free lists
void allocate_new_page(page_size_t);

// free all the kpages we've gotten
void freekpages();

/************External Declaration*****************************************/


/**************Implementation***********************************************/

void*
kma_malloc(kma_size_t size)
{

  printf("allocating: %d\n", (unsigned int)size);

  if (!pages) {
    initializepages();
  }

  size = size + 4;
  
  void* addr = allocintofreelist(size);
 
  if (addr != NULL) {
    return addr;
  }
  
  if (size < 2048) {
    allocate_new_page(NORMAL);
  } else if (size < 4096) {
    allocate_new_page(BIG);
  } else if (size < 8176) {
    allocate_new_page(HUGE);
  }

  addr = allocintofreelist(size);
  if (addr != NULL) {
    return addr;
  }

  return NULL;
}

void
kma_free(void* ptr, kma_size_t size)
{
  freelist_t* list = (freelist_t *)(pages->ptr + sizeof(page_t));
  ptr = (ptr - sizeof(int));
  int mysize = *((int *) ptr);
  printf("size %d mysize %d\n", size, mysize);
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
allocintofreelist(kma_size_t size)
{
  freelist_t* list = (freelist_t*)(pages->ptr + sizeof(page_t));
  int i;
  for (i = 0; i < 10; i ++) {
    if (list->bufsizes[i] >= size) {
      if (list->lists[i] != NULL) {
	void* addr = list->lists[i];
	void* nextaddr = *((void **) addr);
	list->lists[i] = nextaddr;
 	*((int *) addr) = size;
	list->allocs++;
	return addr + sizeof(int);
      } else {
	return NULL;
      }
    } 
  }
  return NULL;
}

void initializepages()
{
  kpage_t* new_kpage = get_page();
  page_t* new_page = (page_t *)(new_kpage->ptr);
  new_page->me = new_kpage;
  new_page->nextpage = NULL;
  pages = new_kpage;
  freelist_t* list = (freelist_t*)((void *)new_page + sizeof(page_t));
  list->minaddr = 0;
  list->maxaddr = 0;
  list->allocs = 0;
  int i;
  int size = 16;
  for(i = 0; i < 10; i ++) {
    list->bufsizes[i] = size;
    list->lists[i] = NULL;
    size *= 2;
  }
  list->bufsizes[9] = 8176;
  void* nextaddr = (void *)new_page + sizeof(page_t) + sizeof(freelist_t);
  size = 16;
  for(i = 0; i < 10; i++) {
    if ((((unsigned long int)nextaddr + size) - (unsigned long int)new_page) < new_kpage->size) {
      addtofreelist(nextaddr, size);
      nextaddr += size;
      size *= 2;
    }
  }
  size /= 2;
  while (size >= 16) {
    while ((((unsigned long int)nextaddr + size) - (unsigned long int)new_page) < new_kpage->size) {
      addtofreelist(nextaddr, size);
      nextaddr += size;
    }
    size /= 2;
  }
}

void allocate_new_page(page_size_t s)
{
  kpage_t* new_kpage = get_page();
  page_t* new_page = (page_t *)(new_kpage->ptr);
  new_page->me = new_kpage;
  new_page->nextpage = NULL;
  page_t* old_page = (page_t *)(pages->ptr);
  while(old_page->nextpage != NULL)
    old_page = old_page->nextpage;
  old_page->nextpage = new_page;
  void* current = new_kpage->ptr + sizeof(page_t);
  void* max = new_kpage->ptr + new_kpage->size;
  int size = 16;
  if (s == NORMAL) {
    while((current + size) < max) {
      addtofreelist(current, size);
      current = current + size;
      size *= 2;
    }
    size /= 2;
  } else if (s == BIG) {
    size = 4096;
  } else if (s == HUGE) {
    size = 8176;
  }
  while (size >= 16) {
    while ((current + size) <= max) {
      addtofreelist(current, size);
      current += size;
    }
    size /= 2;
  }
}

void addtofreelist(void* addr, int size) 
{
  freelist_t* list = (freelist_t*)(pages->ptr + sizeof(page_t));
  int i;
  for (i = 0; i < 10; i ++) {
    if (size == list->bufsizes[i]) {
      *((void **)addr) = list->lists[i];
      list->lists[i] = addr;
      break;
    }
  }
}

#endif // KMA_P2FL
