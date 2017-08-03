#ifndef _FPE_H
#define _FPE_H

#include <jpeglib.h>

void
fpe_encrypt(unsigned char *key, unsigned char *nonce,
            JCOEF *data, int datalen,
            JCOEF minvalue, JCOEF maxvalue,
            int only_nonzero);

void
fpe_encrypt_all(unsigned char *key, unsigned char *nonce,
                JCOEF *data, int datalen,
                JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg);


void
fpe_encrypt_nonzero(unsigned char *key, unsigned char *nonce,
                    JCOEF *data, int datalen,
                    JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg);

void
fpe_decrypt(unsigned char *key, unsigned char *nonce,
            JCOEF *data, int datalen,
            JCOEF minvalue, JCOEF maxvalue,
            int only_nonzero);

void
fpe_decrypt_all(unsigned char *key, unsigned char *nonce,
                JCOEF *data, int datalen,
                JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg);

void
fpe_decrypt_nonzero(unsigned char *key, unsigned char *nonce,
                    JCOEF *data, int datalen,
                    JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg);


#endif
