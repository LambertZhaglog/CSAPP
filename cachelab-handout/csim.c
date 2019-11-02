//lambert zhaglog
//HUST
#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
typedef int bool;
#define true 1
#define false 0
typedef struct{
  unsigned long int tag;
  bool valid;
  int tnu;//times not used every time this set searched
} line;

void usage();
int getSet(unsigned long int,int,int);
unsigned long int getTag(unsigned long int,int,int);
  
int main(int argc, char *argv[])
{
  bool verbose=false;
  bool help=false;
  
  //variable for global
  int bytew=-1;
  int setw=-1;
  int E=-1;//numbers of E per set, -1 for invalid
  char *filename=NULL;
  FILE *file=NULL;

  //parse the argument
  int ch;
  while((ch=getopt(argc,argv,"hvs:E:b:t:"))!=-1){
    switch(ch)
      {
      case 'h':
	help=true;
	break;
      case 'v':
	verbose=true;
	break;
      case 's':
	sscanf(optarg,"%d",&setw);
	break;
      case 'E':
	sscanf(optarg,"%d",&E);
	break;
      case 'b':
	sscanf(optarg,"%d",&bytew);
	break;
      case 't':
	filename=optarg;
	break;
      case '?':
      default:
	usage();
	return -1;
      }
  }	
  if(help==true){
    usage();
    return 0;
  }else{
    if(E<1||setw<0||bytew<1||filename==NULL){
      printf("Miss required command line argument or wrong argument\n");
      usage();
      return -1;
    }else{
      file=fopen(filename,"r");
      if(file==NULL){
	printf("Can't open file %s.",filename);
	return -1;
      }
    }
  }
  
  int S=1<<setw;//numbers of Set in this cache
  //  int B=1<<bytew;//block size in bytes

  line *cache=(line *)malloc(sizeof(line)*E*S);
  if(cache==NULL){
    printf("there is no enough memory to allocate for cache\n");
    return -1;
  }
  //first move the pointer cache to a set pointer and then use the fetch  method of fetch form
  int totolMiss=0;
  int totolEvict=0;
  int totolHit=0;
  char c2;
  while(feof(file)==0){
    // while(1){
    /* execute a new instruciton */
    int  address=0;
    int  width=0;
    fscanf(file," %c %x,%d ",&c2,&address,&width);
    /*
    if(feof(file)!=0){
          break;
    }
    */
    if(c2=='I'){
      continue;
    }
    /* search the target from  cache */
    int   set=getSet(address,setw,bytew);
    unsigned long int  tag=getTag(address,setw,bytew);
    // printf("set is %d, tag is %ld ! for test\n",set,tag);
    line *setCache=cache+set*E;
    int notused=-1;//-1 for invalid
    int lru=-1;//-1 for invalid
    bool find=false;
    for(int e=0;e<E;e++){
      if(setCache[e].valid==true){
	if(setCache[e].tag==tag){
	  find=true;
	  setCache[e].tnu=0;
	}else{
	  setCache[e].tnu++;
	  if(lru==-1){
	    lru=e;
	  }else{
	    if(setCache[e].tnu>setCache[lru].tnu){
	      lru=e;
	    }
	  }
	}
      }else{
	notused=e;
      }
    }
    /* update the the cnt of the miss hit eviction, update the cache */
    if(find==true){
      totolHit++;
    }else{
      totolMiss++;
      if(notused!=-1){
	setCache[notused].tag=tag;
	setCache[notused].tnu=0;
	setCache[notused].valid=true;
      }else{
	setCache[lru].tag=tag;
	setCache[lru].tnu=0;
	totolEvict++;
      }
    }
    if(c2=='M'){
      totolHit++;
    }
    /* handle the verbose mode */
    if(verbose==true){
      printf("%c %x,%d",c2,address,width);
      if(find==true){
	printf(" hit");
      }else{
	printf(" miss");
	if(notused==-1){
	  printf(" eviction");
	}
      }
      if(c2=='M'){
	printf(" hit");
      }
      printf("\n");
    }
  }
  printSummary(totolHit,totolMiss,totolEvict);
  fclose(file);//close the openned file
  // printf("close file successfully! for test");
  free(cache);//free the allocated memory
  // printf("free memory allocated successfully! for test");
  return 0;
}

//the helper function
int getSet(unsigned long int addr,int setw,int bytew){
  addr=addr>>bytew;
  int mask=(1<<setw)-1;
  return addr&mask;
}
unsigned long int getTag(unsigned long int addr,int setw,int bytew){
  return addr>>(setw+bytew);
}

void usage(){
  printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>]\n");
  printf("Options:\n");
  printf("  -h         Print this help message.\n");
  printf("  -v         Optional verbose flag.\n");
  printf("  -s <num>   Number of set index bits.\n");
  printf("  -E <num>   Number of lines per set.\n");
  printf("  -b <num>   Number of block offset bits.\n");
  printf("  -t <file>  Trace file.\n\n");
  printf("Examples:\n");
  printf("  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n");
  printf("  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}
