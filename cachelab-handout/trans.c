/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
/*
 * author: lambert zhaglog
 * institute: HUST
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
   int i, j;

   int ii,jj;
   for(ii=0;ii<N;ii=ii+8){
     for(jj=0;jj<M;jj=jj+8){
       if(ii!=jj){
	 for(i=0;i<8;i++){
	   for(j=0;j<8;j++){
	     B[jj+j][ii+i]=A[ii+i][jj+j];
	   }
	 }
       }else{
	 continue;
       }
     }
   }
   for(ii=0;ii<N-8;ii=ii+8){
     for(i=0;i<8;i++){
       for(j=0;j<8;j++){
	 B[ii+j+8][ii+i+8]=A[ii+i][ii+j];
       }
     }
   }
   for(ii=0;ii<N-8;ii=ii+8){
     for(i=0;i<8;i++){
       for(j=0;j<8;j++){
	 B[ii+i][ii+j]=B[ii+i+8][ii+j+8];
       }
     }
   }
   for(ii=N-8;ii<N;ii=ii+8){
     for(i=0;i<8;i++){
       for(j=0;j<8;j++){
	 B[ii+j][ii+i]=A[ii+i][ii+j];
       }
     }
   }

}
/* 
 * Step 1: Validating and generating memory traces
 * Step 2: Evaluating performance (s=5, E=1, b=5)
 * func 0 (Transpose submission): hits:2140, misses:297, evictions:265

 */
char transpose_block8x8_improve_desc[] = "block 8x8 improved for matrix 32x32";
void transpose_block8x8_imporve(int M, int N, int A[N][M], int B[M][N])
{
   int i, j;

   int ii,jj;
   for(ii=0;ii<N;ii=ii+8){
     for(jj=0;jj<M;jj=jj+8){
       if(ii!=jj){
	 for(i=0;i<8;i++){
	   for(j=0;j<8;j++){
	     B[jj+j][ii+i]=A[ii+i][jj+j];
	   }
	 }
       }else{
	 continue;
       }
     }
   }
   for(ii=0;ii<N-8;ii=ii+8){
     for(i=0;i<8;i++){
       for(j=0;j<8;j++){
	 B[ii+j+8][ii+i+8]=A[ii+i][ii+j];
       }
     }
   }
   for(ii=0;ii<N-8;ii=ii+8){
     for(i=0;i<8;i++){
       for(j=0;j<8;j++){
	 B[ii+i][ii+j]=B[ii+i+8][ii+j+8];
       }
     }
   }
   for(ii=N-8;ii<N;ii=ii+8){
     for(i=0;i<8;i++){
       for(j=0;j<8;j++){
	 B[ii+j][ii+i]=A[ii+i][ii+j];
       }
     }
   }

}
/*
 * block8x8 for matrix 32x32
 * Step 1: Validating and generating memory traces
 * Step 2: Evaluating performance (s=5, E=1, b=5)
 * func 0 (Transpose submission): hits:1710, misses:343, evictions:311
 */

char transpose_block8x8_desc[] = "block 8x8 for 32x32";
void transpose_block8x8(int M, int N, int A[N][M], int B[M][N])
{
   int i, j;

   int ii,jj;
   for(ii=0;ii<N;ii=ii+8){
     for(jj=0;jj<M;jj=jj+8){
       for(i=0;i<8;i++){
	 for(j=0;j<8;j++){
	   B[jj+j][ii+i]=A[ii+i][jj+j];

	 }
       }
     }
   }

}
 
/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    //  registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}
