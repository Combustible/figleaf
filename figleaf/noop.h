#ifndef _NOOP_H
#define _NOOP_H

#include <jpeglib.h>

void
noop_encrypt_block(unsigned char *key, unsigned char *block_nonce,
                   JCOEF *block, int blocklen,
                   JCOEF vmin, JCOEF vmax, int fcn_user_arg);


void
noop_decrypt_block(unsigned char *key, unsigned char *block_nonce,
                   JCOEF *block, int blocklen,
                   JCOEF vmin, JCOEF vmax, int fcn_user_arg);

#endif
