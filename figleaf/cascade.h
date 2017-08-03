#ifndef _CASCADE_H
#define _CASCADE_H

#include <jpeglib.h>

void
cascade_encrypt_block(unsigned char *key, unsigned char *nonce,
                      JCOEF *block, int blocklen,
                      JCOEF vmin, JCOEF vmax, int fcn_user_arg);


void
cascade_decrypt_block(unsigned char *key, unsigned char *nonce,
                      JCOEF *block, int blocklen,
                      JCOEF vmin, JCOEF vmax, int fcn_user_arg);

#endif
