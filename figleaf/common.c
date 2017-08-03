#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include <jpeglib.h>
#include <jutil.h>

#include "common.h"


JCOEF*
get_DCT_coefficients_by_freq(struct jeasy *je, int color, int freq)
{
  if( !(freq < DCTSIZE2) )
    return NULL;

  JCOEF *buf = (JCOEF *) malloc(je->width[color] * je->height[color] * sizeof(JCOEF));
  if(buf == NULL)
    return NULL;

  // Look at each block for component c
  // x,y are the spatial coordinates of the block
  int y = 0;
  int b = 0;
  for(y=0; y < je->height[color]; y++) {
    int x = 0;
    for(x=0; x < je->width[color]; x++) {
      int blocknum = y * je->width[color] + x;
      // DCT coefficients are in je->blocks[c][blocknum]
      // There are DCTSIZE2 of them
      // The DC coefficient is the first one in each block
      // The rest are AC coefficients
      JCOEF coef = je->blocks[color][blocknum][freq];
      // Copy the DC coefficient into our new temporary buffer
      //buf[b++] = (JCOEF) (dc+128);
      buf[b++] = coef;
    } // end for x
  } // end for y

  return buf;
}


JCOEF*
get_DC_coefficients(struct jeasy *je, int color)
{
  return get_DCT_coefficients_by_freq(je, color, 0);
}

void
set_DCT_coefficients_by_freq(struct jeasy *je, int color, int freq, JCOEF *buf)
{
  if(buf == NULL)
    err(1,"Tried to set DCT coefficients to NULL");

  if( !(freq < DCTSIZE2) )
    err(1, "Tried to set invalid DCT coefficent");

  int y = 0;
  int b = 0;
  for(y=0; y < je->height[color]; y++) {
    int x = 0;
    for(x=0; x < je->width[color]; x++) {
      int blocknum = y * je->width[color] + x;
      je->blocks[color][blocknum][freq] = (JCOEF)buf[b++];
    } // end for x
  } // end for y
}

void
set_DC_coefficients(struct jeasy *je, int color, JCOEF *buf)
{
  set_DCT_coefficients_by_freq(je, color, 0, buf);
}





void
print_coefs(JCOEF *buf, int width, int height)
{
  int x, y;
  int i=0;
  for(y=0; y<height; y++) {
    for(x=0; x<width; x++) {
      printf("%3d ", buf[i++]);
    }
    printf("\n");
  }
}


