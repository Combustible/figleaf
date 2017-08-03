#include <stdint.h>
#include <err.h>
#include <sodium.h>

#include "random.h"

uint32_t*
random_uints(unsigned char *key, unsigned char *nonce, int len)
{
  size_t randomlen = len * sizeof(uint32_t);
  uint32_t *random_numbers = (uint32_t *) malloc(randomlen);
  int rc = crypto_stream((unsigned char *)random_numbers, randomlen, nonce, key);
  if (rc != 0)
    err(1, "Error returned by crypto_stream; rc = %d", rc);
  return random_numbers;
}

uint16_t*
random_ushorts(unsigned char *key, unsigned char *nonce, int len)
{
  size_t randomlen = len * sizeof(uint16_t);
  uint16_t *random_numbers = (uint16_t *) malloc(randomlen);
  int rc = crypto_stream((unsigned char *)random_numbers, randomlen, nonce, key);
  if (rc != 0)
    err(1, "Error returned by crypto_stream; rc = %d", rc);
  return random_numbers;

}
