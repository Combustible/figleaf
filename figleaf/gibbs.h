#ifndef _GIBBS_H
#define _GIBBS_H

#include <jpeglib.h>

void
gibbs_encrypt_block(unsigned char *key, unsigned char *block_nonce,
                    JCOEF *block, int blocklen,
                    JCOEF vmin, JCOEF vmax, int fcn_user_arg);

void
gibbs_decrypt_block(unsigned char *key, unsigned char *block_nonce,
                    JCOEF *block, int blocklen,
                    JCOEF vmin, JCOEF vmax, int fcn_user_arg);
#endif
