/*
 * mm-book.c - The malloc package that copy from book CSAPP, section 9.9.12.
 * 
 * Simple, 32-bit and 64-bit clean allocator based on segregate list and use
 * explict double list to recode the free block. Use small-first, best-fit  
 * placement, and boundary tag coalescing, as describled 
 * int the CS:APP3e text. Block must be aligned to doubleword (8 byte)
 * boundaries. Minimum block size is 16 byte.
 * As for the bin set, we define the bin size as 16,24,32,...120,128,256,512,
 * ... 4096,+inf
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
//#define CHUNKSIZE (1<<12) /*Extend heap by this amount (bytes) */
#define CHUNKSIZE ((1<<12)+8)

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

/* Given a free block ptr fp, fp indicate to the predecessor followed by 
 * successor, compute the address of the predecessor and successor */
#define PRED(fp) (fp)
#define SUCC(fp) ((char *)(fp) + WSIZE)

#define DEBUG 0
/* $end mallocmacros */

/* Global variables */
static char *heap_listp =0; /* Pointer to first block */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkheap();
static void checkblock(void *bp);
static void *get_head(size_t asize);

/*
 * get_head - get the asize corresponding free list header, return its address
 */

void *get_head(size_t asize){
  int k=0;
  if(asize<=16){
    k=2;
  }else if(asize<=128){
    k=asize/8+(!!(asize%8));
  }else if(asize<=256){
    k=17;
  }else if(asize<=512){
    k=18;
  }else if(asize<=1024){
    k=19;
  }else if(asize<=2048){
    k=20;
  }else if(asize<=4096){
    k=21;
  }else{
    k=22;
  }
  return heap_listp+(k-2)*WSIZE;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  /* Create the initial empty heap */
  if((heap_listp = mem_sbrk(24*WSIZE)) == (void *)-1)
    return -1;
  for(int i=0;i<21;i++){ // initialize the link header all null
    PUT(heap_listp+(i*WSIZE),NULL);
  }
  PUT(heap_listp+(21*WSIZE),PACK(DSIZE,1)); /* Prologue header */
  PUT(heap_listp+(22*WSIZE),PACK(DSIZE,1)); /* Prologue footer */
  PUT(heap_listp+(23*WSIZE),PACK(0,1)); /* Epilogue header */

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
#if DEBUG
  printf(" malloc size = %d \n",size);
  checkheap();
#endif
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
  /* old verion 
     /* Search the free list for a fit *
     if((bp=find_fit(asize))!=NULL){
     place(bp,asize);
     return bp;
     }
     /* Here I can implement my new stratege */
  // to do for next version
  
  /* No fit found. Get more memory and place the block *
     extendsize=MAX(asize,CHUNKSIZE);
     if((bp=extend_heap(extendsize/WSIZE))==NULL)
     return NULL;
     place(bp,asize);
     return bp;
  */

  /* new version use preallocate method */
  if((bp=find_fit(asize))==NULL){
    extendsize=MAX(asize,CHUNKSIZE);
    if((bp=extend_heap(extendsize/WSIZE))==NULL)
      return NULL;
  }
  if(GET_SIZE(HDRP(bp))>=2*asize){
    size_t csize=GET(HDRP(bp));
    /* update the header */
    place(bp,csize);// remove this block from list
    // add prealloc space
    PUT(HDRP(bp),PACK(asize,0));
    PUT(FTRP(bp),PACK(asize,0));
    bp=NEXT_BLKP(bp);
    PUT(HDRP(bp),PACK(csize-asize,1));
    PUT(FTRP(bp),PACK(csize-asize,1));
    coalesce(PREV_BLKP(bp));//mount preallocate block to the link list
    
    csize=GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2*DSIZE)) { 
      PUT(HDRP(bp), PACK(asize, 1));
      PUT(FTRP(bp), PACK(asize, 1)); 
      bp = NEXT_BLKP(bp);
      PUT(HDRP(bp), PACK(csize-asize, 0));
      PUT(FTRP(bp), PACK(csize-asize, 0));
      coalesce(bp);
      bp=PREV_BLKP(bp);
    }
    else { 
      PUT(HDRP(bp), PACK(csize, 1));
      PUT(FTRP(bp), PACK(csize, 1));
    }
    return bp;
    
  }else{
    place(bp,asize);
    return bp;
  }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
#if DEBUG
  printf(" free addr = %x\n",ptr);
  checkheap();
#endif
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
 * this function is responsible to set the free block in the right link list 
 */
static void *coalesce(void *bp){
#if DEBUG
  printf(" coalesce addr = %x\n",bp);
  checkheap();
#endif
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size =GET_SIZE(HDRP(bp));

  char *lh=NULL; //link list header

  if(prev_alloc && next_alloc){ /* case 1*/
    lh=get_head(size);
    PUT(SUCC(bp), GET(lh));
    PUT(PRED(bp),NULL);
    if(GET(lh)!=NULL){
      PUT(PRED(GET(lh)),bp);
    }
    PUT(lh,bp);
  }else if( prev_alloc && !next_alloc){ /* case 2 */
    //remove the coalesced free block from correspond old list
    void *tmpaddr=NEXT_BLKP(bp);
    int tmpsize=GET_SIZE(HDRP(tmpaddr));
    lh=get_head(tmpsize);
    char *tmpp;
    if(GET(lh)==tmpaddr){
      PUT(lh,GET(SUCC(tmpaddr)));
      if(GET(SUCC(tmpaddr))!=NULL){
	PUT(PRED(GET(SUCC(tmpaddr))),NULL);
      }
    }else{
      for(tmpp=(char *)GET(lh);(GET(SUCC(tmpp))!=NULL)
	    &&(GET(SUCC(tmpp))!=tmpaddr);tmpp=GET(SUCC(tmpp))){}
      if(GET(SUCC(tmpp))==NULL){
	printf("error from coalesce case 2\n");
      }else{
	PUT(SUCC(tmpp),GET(SUCC(tmpaddr)));
	if(GET(SUCC(tmpaddr))!=NULL){
	  PUT(PRED(GET(SUCC(tmpaddr))),tmpp);
	}
      }
    }
    //update the free block boundary and size
    size =size+ GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));// it's right, because ftrp depend oon hdrp

    // update the new size list
    lh=get_head(size);
    PUT(SUCC(bp),GET(lh));
    PUT(PRED(bp),NULL);
    if(GET(lh)!=NULL){
      PUT(PRED(GET(lh)),bp);
    }
    PUT(lh,bp);
  }else if (!prev_alloc && next_alloc){ /* case 3 */
    //remove the coalesced free block from correspond old list
    void *tmpaddr=PREV_BLKP(bp);
    int tmpsize=GET_SIZE(HDRP(tmpaddr));
    lh=get_head(tmpsize);
    char *tmpp;
    if((void *)GET(lh)==tmpaddr){
      PUT(lh,GET(SUCC(tmpaddr)));
      if(GET(SUCC(tmpaddr))!=NULL){
	PUT(PRED(GET(SUCC(tmpaddr))),NULL);
      }
    }else{
      for(tmpp=(char *)GET(lh);(GET(SUCC(tmpp))!=NULL)
	    &&(GET(SUCC(tmpp))!=tmpaddr);tmpp=GET(SUCC(tmpp))){}
      if(GET(SUCC(tmpp))==NULL){
	printf("error from coalesce case 2\n");
      }else{
	PUT(SUCC(tmpp),GET(SUCC(tmpaddr)));
	if(GET(SUCC(tmpaddr))!=NULL){
	  PUT(PRED(GET(SUCC(tmpaddr))),tmpp);
	}
      }
    }
    //update the free block boundary and size
    size=size+ GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    bp=PREV_BLKP(bp);
    //update the new size list
    lh=get_head(size);
    PUT(SUCC(bp),GET(lh));
    PUT(PRED(bp),NULL);
    if(GET(lh)!=NULL){
      PUT(PRED(GET(lh)),bp);
    }
    PUT(lh,bp);  
  }else{  /* case 4 */
    //remove the coalesced free block from correspond old list
    void *tmpaddr=PREV_BLKP(bp);
    int tmpsize=GET_SIZE(HDRP(tmpaddr));
    lh=get_head(tmpsize);
    char *tmpp;
    if(GET(lh)==tmpaddr){
      PUT(lh,GET(SUCC(tmpaddr)));
      if(GET(SUCC(tmpaddr))!=NULL){
	PUT(PRED(GET(SUCC(tmpaddr))),NULL);
      }
    }else{
      for(tmpp=(char *)GET(lh);(GET(SUCC(tmpp))!=NULL)
	    &&(GET(SUCC(tmpp))!=tmpaddr);tmpp=GET(SUCC(tmpp))){}
      if(GET(SUCC(tmpp))==NULL){
	printf("error from coalesce case 2\n");
      }else{
	PUT(SUCC(tmpp),GET(SUCC(tmpaddr)));
	if(GET(SUCC(tmpaddr))!=NULL){
	  PUT(PRED(GET(SUCC(tmpaddr))),tmpp);
	}
      }
    }
    //remove once again
    tmpaddr=NEXT_BLKP(bp);
    tmpsize=GET_SIZE(HDRP(tmpaddr));
    lh=get_head(tmpsize);
    if(GET(lh)==tmpaddr){
      PUT(lh,GET(SUCC(tmpaddr)));
      if(GET(SUCC(tmpaddr))!=NULL){
	PUT(PRED(GET(SUCC(tmpaddr))),NULL);
      }
    }else{
      for(tmpp=(char *)GET(lh);(GET(SUCC(tmpp))!=NULL)
	    &&(GET(SUCC(tmpp))!=tmpaddr);tmpp=GET(SUCC(tmpp))){}
      if(GET(SUCC(tmpp))==NULL){
	printf("error from coalesce case 2\n");
      }else{
	PUT(SUCC(tmpp),GET(SUCC(tmpaddr)));
	if(GET(SUCC(tmpaddr))!=NULL){
	  PUT(PRED(GET(SUCC(tmpaddr))),tmpp);
	}
      }
    }
    //update free block buandary and size
    size=size+GET_SIZE(HDRP(PREV_BLKP(bp))) +
      GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
    bp=PREV_BLKP(bp);
    //update the new size list
    lh=get_head(size);
    PUT(SUCC(bp),GET(lh));
    PUT(PRED(bp),NULL);
    if(GET(lh)!=NULL){
      PUT(PRED(GET(lh)),bp);
    }
    PUT(lh,bp);      
  }
  return bp;
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
  void *newptr;
  size_t oldsize;
  //  printf(" realloc addr = %x to size = %d\n",ptr,size);
  //  checkheap();
  /* If size ==0 then this is just free, and we return NULL */
  if(size==0){
    mm_free(ptr);
    return 0;
  }

  /* if oldptr is NULL, then this is just malloc. */
  if(ptr ==NULL){
    return mm_malloc(size);
  }

  /* old version 
     newptr=mm_malloc(size);

     /* if reallloc fails the original block if left untouched *
     if(!newptr){
     return 0;
     }

     /* Copy the old data *
     oldsize = GET_SIZE(HDRP(ptr));
     if(size<oldsize)
     oldsize =size;
     memcpy(newptr,ptr,oldsize);
  
     mm_free(ptr);
     return newptr;
  */
  oldsize=GET_SIZE(HDRP(ptr));
  size_t asize=DSIZE*((size+(DSIZE)+(DSIZE-1))/DSIZE);
  if(asize<=oldsize){//realloc try to reduce the allocate space
    //checked
    if(oldsize-size>=2*DSIZE){
      PUT(HDRP(ptr),PACK(asize,1));
      PUT(FTRP(ptr),PACK(asize,1));
      void *bp=NEXT_BLKP(ptr);
      PUT(HDRP(bp),PACK(oldsize-asize,0));
      PUT(FTRP(bp),PACK(oldsize-asize,0));
      coalesce(bp);
    }else{
      //do nothing
    }
    return ptr;
  }else{
    size_t totalsize=GET_ALLOC(HDRP(PREV_BLKP(ptr)))==0?
      GET_SIZE(HDRP(PREV_BLKP(ptr))):0;
    totalsize+=GET_ALLOC(HDRP(NEXT_BLKP(ptr)))==0?
      GET_SIZE(HDRP(NEXT_BLKP(ptr))):0;
    totalsize+=oldsize;
    //   printf("total size is %d\n",totalsize);
    if(asize<=totalsize){
      if(GET_ALLOC(HDRP(PREV_BLKP(ptr)))==0){
	newptr=PREV_BLKP(ptr);
      }else{
	newptr=ptr;
      }
      unsigned int pred=GET(ptr);
      unsigned int succ=GET(SUCC(ptr));
      //     printf("tag 1\n");
      mm_free(ptr);
      if(totalsize-asize>=2*DSIZE){
	// split
	place(newptr,totalsize);
	PUT(HDRP(newptr), PACK(asize, 1));
	memmove(newptr,ptr,oldsize-2*WSIZE);
	PUT(FTRP(newptr), PACK(asize, 1)); 
	PUT(PRED(newptr),pred);
	PUT(SUCC(newptr),succ);
	void *bp = NEXT_BLKP(newptr);
	PUT(HDRP(bp), PACK(totalsize-asize, 0));
	PUT(FTRP(bp), PACK(totalsize-asize, 0));
	//	printf("tag 2\n");
	coalesce(bp);
      }else{
	place(newptr,totalsize);
	memmove(newptr,ptr,oldsize-0*WSIZE);
	PUT(PRED(newptr),pred);
	PUT(SUCC(newptr),succ);
      }
      //      printf("after realloc\n");
      //     checkheap();
      return newptr;
    }else{// the neibors space not enough to realloc 
      newptr=mm_malloc(size);
      if(!newptr){
	return 0;
      }
      memcpy(newptr,ptr,oldsize-0*WSIZE);
      mm_free(ptr);
      return newptr;
    }
  }
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
  /* update the header */
  void *lh=get_head(csize);
  char *tmpp;
  if((void *)GET(lh)==bp){
    PUT(lh,GET(SUCC(bp)));
    if(GET(SUCC(bp))!=NULL){
      PUT(PRED(GET(SUCC(bp))),NULL);
    }
  }else{
    for(tmpp=(char *)GET(lh);(GET(SUCC(tmpp))!=NULL)
	  &&(GET(SUCC(tmpp))!=bp);tmpp=GET(SUCC(tmpp))){}
    if(GET(SUCC(tmpp))==NULL){
      printf("error from coalesce case 2\n");
    }else{
      PUT(SUCC(tmpp),GET(SUCC(bp)));
      if(GET(SUCC(bp))!=NULL){
	PUT(PRED(GET(SUCC(bp))),tmpp);
      }
    }
  }  
  if ((csize - asize) >= (2*DSIZE)) { 
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1)); 
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize-asize, 0));
    PUT(FTRP(bp), PACK(csize-asize, 0));
    coalesce(bp);
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
  int *lh=get_head(asize);
  /* small - fisrt */
  for(;(int)lh<(int)((char*)heap_listp+21*WSIZE);lh++){
    if(GET(lh)!=NULL){
      int *p;
      for(p=GET(lh);p!=NULL;p=GET(SUCC(p))){
	if(GET_SIZE(HDRP(p))>=asize){
	  break;
	}
      }
      /* best fit */
      if(p!=NULL){
	int *q;
	for(q=GET(SUCC(p));q!=NULL;q=GET(SUCC(q))){
	  if((GET_SIZE(HDRP(q))>=asize)&&(GET_SIZE(HDRP(q))<GET_SIZE(HDRP(p)))){
	    p=q;
	  }
	}
	return p;
      }
    }
  }
  return NULL;
}
/* $end mmfirstfit */

static void printblock(void *bp) 
{
  size_t hsize, halloc, fsize, falloc;

  // checkheap(0);
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
void checkheap() 
{
  char *bp = heap_listp;
  int verbose=1;
  /*
    if (verbose)
    printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
    printf("Bad prologue header\n");
    checkblock(heap_listp);
  */

  for (bp =(char *)heap_listp+24*WSIZE; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (verbose) 
      printblock(bp);
    checkblock(bp);
  }

  if (verbose)
    printblock(bp);
  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
    printf("Bad epilogue header\n");
}










