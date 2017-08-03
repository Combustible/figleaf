#include <stdlib.h>
#include <stdio.h>

#include <jpeglib.h>
#include "noop.h"

void
noop_encrypt_block(unsigned char *key, unsigned char *block_nonce,
                   JCOEF *block, int blocklen,
                   JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  // Do nothing

  /*
  int i = 0;
  for(i=0; i < blocklen; i++)
    block[i] += 10;
  */
}


void
noop_decrypt_block(unsigned char *key, unsigned char *block_nonce,
                   JCOEF *block, int blocklen,
                   JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  // Do nothing

  /*
  int i = 0;
  for(i=0; i < blocklen; i++)
    block[i] -= 10;
  */
}
