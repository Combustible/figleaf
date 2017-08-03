#include <err.h>
#include <sodium.h>

#include <math.h>
#include "jpeglib.h"
#include "random.h"
#include "fisheryates.h"
#include "lsb.h"
#include "util.h"


/**
 * If this parameter is set, the LSB TPE encrypt function will overwrite the
 * plaintext value directly with the block average. Decryption will no longer work
 */
#define LSB_TPE_DEBUG_THUMB_OUTPUT 0

/** Tripping one of these asserts means there is a bug someplace */

/**
 * !!!! WARNING !!!! TODO
 *
 * only_nonzero == 1 is not working properly. It skips far more (>4x) elements
 * on encrypt than it skips on decrypt - resulting in a noticably damaged plaintext
 *
 * Skipping zero elements works fine. The problem is skipping elements that would
 * encrypt to zero. I (Byron) don't know what the problem is, but it seems to work
 * well enough without skipping these coefficients. Fixing this will eventually
 * improve the filesize, though.
 */
void
lsb_encrypt(unsigned char *key, unsigned char *nonce,
            JCOEF *data, int datalen,
            JCOEF minvalue, JCOEF maxvalue,
            int only_nonzero, int num_lsb_bits)
{
  /* Can't encrypt this value if there's no range to play with */
  if (maxvalue == minvalue) return;

  unsigned int range = maxvalue - minvalue + 1;

  /* Make sure range is a power of two - only one bit should be set */
  ASSERTF(range > 0 && __builtin_popcount(range) == 1,
          "Range must be > 0 and a power of two!\t(%d,%d)", minvalue, maxvalue);

  /* Allocate an array which will contain pointers to all pixels in this block */
  JCOEF **pixel_list = (JCOEF **)calloc(1, datalen * sizeof(JCOEF *));
  ASSERTF(pixel_list != NULL, "Allocation of pixel list failed %d", range);

  /* Use the libsodium stream cipher to generate a stream of pseudorandom bytes */
  size_t kslen = datalen * sizeof(unsigned short);
  unsigned short *keystream = (unsigned short *) malloc(kslen);
  int rc = crypto_stream((unsigned char *)keystream, kslen, nonce, key);
  ASSERTF(rc == 0, "crypto_stream failed! rc=%d", rc);

  /* Figure out what power of 2 the range covers */
  int num_bits = 0;
  int temp_range = range;
  while (temp_range != 1) {
    temp_range /= 2;
    num_bits++;
  }

  /* Constrain the user input to reasonable values */
  if (num_lsb_bits < 0)
    num_lsb_bits = LSB_TPE_DEFAULT_NUM_BITS;
  if (num_lsb_bits > num_bits)
    num_lsb_bits = num_bits;

#if 0
  printf("Counted number of bits=%d minvalue=%d maxvalue=%d range=%d\n",
         num_bits, minvalue, maxvalue, range);
#endif

  /* Plaintext sum of values that were encrypted */
  int64_t target = 0;
  /* Ciphertext sum of values post encryption */
  int64_t cipher = 0;

  /* Perform the actual encryption of the data */
  for (int i = 0; i < datalen; i++) {
    if ((data[i] != 0) || (only_nonzero == 0)) {
      /* Shift the data to be 0 <= data[i] < range */
      uint16_t data_tmp = data[i] - minvalue;
      ASSERTF(data_tmp >= 0 && data_tmp < range, "Value out of range");

      /* Sum the plaintext values that will be encrypted */
      target += data_tmp;

      /* Reverse the bits of the plaintext blocks */
      data_tmp = lsb_reverse_uint16(data_tmp, num_bits);

      /*
       * If only encrypting nonzero, don't encrypt values
       * that would become zero
       */
      if (only_nonzero != 0 &&
          data_tmp == (keystream[i] & ((1 << num_bits) - 1)))
        continue;

      /* XOR the data with the keystream */
      data_tmp ^= keystream[i] & ((1 << num_bits) - 1);

      /* Sum the ciphertext values */
      cipher += data_tmp;

      /*
       * Store pointers to the encrypted data elements
       * so we can shuffle the pointers
       */
      pixel_list[i] = &data[i];

      /* Shift the data back into the original range */
      data[i] = (JCOEF)data_tmp + minvalue;
      ASSERTF(data[i] >= minvalue && data[i] <= maxvalue, "Value out of range");
    }
  }

  /*
   * Compute how much the ciphertext needs to change to make its average
   * match that of the plaintext
   */
  int64_t delta = target - cipher;

#if LSB_TPE_DEBUG_THUMB_OUTPUT != 0
  for (int i = 0; i < datalen; i++) {
    data[i] = (target / datalen) + minvalue;
  }
#else /* LSB_TYPE_DEBUG_THUMB_OUTPUT == 0 */

  if (LSB_DISTRIBUTE_ENABLED) {
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
  for (int bit = 0; bit < num_lsb_bits; bit++) {
    /*
     * Iterate over the randomly sorted pointer array until enough
     * pixels have changed or there are no pixels left
     */
    for (int i = 0; i < datalen; i++) {
      /* If some values were not encrypted, don't embed in them */
      if (pixel_list[i] == NULL)
        continue;

      /* Shift the data to be 0 <= data[i] < range */
      uint16_t data_tmp = *pixel_list[i] - minvalue;
      ASSERTF(data_tmp >= 0 && data_tmp < range, "Value out of range");

      uint16_t mask = 1 << (num_bits - 1 - bit);
      if (llabs(delta) < (mask / 2)) {
        // Can't improve the average any more by flipping bits in this position
        break;
      } else if (delta > 0 && (data_tmp & mask) == 0) {
        data_tmp |= mask;
        delta -= mask;
      } else if (delta < 0 && (data_tmp & mask) != 0) {
        data_tmp &= ~mask;
        delta += mask;
      }

      /* Shift the data back into the original range */
      *pixel_list[i] = (JCOEF)data_tmp + minvalue;
      ASSERTF(*pixel_list[i] >= minvalue && *pixel_list[i] <= maxvalue, "Value out of range");
    }
  }
#endif /* LSB_TYPE_DEBUG_THUMB_OUTPUT */

  // Overwrite the keystream so it's not just hanging around in memory
  sodium_memzero(keystream, kslen);
  free(keystream);
  free(pixel_list);
}

void
lsb_encrypt_dc(unsigned char *key, unsigned char *nonce,
               JCOEF *data, int datalen,
               JCOEF minvalue, JCOEF maxvalue,
               int fcn_user_arg)
{
  lsb_encrypt(key, nonce, data, datalen, minvalue, maxvalue, 0, fcn_user_arg);
}

void
lsb_encrypt_ac(unsigned char *key, unsigned char *nonce,
               JCOEF *data, int datalen,
               JCOEF minvalue, JCOEF maxvalue,
               __attribute__((unused))int fcn_user_arg)
{
  /* TODO: This causes the file sizes to get pretty big. This is because
   * if a thumbnail block has any JPEG entries with non-zero elements in one
   * field, then ALL other JPEG entries in that field will end up being nonzero.
   *
   * This makes it easy to decrypt, but isn't practical
   */
  lsb_encrypt(key, nonce, data, datalen, minvalue, maxvalue, 0, 0);
}


void
lsb_decrypt(unsigned char *key, unsigned char *nonce,
            JCOEF *data, int datalen,
            JCOEF minvalue, JCOEF maxvalue,
            int only_nonzero, int num_lsb_bits)
{
  /* Can't decrypt this value if there's no range to play with */
  if (maxvalue == minvalue) return;

  unsigned int range = maxvalue - minvalue + 1;

  /* Make sure range is a power of two - only one bit should be set */
  ASSERTF(range > 0 && __builtin_popcount(range) == 1,
          "Range must be > 0 and a power of two!\t(%d,%d)", minvalue, maxvalue);

  /* Use the libsodium stream cipher to generate a stream of pseudorandom bytes */
  size_t kslen = datalen * sizeof(unsigned short);
  unsigned short *keystream = (unsigned short *) malloc(kslen);
  int rc = crypto_stream((unsigned char *)keystream, kslen, nonce, key);
  ASSERTF(rc == 0, "crypto_stream failed! rc=%d", rc);

  /* Figure out what power of 2 the range covers */
  int num_bits = 0;
  int temp_range = range;
  while (temp_range != 1) {
    temp_range /= 2;
    num_bits++;
  }

#if 0
  printf("Counted number of bits=%d minvalue=%d maxvalue=%d range=%d\n",
         num_bits, minvalue, maxvalue, range);
#endif

  /* Perform the actual decryption of the data */
  for (int i = 0; i < datalen; i++) {
    if ((data[i] != 0) || (only_nonzero == 0)) {
      /* Shift the data to be 0 <= data[i] < range */
      uint16_t data_tmp = data[i] - minvalue;
      ASSERTF(data_tmp >= 0 && data_tmp < range, "Value out of range");

      /*
       * If only decryption nonzero, don't decrypt values that would
       * become zero (because they weren't encrypted to begin with)
       */
      if (only_nonzero != 0 &&
          data_tmp == (keystream[i] & ((1 << num_bits) - 1)))
        continue;

      /* XOR the data with the keystream */
      data_tmp ^= keystream[i] & ((1 << num_bits) - 1);

      /* Reverse the bits of the plaintext blocks */
      data_tmp = lsb_reverse_uint16(data_tmp, num_bits);

      /* Shift the data back into the original range */
      data[i] = (JCOEF)data_tmp + minvalue;
      ASSERTF(data[i] >= minvalue && data[i] <= maxvalue, "Value out of range");
    }
  }

  // Overwrite the keystream so it's not just hanging around in memory
  sodium_memzero(keystream, kslen);
  free(keystream);
}


void
lsb_decrypt_dc(unsigned char *key, unsigned char *nonce,
               JCOEF *data, int datalen,
               JCOEF minvalue, JCOEF maxvalue,
               int fcn_user_arg)
{
  lsb_decrypt(key, nonce, data, datalen, minvalue, maxvalue, 0, fcn_user_arg);
}

void
lsb_decrypt_ac(unsigned char *key, unsigned char *nonce,
               JCOEF *data, int datalen,
               JCOEF minvalue, JCOEF maxvalue,
               __attribute__((unused))int fcn_user_arg)
{
  lsb_decrypt(key, nonce, data, datalen, minvalue, maxvalue, 0, 0);
}


