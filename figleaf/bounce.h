#ifndef _BOUNCE_H
#define _BOUNCE_H

#include <jpeglib.h>

JCOEF
bounce(JCOEF plaintext, JCOEF delta, JCOEF vmin, JCOEF vmax);


void
bounce_encrypt_block(unsigned char *key, unsigned char *nonce,
                     JCOEF *block, int blocklen,
                     JCOEF vmin, JCOEF vmax, int fcn_user_arg);

JCOEF
unbounce(JCOEF ciphertext, unsigned char *key);


void
bounce_decrypt_block(unsigned char *key, unsigned char *nonce,
                     JCOEF *block, int blocklen,
                     JCOEF vmin, JCOEF vmax, int fcn_user_arg);

#endif
