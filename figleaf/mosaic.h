#ifndef _MOSAIC_H
#define _MOSAIC_H

#include <jpeglib.h>

void
mosaic_flatten_block(unsigned char *key, unsigned char *block_nonce,
                    JCOEF *block, int blocklen,
                    JCOEF vmin, JCOEF vmax, int fcn_user_arg);

void
mosaic_fuzzy_block(unsigned char *key, unsigned char *block_nonce,
                   JCOEF *block, int blocklen,
                   JCOEF vmin, JCOEF vmax, int fcn_user_arg);

void
mosaic_zero_block(unsigned char *key, unsigned char *block_nonce,
                  JCOEF *block, int blocklen,
                  JCOEF vmin, JCOEF vmax, int fcn_user_arg);

#endif
