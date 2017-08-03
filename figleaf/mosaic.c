#include <stdlib.h>
#include <stdio.h>

#include <jpeglib.h>
#include "mosaic.h"
#include "fpe.h"
#include "util.h"

void
mosaic_flatten_block(unsigned char *key, unsigned char *block_nonce,
                     JCOEF *block, int blocklen,
                     JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  // Replace every element with the average

  int i = 0;
  long sum = 0L;
  JCOEF avg = 0;

  for(i=0; i < blocklen; i++)
    sum += block[i];

  avg = (JCOEF) (sum * 1.0 / blocklen + 0.5);

  for(i=0; i < blocklen; i++)
    block[i] = avg;

}

void
mosaic_fuzzy_block(unsigned char *key, unsigned char *block_nonce,
                   JCOEF *block, int blocklen,
                   JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  // Generate pseudorandom values around the average

  // First flatten the block
  mosaic_flatten_block(key, block_nonce, block, blocklen, vmin, vmax, fcn_user_arg);

  int num_bits = fcn_user_arg;
  if(num_bits < 1) // We're done!
    return;

  // Then randomize the low-order bits
  // One easy way to code this is to re-use our FPE
  // encryption function to encrypt the lowest bits.
  JCOEF radius = 1 << (num_bits-1);
  JCOEF avg = block[0];  // Any element would do.  They're all == the average now.
  JCOEF tmp_min = max(-1024, avg - radius);
  JCOEF tmp_max = min( 1023, avg + radius);

  fpe_encrypt(key, block_nonce, block, blocklen, tmp_min, tmp_max, 0);
}

void
mosaic_zero_block(unsigned char *key, unsigned char *block_nonce,
                     JCOEF *block, int blocklen,
                     JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  // Set all elements to zero
  int i = 0;
  for(i=0; i < blocklen; i++)
    block[i] = 0;
}
