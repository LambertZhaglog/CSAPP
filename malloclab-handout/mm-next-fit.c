/*
 * mm-book.c - The malloc package that copy from book CSAPP, section 9.9.12.
 * 
 * Simple, 32-bit and 64-bit clean allocator based on implicite free
 * lists, first-fit placement, and boundary tag coalescing, as describled 
 * int the CS:APP3e text. Block must be aligned to doubleword (8 byte)
 * boundaries. Minimum block size is 16 byte.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
	       /* Team name */
	       "lambert",
	       /* First member's full name */
	       "Lambert Zhaglog",
	       /* First member's email address */
	       "LambertZhaglog@hust.edu.cn",
	       /* Second member's full name (leave blank if none) */
	       "",
	       /* Second member's email address (leave blank if none) */
	       ""
};

/*
 * If NEXT_FIT defined use next fit search, else use first-fit search
 */

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE 4      /* Word and header/footer size (byte) */
#define DSIZE 8      /* Double word size (byte) */
#define CHUNKSIZE (1<<12) /*Extend heap by this amount (bytes) */

#define MAX(x,y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word  at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p)= (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/*Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) -WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
/* $end mallocmacros */

/* Global variables */
  static char *heap_listp =0; /* Pointer to first block */
#define NEXT_FIT
#ifdef NEXT_FIT
static char *rover; /* Next fit rover */
#endif

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkheap(int verbose);
static void checkblock(void *bp);



/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  /* Create the initial empty heap */
  if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
    return -1;
  PUT(heap_listp, 0); /*Alignment padding */
  PUT(heap_listp+(1*WSIZE),PACK(DSIZE,1)); /* Prologue header */
  PUT(heap_listp+(2*WSIZE),PACK(DSIZE,1)); /* Prologue footer */
  PUT(heap_listp+(3*WSIZE),PACK(0,1)); /* Epilogue header */
  heap_listp =heap_listp+(2*WSIZE);

#ifdef NEXT_FIT
  rover=heap_listp;
#endif

  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
    return -1;
  return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
  size_t asize; /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if not fit */
  char *bp;
  
  /* no need to call mm_init() by the lib, because the application call it for preparation.
     if(heap_listp==0){
     mm_init();
     }
  */
  
  /* Ignore spurious requests */
  if(size==0)
    return NULL;

  /* Adjust block size to include overhead and alignment reqs. */
  /* we can use unify asize caculation expression 
     if(size<=DSIZE)
     asize=2*DSIZE;
     else
     asize=DSIZE*((size + (DSIZE) + (DSIZE-1))/DSIZE);
  */
  asize=DSIZE*((size+(DSIZE)+(DSIZE-1))/DSIZE);

  /* Search the free list for a fit */
  if((bp=find_fit(asize))!=NULL){
    place(bp,asize);
    return bp;
  }

  /* No fit found. Get more memory and place the block */
  extendsize=MAX(asize,CHUNKSIZE);
  if((bp=extend_heap(extendsize/WSIZE))==NULL)
    return NULL;
  place(bp,asize);
  return bp;
  
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
  void *bp=ptr;
  if(bp==0)
    return;

  size_t size=GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size,0));
  PUT(FTRP(bp), PACK(size,0));
  coalesce(bp);
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp){
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size =GET_SIZE(HDRP(bp));

  if(prev_alloc && next_alloc){ /* case 1*/
    return bp;
  }else if( prev_alloc && !next_alloc){ /* case 2 */
    size =size+ GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));// it's right, because ftrp depend oon hdrp
  }else if (!prev_alloc && next_alloc){ /* case 3 */
    size=size+ GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    bp=PREV_BLKP(bp);
  }else{  /* case 4 */
    size=size+GET_SIZE(HDRP(PREV_BLKP(bp))) +
      GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
    bp=PREV_BLKP(bp);
  }
#ifdef NEXT_FIT
  /* Make sure the rover isn't pointing into the free block */
  /* that we just coalesced */
  if((rover>(char *)bp) && (rover < NEXT_BLKP(bp)))
    rover =bp;
#endif

  return bp;
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
  void *newptr;
  size_t oldsize;

  /* If size ==0 then this is just free, and we return NULL */
  if(size==0){
    mm_free(ptr);
    return 0;
  }

  /* if oldptr is NULL, then this is just malloc. */
  if(ptr ==NULL){
    return mm_malloc(size);
  }

  newptr=mm_malloc(size);

  /* if reallloc fails the original block if left untouched */
  if(!newptr){
    return 0;
  }

  /* Copy the old data */
  oldsize = GET_SIZE(HDRP(ptr));
  if(size<oldsize)
    oldsize =size;
  memcpy(newptr,ptr,oldsize);
  
  mm_free(ptr);
  return newptr;
}

/*
 * mm_checkheap - check the heap for correctness
 */
void mm_check(void){
  checkheap(1);//verbose =1;
}

/*
 * the remaining routines are internal healper routines
 */

/*
 * extend_heap - extend heap with free block and return its block pointer
 */
static void * extend_heap(size_t words){
  char *bp;
  size_t size;
  /* allocate an even number of words to maintain alignment */
  size =(words %2)? (words +1): words;
  size=size* WSIZE;
  if((long)(bp=mem_sbrk(size)) ==-1)
    return NULL;

  /* Initialize free block header/footer and the epilogue header */
  PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   //line:vm:mm:freeblockhdr
  PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   //line:vm:mm:freeblockftr
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ //line:vm:mm:newepihdr

  /* Coalesce if the previous block was free */
  return coalesce(bp);                                          //line:vm:mm:returnblock
}
/* $end mmextendheap */

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
/* $end mmplace-proto */
{
  size_t csize = GET_SIZE(HDRP(bp));   

  if ((csize - asize) >= (2*DSIZE)) { 
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize-asize, 0));
    PUT(FTRP(bp), PACK(csize-asize, 0));
  }
  else { 
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}
/* $end mmplace */

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
/* $begin mmfirstfit */
/* $begin mmfirstfit-proto */
static void *find_fit(size_t asize)
/* $end mmfirstfit-proto */
{
  /* $end mmfirstfit */

#ifdef NEXT_FIT 
  /* Next fit search */
  char *oldrover = rover;

  /* Search from the rover to the end of list */
  for ( ; GET_SIZE(HDRP(rover)) > 0; rover = NEXT_BLKP(rover))
    if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
      return rover;

  /* search from start of list to old rover */
  for (rover = heap_listp; rover < oldrover; rover = NEXT_BLKP(rover))
    if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
      return rover;

  return NULL;  /* no fit found */
#else 
  /* $begin mmfirstfit */
  /* First-fit search */
  void *bp;

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
      return bp;
    }
  }
  return NULL; /* No fit */
#endif
}
/* $end mmfirstfit */

static void printblock(void *bp) 
{
  size_t hsize, halloc, fsize, falloc;

  checkheap(0);
  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));  
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));  

  if (hsize == 0) {
    printf("%p: EOL\n", bp);
    return;
  }

  printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp, 
	 hsize, (halloc ? 'a' : 'f'), 
	 fsize, (falloc ? 'a' : 'f')); 
}

static void checkblock(void *bp) 
{
  if ((size_t)bp % 8)
    printf("Error: %p is not doubleword aligned\n", bp);
  if (GET(HDRP(bp)) != GET(FTRP(bp)))
    printf("Error: header does not match footer\n");
}

/* 
 * checkheap - Minimal check of the heap for consistency 
 */
void checkheap(int verbose) 
{
  char *bp = heap_listp;

  if (verbose)
    printf("Heap (%p):\n", heap_listp);

  if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
    printf("Bad prologue header\n");
  checkblock(heap_listp);

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (verbose) 
      printblock(bp);
    checkblock(bp);
  }

  if (verbose)
    printblock(bp);
  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
    printf("Bad epilogue header\n");
}










