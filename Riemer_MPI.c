#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>
#include <malloc.h>     /* for _fmalloc() and _ffree() */
#include <math.h>

#define WIDTH 512
#define HEIGHT 512

#define BLOCKSIZE 16384       /* weights for the errors of recent pixels */

enum {
  NONE,
  UP,
  LEFT,
  DOWN,
  RIGHT,
};

static int cur_x=0, cur_y=0;
static int img_width=0, img_height=0;
static unsigned char *img_ptr;

#define SIZE 32              /* queue size: number of pixels remembered */
#define MAX  32                 /* relative weight of youngest pixel in the
                                 * queue, versus the oldest pixel */

static int weights[SIZE];       /* weights for the errors of recent pixels */


int max(int a, int b){
  return a>b?a:b;
}
static void init_weights(int a[],int size,int max)
{
  double m = exp(log(max)/(size-1));
  //double m=2;
  double v;
  int i;

  for (i=0, v=1.0; i<size; i++) {
    a[i]=(int)(v+0.5);  /* store rounded value */
    v*=m;               /* next value */
  } /*for */
}

static void dither_pixel(unsigned char *pixel)
{
static int error[SIZE]; /* queue with error values of recent pixels */
  int i,pvalue,err;

  for (i=0,err=0L; i<SIZE; i++)
    err+=error[i]*weights[i];
  pvalue=*pixel + err/MAX;
  pvalue = (pvalue>=128) ? 255 : 0;
  memmove(error,error+1,(SIZE-1)*sizeof error[0]);    /* shift queue */
  error[SIZE-1] = *pixel - pvalue;
  *pixel=(unsigned char)pvalue;
}

static void move(int direction)
{
  /* dither the current pixel */
  if (cur_x>=0 && cur_x<img_width && cur_y>=0 && cur_y<img_height)
    dither_pixel(img_ptr);

  /* move to the next pixel */
  switch (direction) {
  case LEFT:
    cur_x--;
    img_ptr--;
    break;
  case RIGHT:
    cur_x++;
    img_ptr++;
    break;
  case UP:
    cur_y--;
    img_ptr-=img_width;
    break;
  case DOWN:
    cur_y++;
    img_ptr+=img_width;
    break;
  } /* switch */
}

void hilbert_level(int level,int direction)
{
  if (level==1) {
    switch (direction) {
    case LEFT:
      move(RIGHT);
      move(DOWN);
      move(LEFT);
      break;
    case RIGHT:
      move(LEFT);
      move(UP);
      move(RIGHT);
      break;
    case UP:
      move(DOWN);
      move(RIGHT);
      move(UP);
      break;
    case DOWN:
      move(UP);
      move(LEFT);
      move(DOWN);
      break;
    } /* switch */
  } else {
    switch (direction) {
    case LEFT:
      hilbert_level(level-1,UP);
      move(RIGHT);
      hilbert_level(level-1,LEFT);
      move(DOWN);
      hilbert_level(level-1,LEFT);
      move(LEFT);
      hilbert_level(level-1,DOWN);
      break;
    case RIGHT:
      hilbert_level(level-1,DOWN);
      move(LEFT);
      hilbert_level(level-1,RIGHT);
      move(UP);
      hilbert_level(level-1,RIGHT);
      move(RIGHT);
      hilbert_level(level-1,UP);
      break;
    case UP:
      hilbert_level(level-1,LEFT);
      move(DOWN);
      hilbert_level(level-1,UP);
      move(RIGHT);
      hilbert_level(level-1,UP);
      move(UP);
      hilbert_level(level-1,RIGHT);
      break;
    case DOWN:
      hilbert_level(level-1,RIGHT);
      move(UP);
      hilbert_level(level-1,DOWN);
      move(LEFT);
      hilbert_level(level-1,DOWN);
      move(DOWN);
      hilbert_level(level-1,LEFT);
      break;
    } /* switch */
  } /* if */
}

int logb2(int value)
{
  int result=0;
  while (value>1) {
    value >>= 1;
    result++;
  } /*while */
  return result;
}

void Riemersma(void *image,int width,int height)
{
  int level,size;

  /* determine the required order of the Hilbert curve */
  size=max(width,height);
  level=logb2(size);  
  if ((1L << level) < size)
    level++;

  init_weights(weights,SIZE,MAX);
  img_ptr=image;
  img_width=width;
  img_height=height;
  cur_x=0;
  cur_y=0;
  if (level>0)
    hilbert_level(level,UP);
  move(NONE);
}

int main(int argc, char* argv[]){
  int rank,size;
  unsigned char *ptr;
  unsigned char *process_ptr;
  unsigned char *image;
  FILE *fp;
  long filesize;
  long process_size;
  char filename[128];
  filesize=(long)WIDTH*HEIGHT;
  ptr=image;
  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  printf("rank--> %d\n",rank);
  int dim=sqrt(size);
  if(rank==0){
    if (argc<2 || argc>3) {
    printf("Usage: riemer <input> [output]\n\n"
           "Input and output files must be raw gray-scale "
           "files with a size of 256*256\n"
           "pixels; one byte per pixel.\n");
    MPI_Abort(MPI_COMM_WORLD,911);
    } /* if */
    if ((fp=fopen(argv[1],"rb"))==NULL) {
      printf("File not found (%s)\n",argv[1]);
      MPI_Abort(MPI_COMM_WORLD,911);
    } /* if */
    if ((image=malloc((long)WIDTH*HEIGHT))==NULL) {
      printf("Insufficient memory\n");
      MPI_Abort(MPI_COMM_WORLD,911);
    } /* if */
    /* read in the file */
    filesize=(long)WIDTH*HEIGHT;
    process_size=filesize/size;
    ptr=image;
    while (filesize>BLOCKSIZE) {
      fread(ptr,1,BLOCKSIZE,fp);
      filesize-=BLOCKSIZE;
      ptr+=BLOCKSIZE;
    } /* while */
    if (filesize>0)
      fread(ptr,1,(int)filesize,fp);
    fclose(fp);
  }
  MPI_Bcast(&process_size,1,MPI_LONG,0,MPI_COMM_WORLD);
  process_ptr = malloc(sizeof(char)*process_size);  
  MPI_Scatter(image,process_size,MPI_CHAR,process_ptr,process_size,MPI_CHAR,0,MPI_COMM_WORLD);
  Riemersma(process_ptr,WIDTH/dim,HEIGHT/dim);
  MPI_Gather(process_ptr,process_size,MPI_CHAR,image,process_size,MPI_CHAR,0,MPI_COMM_WORLD);
  if(rank==0){
    fp=fopen("output.raw","wb");
    filesize=(long)WIDTH*HEIGHT;
    ptr=image;
    while (filesize>BLOCKSIZE) {
      fwrite(ptr,1,BLOCKSIZE,fp);
      filesize-=BLOCKSIZE;
      ptr+=BLOCKSIZE;
    } /* while */
    if (filesize>0)
      fwrite(ptr,1,(int)filesize,fp);
    fclose(fp);
  }
  MPI_Finalize();
  return 0;
}