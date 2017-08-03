#ifndef _SHUFFLE_H
#define _SHUFFLE_H

#include <jpeglib.h>

void
shuffle_encrypt_block(unsigned char *key, unsigned char *nonce,
                      JCOEF *block, int blocklen,
                      JCOEF vmin, JCOEF vmax, int fcn_user_arg);

void
shuffle_decrypt_block(unsigned char *key, unsigned char *nonce,
                      JCOEF *block, int blocklen,
                      JCOEF vmin, JCOEF vmax, int fcn_user_arg);

#endif
