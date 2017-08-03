#ifndef _LSB_H
#define _LSB_H

#include <stdint.h>
#include <jpeglib.h>

/**
 * If set to 1, the LSB modifications are distributed across the block
 * Otherwise LSB modifications proceed from top to bottom within the block
 */
#define LSB_DISTRIBUTE_ENABLED 1

/** The default number of bits to use to embed the thumbnail if not specified */
#define LSB_TPE_DEFAULT_NUM_BITS 2
void
lsb_encrypt_ac(unsigned char *key, unsigned char *nonce,
               JCOEF *data, int datalen,
               JCOEF minvalue, JCOEF maxvalue,
               int fcn_user_arg);

void
lsb_encrypt_dc(unsigned char *key, unsigned char *nonce,
               JCOEF *data, int datalen,
               JCOEF minvalue, JCOEF maxvalue,
               int fcn_user_arg);

void
lsb_decrypt_ac(unsigned char *key, unsigned char *nonce,
               JCOEF *data, int datalen,
               JCOEF minvalue, JCOEF maxvalue,
               int fcn_user_arg);

void
lsb_decrypt_dc(unsigned char *key, unsigned char *nonce,
               JCOEF *data, int datalen,
               JCOEF minvalue, JCOEF maxvalue,
               int fcn_user_arg);

static inline uint16_t
lsb_reverse_uint16(uint16_t val, int num_bits)
{
  uint16_t tmp = val & (~((1 << num_bits) - 1));
  for (int i = 0; i < num_bits; ++i) {
    tmp |= ((val >> i) & 0x01) << ((num_bits - 1) - i);
  }
  return tmp;
}

#endif
