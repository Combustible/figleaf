#ifndef _DRPE_LSB_H
#define _DRPE_LSB_H

#include <jpeglib.h>

void
drpe_lsb_encrypt_all(unsigned char *key, unsigned char *nonce,
                     JCOEF *data, int datalen,
                     JCOEF minvalue, JCOEF maxvalue,
                     int num_lsb_bits);

void
drpe_lsb_decrypt_all(unsigned char *key, unsigned char *nonce,
                     JCOEF *data, int datalen,
                     JCOEF minvalue, JCOEF maxvalue,
                     int num_lsb_bits);


#endif
