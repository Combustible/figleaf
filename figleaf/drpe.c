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

#include <err.h>
#include <sodium.h>
#include <stdbool.h>

#include <jpeglib.h>
#include "util.h"
#include "drpe.h"
#include "minmax.h"

#define DRPE_DEBUG 0

#if DRPE_DEBUG
#define DEBUG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...)
#endif

/*
 * Gets the least-significant bit b of value x
 * For example, b = 0 gets the least-significant bit,
 * b = 3 is the 4th least-significant bit
 */
#define GET_BIT(x,b) \
  (((x) >> ((b)-1)) & 0x01)

/*
 * Returns the number of bits that need to be encrypted
 * (those that do not share the same prefix)
 *
 * sign_bit_position is set to the 0-based position of
 * the bit that stores the sign value
 */
int
drpe_calc_numbits(JCOEF minvalue, JCOEF maxvalue,
                  unsigned short *bitmask,
                  int *sign_bit_position)
{
  int num_bits = 0;
  if (minvalue == maxvalue) {
    num_bits = 0;
    *bitmask = 0;
    *sign_bit_position = 0;
  } else if ((minvalue >= 0 && maxvalue >= 0) || (minvalue < 0 && maxvalue < 0)) {
    // Sign bits are the same.
    // So we simply count up how many LSB's are different.
    num_bits = 16;
    ASSERT(sizeof(JCOEF) == 2);
    //printf("min = %4hd\tmax = %4hd\n", minvalue, maxvalue);
    while (num_bits > 0 && (GET_BIT(minvalue, num_bits) == GET_BIT(maxvalue, num_bits))) {
      num_bits -= 1;
    }

    /*
     * __builtin_clrsb()  returns the number of redundant sign bits for an integer
     * Thus this line determines which bit is the last sign bit in the data
     */
    *sign_bit_position = 31 - min(__builtin_clrsb(minvalue), __builtin_clrsb(maxvalue));
    ASSERT(*sign_bit_position > num_bits - 1);
    DEBUG("SAME SIGN - min: %d (0x%x)  max: %d (0x%x)  num_bits: %d  sign_pos: %d\n", minvalue,
          minvalue, maxvalue, maxvalue, num_bits, *sign_bit_position);
  } else {
    /* Compute the min/max values that consume only as many bits as the sample */
    int tmp_min = 0, tmp_max = 0;
    while (num_bits < 16) {
      minmax_signed_integer(num_bits, &tmp_min, &tmp_max);

      if ((minvalue < tmp_min) || (maxvalue > tmp_max))
        num_bits++;
      else
        break;
    }
    *sign_bit_position = num_bits - 1;
    DEBUG("DIFF SIGN - min: %d (0x%x)  max: %d (0x%x)  num_bits: %d  sign_pos: %d\n", minvalue,
          minvalue, maxvalue, maxvalue, num_bits, *sign_bit_position);
  }

  ASSERTF(*sign_bit_position >= (num_bits - 1), "sign_bit_position: %d  num_bits: %d", *sign_bit_position, num_bits);
  *bitmask = ((1 << num_bits) - 1);
  //printf("Calc'd bitmask = %4hu\n", *bitmask);
  return num_bits;
}

JCOEF drpe_fixup_sign_bit(uint16_t damaged_sign_ciphertext, int sign_bit_position)
{
  uint16_t fixup_value = (damaged_sign_ciphertext >> sign_bit_position) & 0x1;
  DEBUG("dmgd: %04x  fixup_value: %u  sign_bit_position: %d\n",
        damaged_sign_ciphertext, fixup_value, sign_bit_position);

  /* Propagate the fixup value to the rest of the sign bits */
  int i = sign_bit_position;
  for (; i < 16; i++) {
    uint16_t mask = ~(0x1 << i);
    /* Clear the bit at the appropriate position */
    damaged_sign_ciphertext &= mask;
    /* Set that bit to the fixup value */
    damaged_sign_ciphertext |= (fixup_value << i);
  }

  return ((JCOEF)damaged_sign_ciphertext);
}

void
drpe_encrypt_decrypt(unsigned char *key, unsigned char *nonce,
                     JCOEF *data, int datalen,
                     JCOEF minvalue, JCOEF maxvalue,
                     int only_nonzero)
{
  //printf("%s:%d %s()\n", __FILE__, __LINE__, __func__);

  /* Nothing to do with totally empty blocks */
  if (minvalue == 0 && maxvalue == 0) return;

  // Use the libsodium stream cipher to generate a stream of pseudorandom bytes
  size_t kslen = datalen * sizeof(unsigned short);
  unsigned short *keystream = (unsigned short *) malloc(kslen);
  int rc = crypto_stream((unsigned char *)keystream, kslen, nonce, key);
  if (rc != 0)
    err(1, "Error returned by crypto_stream; rc = %d", rc);

  unsigned short bitmask = 0;
  int sign_bit_position;
  int num_drpe_encrypt_bits = drpe_calc_numbits(minvalue, maxvalue, &bitmask, &sign_bit_position);


  if (num_drpe_encrypt_bits == 0) {
    if (minvalue != 0 || maxvalue != 0)
      printf("Warning: This should almost never happen - min: %d max: %d\n", minvalue, maxvalue);
    // This block is totally uniform.  We're done.
    //printf("This block is totally uniform.  (%4hd vs %4hd)  We're done.\n", minvalue, maxvalue);
    goto done;
  }

  //printf("min = %4hd\tmax = %4hd\tbits = %2d\tmask = %4hu\n", minvalue, maxvalue, num_drpe_encrypt_bits, bitmask);

  int i = 0;
  for (i = 0; i < datalen; i++) {
    /*
     * Convert the signed JCOEF into an uint16_t, which has the exact same bit structure
     * Note that in 2s complement format, the sign bit is by definition all bits up
     * to the integer data (all most significant bits are the same until data)
     */
    uint16_t plaintext = data[i];

    /*
     * Do the encryption by XOR'ing with the previously determined number of bits
     *
     * Note that this may damage the least-significant sign bit. This will need to
     * be copied up to the rest of the sign bits for it to be valid 2's complement
     */
    uint16_t damaged_sign_ciphertext = plaintext ^ (keystream[i] & bitmask);

    JCOEF ciphertext = drpe_fixup_sign_bit(damaged_sign_ciphertext, sign_bit_position);

    if (only_nonzero && (data[i] == 0 || ciphertext == 0)) {
      // If we're only encrypting the non-zero coefficients,
      // then we can't allow a non-zero one to become zero
    } else {
      data[i] = ciphertext;
    }
  }

done:
  // Overwrite the keystream so it's not just hanging around in memory
  sodium_memzero(keystream, kslen);
  free(keystream);
}

void
drpe_encrypt_decrypt_all(unsigned char *key, unsigned char *nonce,
                         JCOEF *data, int datalen,
                         JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg)
{
  //printf("%s:%d %s()\n", __FILE__, __LINE__, __func__);
  drpe_encrypt_decrypt(key, nonce, data, datalen, minvalue, maxvalue, false);
}

void
drpe_encrypt_decrypt_nonzero(unsigned char *key, unsigned char *nonce,
                             JCOEF *data, int datalen,
                             JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg)
{
  //printf("%s:%d %s()\n", __FILE__, __LINE__, __func__);
  drpe_encrypt_decrypt(key, nonce, data, datalen, minvalue, maxvalue, true);
}
