#include<stdio.h>
int howManyBits(int x) {
  int xx=x^(x<<1);
  int n=(!!(xx>>16))<<4;
  n+=(!!(xx>>(8+n)))<<3;
  n+=(!!(xx>>(4+n)))<<2;
  n+=(!!(xx>>(2+n)))<<1;
  n+=(!!(xx>>(1+n)));
  return n+1;
}

int main(){
  printf("how many bits for %x,%d\n",12,howManyBits(12));
  printf("how many bits for %x,%d\n",298,howManyBits(298));
  printf("how many bits for %x,%d\n",-5,howManyBits(-5));
  printf("how many bits for %x,%d\n",0,howManyBits(0));
  printf("how many bits for %x,%d\n",-1,howManyBits(-1));
  printf("how many bits for %x,%d\n",1<<31,howManyBits(1<<31));
  printf("how many bits for %x,%d\n",1<<30,howManyBits(1<<30));
  return 1;
}
