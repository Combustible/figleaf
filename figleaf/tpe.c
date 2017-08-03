#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sodium.h>
#include <math.h>
#include <limits.h>

#include <jpeglib.h>
#include <jutil.h>

#include "util.h"
#include "figleaf.h"
#include "tpe.h"
#include "minmax.h"
#include "gibbs.h"

#define GIBBS_CRAZY_DEBUGGING 0

void
tpe_process_block(unsigned char *key,
                  struct jeasy *je, int color,
                  int xmin, int ymin, 
                  struct figleaf_context *ctx)
{
  int xmax = min(xmin + ctx->blocksize/8 - 1, je->width[color]-1);
  int ymax = min(ymin + ctx->blocksize/8 - 1, je->height[color]-1);
  int x = xmin;
  int y = ymin;
  int bw = xmax - xmin + 1;
  int bh = ymax - ymin + 1;
  int blocklen = bw * bh;
  JCOEF *block = (JCOEF *) malloc(blocklen * sizeof(JCOEF));
  //uint16_t *table = je->table[color]->quantval;

  //printf("Processing block at\tc=%d\ty=%d\tx=%d\n", color, ymin, xmin);
  //printf("\tymax=%d\txmax=%d\tblocklen = %d\n", ymax, xmax, blocklen);

  int freq=0;
  for(freq=0; freq < DCTSIZE2; freq++) {

    int i = 0;
    for(y=ymin; y <= ymax; y++) {
      for(x=xmin; x <= xmax; x++) {
        int jpeg_blocknum = y * je->width[color] + x;
        //printf("\t\tx = %d\ty = %d\tjb = %d\ti = %d\n", x, y, jpeg_blocknum, i);
        block[i++] = je->blocks[color][jpeg_blocknum][freq];
      }
    }

    /*
    if(freq==0) {
      printf("Block =\n");
      print_block(block);
    }
    */

    block_crypto_fcn crypt = NULL;
    JCOEF vmin = 0;
    JCOEF vmax = 0;
    JCOEF average = 0;

    if(freq > 0) { // These are AC coefficients
      // The AC coefficients should be roughly centered at zero
      minmax_set_debug(0);
      if( ctx->AC_minmax_fcn )
        ctx->AC_minmax_fcn(block, blocklen, 0, 10, &vmin, &vmax, &average);

      // And we'll use the given AC crypto function to en/de-crypt the block
      crypt = ctx->AC_crypto_fcn;
    }
    else { // freq == 0 -- these are the DC coefficients
      // The DC coefficients are probably not centered on zero

      minmax_set_debug(0);
      //minmax_poweroftwo(block, blocklen, 11, &vmin, &vmax, &average);
      //minmax_bitmask(block, blocklen, 1, 10, &vmin, &vmax, &average);
      if( ctx->DC_minmax_fcn )
        ctx->DC_minmax_fcn(block, blocklen, 0, 10, &vmin, &vmax, &average);
      //minmax_average_plusminus_poweroftwo(block, blocklen, 10, &vmin, &vmax, &average);
      //minmax_fixedbase_plus_poweroftwo(block, blocklen, 6, 11, &vmin, &vmax, &average);

      // And we'll use the given DC crypto function to en/de-crypt the block
      crypt = ctx->DC_crypto_fcn;
    }

    unsigned char nonce[crypto_stream_NONCEBYTES];
    int tweakinput[] = {color, x, y, freq};
    crypto_generichash(nonce, sizeof nonce, (unsigned char *) tweakinput, sizeof tweakinput, NULL, 0);

    crypt(key, nonce, block, blocklen, vmin, vmax, ctx->fcn_user_arg);

    // Finally, put the encrypted values back into the JPEG structure
    i = 0;
    for(y=ymin; y <= ymax; y++) {
      for(x=xmin; x <= xmax; x++) {
        int jpeg_blocknum = y * je->width[color] + x;

        if(freq == 0 && ((block[i] < -1024) || (block[i] > 1023))) {
          printf("WTF? x=%4d y=%4d\tblock[%2d] = %hd\n", x, y, i, block[i]);
        }

        je->blocks[color][jpeg_blocknum][freq] = block[i++];
      }
    }

  }

  free(block);

} // end tpe_process_block()


// Process the entire image, one block at a time
void
tpe_process_image(unsigned char *key,
                  struct jeasy *je,
                  struct figleaf_context *ctx)
{
  // Work on each color component c (ie c is either Y, Cb, or Cr)
  int c = 0;
  for(c=0; c < je->comp; c++)
  {
    //printf("Color component %d has %d x %d blocks\n", c, je->width[c], je->height[c]);

    /*
    printf("Quantization table:\n");
    short *table = je->table[c]->quantval;
    print_block(table);
    printf("\n");
    */

    int y;
    for(y=0; y < je->height[c]; y += ctx->blocksize/8) {
      int x;
      for(x=0; x < je->width[c]; x += ctx->blocksize/8) {
        tpe_process_block(key, je, c, x, y, ctx);
      }
    }


  } // end for c



}


