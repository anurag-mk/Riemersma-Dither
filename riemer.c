/* Riemersma dither
 *
 * This program reads in an uncompressed gray-scale image with one byte per
 * pixel and a size of 256*256 pixels (no image header). It dithers the image
 * and writes an output image in the same format.
 *
 * This program was tested with Borland C++ 3.1 16-bit (DOS), compiled in
 * large memory model. For other compilers, you may have to replace the
 * calls to _fmalloc() and _ffree() with straight malloc() and free() calls.
 */
#include <malloc.h>     /* for _fmalloc() and _ffree() */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH   512
#define HEIGHT  512

#define BLOCKSIZE 16384 // read in chunks of 16 kBytes

enum {
  NONE,
  UP,
  LEFT,
  DOWN,
  RIGHT,
};

/* variables needed for the Riemersma dither algorithm */
static int cur_x=0, cur_y=0;
static int img_width=0, img_height=0;
static unsigned char *img_ptr;

#define SIZE 16                 /* queue size: number of pixels remembered */
#define MAX  16                 /* relative weight of youngest pixel in the
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

void main(int argc,char *argv[])
{
  unsigned char *ptr;
  unsigned char *image;
  FILE *fp;
  long filesize;
  char filename[128];

  /* check arguments */
  if (argc<2 || argc>3) {
    printf("Usage: riemer <input> [output]\n\n"
           "Input and output files must be raw gray-scale "
           "files with a size of 256*256\n"
           "pixels; one byte per pixel.\n");
    return;
  } /* if */
  if ((fp=fopen(argv[1],"rb"))==NULL) {
    printf("File not found (%s)\n",argv[1]);
    return;
  } /* if */
  if ((image=malloc((long)WIDTH*HEIGHT))==NULL) {
    printf("Insufficient memory\n");
    return;
  } /* if */

  /* read in the file */
  filesize=(long)WIDTH*HEIGHT;
  ptr=image;
  while (filesize>BLOCKSIZE) {
    fread(ptr,1,BLOCKSIZE,fp);
    filesize-=BLOCKSIZE;
    ptr+=BLOCKSIZE;
  } /* while */
  if (filesize>0)
    fread(ptr,1,(int)filesize,fp);
  fclose(fp);

  /* dither (replacing the original */
  Riemersma(image,WIDTH,HEIGHT);

  /* save the dithered file */
  if (argc==3)
    strcpy(filename,argv[2]);
  else
    strcpy(filename,"riemer.raw");
  fp=fopen(filename,"wb");
  if (fp==NULL) {
    printf("Cannot create file \"%s\"\n",filename);
    return;
  } /* if */

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

  free(image);
}

