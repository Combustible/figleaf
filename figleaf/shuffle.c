#include <stdio.h>
#include <stdlib.h>
#include <sodium.h>

#include <jpeglib.h>
#include <jutil.h>
#include "fisheryates.h"
#include "shuffle.h"


void
shuffle_encrypt_block(unsigned char *key, unsigned char *nonce,
                      JCOEF *block, int blocklen,
                      JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  fisheryates_shuffle(key, nonce, block, blocklen);
}

void
shuffle_decrypt_block(unsigned char *key, unsigned char *nonce,
                      JCOEF *block, int blocklen,
                      JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  fisheryates_unshuffle(key, nonce, block, blocklen);
}






