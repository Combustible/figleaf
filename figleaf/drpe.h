#ifndef DRPE_H
#define DRPE_H

/* Dynamic Range Preserving Encryption (DRPE)
 * (pronounced "Derpy")
 *
 * This file defines functions for encrypting only
 * the low-order bits that vary, and for decrypting
 * to recover the original values.  In either case,
 * the most significant bits, which are the same in
 * all elements of the input array, are left alone.
 *
 */

#include <stdint.h>
#include <jpeglib.h>

void
drpe_encrypt_decrypt(unsigned char *key, unsigned char *nonce,
                     JCOEF *data, int datalen,
                     JCOEF minvalue, JCOEF maxvalue,
                     int only_nonzero);

void
drpe_encrypt_decrypt_all(unsigned char *key, unsigned char *nonce,
                         JCOEF *data, int datalen,
                         JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg);

void
drpe_encrypt_decrypt_nonzero(unsigned char *key, unsigned char *nonce,
                             JCOEF *data, int datalen,
                             JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg);

int
drpe_calc_numbits(JCOEF minvalue, JCOEF maxvalue,
                  unsigned short *bitmask,
                  int *sign_bit_position);

JCOEF drpe_fixup_sign_bit(uint16_t damaged_sign_ciphertext, int sign_bit_position);

#endif
