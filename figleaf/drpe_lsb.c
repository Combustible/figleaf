#include <err.h>
#include <sodium.h>

#include <math.h>
#include <jpeglib.h>
#include "random.h"
#include "fisheryates.h"
#include "util.h"
#include "lsb.h"
#include "drpe.h"
#include "drpe_lsb.h"

#define DRPE_LSB_DEBUG 0

#if DRPE_LSB_DEBUG
#define DEBUG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...)
#endif

void
drpe_lsb_encrypt_all(unsigned char *key, unsigned char *nonce,
                     JCOEF *data, int datalen,
                     JCOEF minvalue, JCOEF maxvalue,
                     int num_lsb_bits)
{
  /* Nothing to do with totally empty blocks */
  if (minvalue == 0 && maxvalue == 0) return;

  /* Allocate an array which will contain pointers to all pixels in this block */
  JCOEF **pixel_list = (JCOEF **)calloc(1, datalen * sizeof(JCOEF *));
  ASSERTF(pixel_list != NULL, "Allocation of pixel list failed (%zu bytes)",
          datalen * sizeof(JCOEF *));

  /* Use the libsodium stream cipher to generate a stream of pseudorandom bytes */
  size_t kslen = datalen * sizeof(unsigned short);
  unsigned short *keystream = (unsigned short *) malloc(kslen);
  int rc = crypto_stream((unsigned char *)keystream, kslen, nonce, key);
  ASSERTF(rc == 0, "crypto_stream failed! rc=%d", rc);

  unsigned short bitmask = 0;
  int sign_bit_position;
  int num_drpe_encrypt_bits = drpe_calc_numbits(minvalue, maxvalue, &bitmask, &sign_bit_position);

  /* Constrain the user input to reasonable values */
  if (num_lsb_bits < 0)
    num_lsb_bits = LSB_TPE_DEFAULT_NUM_BITS;
  if (num_lsb_bits > sign_bit_position + 1)
    num_lsb_bits = sign_bit_position + 1;

  /* drpe-preserved bits count towards LSB fixup quota */
  int num_drpe_ignored_bits = sign_bit_position + 1 - num_drpe_encrypt_bits;
  num_lsb_bits -= num_drpe_ignored_bits;
  if (num_lsb_bits < 0) num_lsb_bits = 0;

  DEBUG("min: %d  max: %d  num_drpe_encrypt_bits: %d  num_drpe_ignored_bits: %d  num_lsb_bits: %d\n",
        minvalue, maxvalue, num_drpe_encrypt_bits, num_drpe_ignored_bits, num_lsb_bits);

  /* Plaintext sum of values that were encrypted */
  int64_t target = 0;
  /* Ciphertext sum of values post encryption */
  int64_t cipher = 0;

  /* Perform the actual encryption of the data */
  for (int i = 0; i < datalen; i++) {
    /* Sum the plaintext values that will be encrypted */
    target += data[i];

    /*
     * Convert the signed JCOEF into an uint16_t, which has the exact same bit structure
     * Note that in 2s complement format, the sign bit is by definition all bits up
     * to the integer data (all most significant bits are the same until data)
     */
    uint16_t data_tmp = data[i];

    /* Reverse the bits of the data_tmp blocks */
    data_tmp = lsb_reverse_uint16(data_tmp, num_drpe_encrypt_bits);
    ASSERT((uint16_t)data[i] == lsb_reverse_uint16(data_tmp, num_drpe_encrypt_bits));

    /*
     * Do the encryption by XOR'ing with the previously determined number of bits
     *
     * Note that this may damage the least-significant sign bit. This will need to
     * be copied up to the rest of the sign bits for it to be valid 2's complement
     */
    uint16_t damaged_sign_ciphertext = data_tmp ^ (keystream[i] & bitmask);

    JCOEF ciphertext = drpe_fixup_sign_bit(damaged_sign_ciphertext, sign_bit_position);

    /* DEBUG("encrypt: in: %04x  rev: %04x  damaged_enc: %04x  enc: %04x\n", */
    /*       (uint16_t)data[i], data_tmp, damaged_sign_ciphertext, (uint16_t)ciphertext); */

    data[i] = ciphertext;

    /*
     * Store pointers to the encrypted data elements
     * so we can shuffle the pointers
     */
    pixel_list[i] = &data[i];

    /* Sum the ciphertext values */
    cipher += data[i];
  }

  /*
   * Compute how much the ciphertext needs to change to make its average
   * match that of the plaintext
   */
  int64_t delta = target - cipher;

  DEBUG("target: %ld  cipher: %ld  delta: %ld\n", target, cipher, delta);

  if (LSB_DISTRIBUTE_ENABLED) {
    DEBUG("Shuffling pixel list\n");

    /*
     * Use the fisher-yates algorithm to shuffle the pointers to data values
     *
     * Only shuffle once - that way pixels that are damaged by embedding one
     * bit value are also used for subsequent embedding of additional bits
     * if necessary
     */
    int r = 0;
    uint32_t *randomness = random_uints(key, nonce, datalen);

    for (int i = datalen - 1; i > 0; i--) {
      unsigned int j = randomness[r++] % (i + 1);
      // swap elements i and j
      JCOEF *tmp = pixel_list[i];
      pixel_list[i] = pixel_list[j];
      pixel_list[j] = tmp;
    }

    free(randomness);
  }

  /* Prioritize changing higher significance bits first */
  ASSERT(num_lsb_bits >= 0);
  for (int bit = 0; bit < num_lsb_bits; bit++) {
    DEBUG("Starting patchup with bit %d\n", bit);

    /*
     * Iterate over the randomly sorted pointer array until enough
     * pixels have changed or there are no pixels left
     */
    for (int i = 0; i < datalen; i++) {
      ASSERT(pixel_list[i] != NULL);

      /*
       * Convert the signed JCOEF into an uint16_t, which has the exact same bit structure
       * Note that in 2s complement format, the sign bit is by definition all bits up
       * to the integer data (all most significant bits are the same until data)
       */
      uint16_t data_tmp = *pixel_list[i];

      ASSERT(num_lsb_bits <= num_drpe_encrypt_bits);

      int mask_pos = num_drpe_encrypt_bits - 2 - bit;
      if (mask_pos < 0) {
        /* TODO: This needs more investigation.
         * The above line (-2) makes sure that fixup doesn't embed in the sign bit
         * This theoretically shouldn't be necessary - but for some reason
         * the image is ugly if you embed there.
         */
        goto fixup_done;
      }
      uint16_t mask = 1 << mask_pos;
      if (llabs(delta) < (mask / 2)) {
        /*
         * Can't improve the average any more by flipping bits in this position
         *
         * At this point the average must be really close - just stop trying to
         * flip bits at all
         */
        DEBUG("Fixup done at bit %d! delta: %ld\n", bit, delta);
        goto fixup_done;
      } else if (delta > 0 && (data_tmp & mask) == 0) {
        data_tmp |= mask;
        delta -= mask;

        DEBUG("Setting bit %d! mask: %04x  delta: %ld\n", bit, mask, delta);
      } else if (delta < 0 && (data_tmp & mask) != 0) {
        data_tmp &= ~mask;
        delta += mask;

        DEBUG("Clearing bit %d! mask: %04x  delta: %ld\n", bit, mask, delta);
      }

      DEBUG("Fixup: before: %04x\n", (uint16_t)*pixel_list[i]);
      *pixel_list[i] = drpe_fixup_sign_bit(data_tmp, sign_bit_position);
      DEBUG("Fixup: after: %04x\n", (uint16_t)*pixel_list[i]);
    }
  }
fixup_done:

  // Overwrite the keystream so it's not just hanging around in memory
  sodium_memzero(keystream, kslen);
  free(keystream);
  free(pixel_list);
}

void
drpe_lsb_decrypt_all(unsigned char *key, unsigned char *nonce,
                     JCOEF *data, int datalen,
                     JCOEF minvalue, JCOEF maxvalue,
                     int num_lsb_bits)
{
  /* Nothing to do with totally empty blocks */
  if (minvalue == 0 && maxvalue == 0) return;

  /* Use the libsodium stream cipher to generate a stream of pseudorandom bytes */
  size_t kslen = datalen * sizeof(unsigned short);
  unsigned short *keystream = (unsigned short *) malloc(kslen);
  int rc = crypto_stream((unsigned char *)keystream, kslen, nonce, key);
  ASSERTF(rc == 0, "crypto_stream failed! rc=%d", rc);

  unsigned short bitmask = 0;
  int sign_bit_position;
  int num_drpe_encrypt_bits = drpe_calc_numbits(minvalue, maxvalue, &bitmask, &sign_bit_position);

  /* Perform the actual encryption of the data */
  for (int i = 0; i < datalen; i++) {
    /*
     * Convert the signed JCOEF into an uint16_t, which has the exact same bit structure
     * Note that in 2s complement format, the sign bit is by definition all bits up
     * to the integer data (all most significant bits are the same until data)
     */
    uint16_t data_tmp = data[i];

    /*
     * Do the encryption by XOR'ing with the previously determined number of bits
     *
     * Note that this may damage the least-significant sign bit. This will need to
     * be copied up to the rest of the sign bits for it to be valid 2's complement
     */
    data_tmp = data_tmp ^ (keystream[i] & bitmask);

    /* Reverse the bits of the data_tmp blocks */
    data_tmp = lsb_reverse_uint16(data_tmp, num_drpe_encrypt_bits);

    JCOEF ciphertext = drpe_fixup_sign_bit(data_tmp, sign_bit_position);
    data[i] = ciphertext;
  }

  // Overwrite the keystream so it's not just hanging around in memory
  sodium_memzero(keystream, kslen);
  free(keystream);
}

