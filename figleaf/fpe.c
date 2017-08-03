#include <err.h>
#include <sodium.h>
#include <stdbool.h>

#include <jpeglib.h>
#include "fpe.h"

#define DEBUG_FPE 0

int debug_fpe = 0;

void fpe_set_debug(int dbg)
{
  debug_fpe = dbg;
}

void
fpe_encrypt(unsigned char *key, unsigned char *nonce,
            JCOEF *data, int datalen,
            JCOEF minvalue, JCOEF maxvalue,
            int only_nonzero)
{
  // Use the libsodium stream cipher to generate a stream of pseudorandom bytes
  size_t kslen = datalen * sizeof(unsigned short);
  unsigned short *keystream = (unsigned short *) malloc(kslen);
  int rc = crypto_stream((unsigned char *)keystream, kslen, nonce, key);
  if (rc != 0)
    err(1, "Error returned by crypto_stream; rc = %d", rc);

  int range = maxvalue-minvalue+1;
  if(range <= 0)
    err(1, "FPE Encrypt: Range is empty!\t(%d,%d)", minvalue, maxvalue);

  int i=0;

#if DEBUG_FPE
  printf("Plaintext:\t(range = [%d,%d])\n", minvalue, maxvalue);
  for(i=0; i < datalen; i++)
    printf("%3d ", data[i]);
  printf("\n");
  printf("Keystream:\n");
  for(i=0; i < datalen; i++)
    printf("%3d ", keystream[i]%range);
  printf("\n");
#endif

  JCOEF minoutput = 255;
  JCOEF maxoutput = -256;

  for(i=0; i < datalen; i++) {
    unsigned short offset = data[i]-minvalue;
    unsigned short delta = keystream[i] % range;
    short ciphertext = ((offset + delta) % range) + minvalue;
    if(only_nonzero) {
      if(data[i] != 0 && ciphertext != 0) {
        // If we're only encrypting the non-zero coefficients,
        // then we can't allow a non-zero one to become zero
        data[i] = ciphertext;
      }
    } else {
      data[i] = ciphertext;
    }
    if(data[i] > maxoutput)
      maxoutput = data[i];
    if(data[i] < minoutput)
      minoutput = data[i];
  }

  // Overwrite the keystream so it's not just hanging around in memory
  sodium_memzero(keystream, kslen);
  free(keystream);
#if DEBUG_FPE
  printf("Ciphertext:\n");
  for(i=0; i < datalen; i++)
    printf("%3d ", data[i]);
  printf("\n");
#endif
}

void
fpe_encrypt_all(unsigned char *key, unsigned char *nonce,
            JCOEF *data, int datalen,
            JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg)
{
  fpe_encrypt(key, nonce, data, datalen, minvalue, maxvalue, false);
}

void
fpe_encrypt_nonzero(unsigned char *key, unsigned char *nonce,
            JCOEF *data, int datalen,
            JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg)
{
  fpe_encrypt(key, nonce, data, datalen, minvalue, maxvalue, true);
}


void
fpe_decrypt(unsigned char *key, unsigned char *nonce,
            JCOEF *data, int datalen,
            JCOEF minvalue, JCOEF maxvalue,
            int only_nonzero)
{
  // Use the libsodium stream cipher to generate a stream of pseudorandom bytes
  size_t kslen = datalen * sizeof(short);
  unsigned short *keystream = (unsigned short *) malloc(kslen);
  if(keystream == NULL)
    err(1, "Couldn't allocate space for encryption key stream");

  int rc = crypto_stream((unsigned char *)keystream, kslen, nonce, key);
  if (rc != 0)
    err(1, "Error returned by crypto_stream; rc = %d", rc);

  int range = maxvalue-minvalue+1;
  if(range <= 0)
    err(1, "FPE Decrypt: Range is empty!");
  if(range == 1) {
    int i = 0;
    for(i=0; i < datalen; i++)
      data[i] = minvalue;
    //return;
    goto done;
  }

  int i=0;

#if DEBUG_FPE
  printf("Ciphertext:\n");
  for(i=0; i < datalen; i++)
    printf("%3d ", data[i]);
  printf("\n");
  printf("Keystream:\n");
  for(i=0; i < datalen; i++)
    printf("%3d ", keystream[i]%range);
  printf("\n");
#endif

  for(i=0; i < datalen; i++) {
    unsigned short offset = data[i]-minvalue;
    unsigned short delta_inv = range - (keystream[i] % range);
    short plaintext = ((offset + delta_inv) % range) + minvalue;
    if(only_nonzero && data[i]==0) {
      // Don't actually do the decryption.  We skipped this one on the encryption side.
    } else {
      data[i] = plaintext;
    }
  }

done:
  // Overwrite the keystream so it's not just hanging around in memory
  sodium_memzero(keystream, kslen);
  // And return it to the OS
  free(keystream);
#if DEBUG_FPE
  printf("Plaintext:\n");
  for(i=0; i < datalen; i++)
    printf("%3d ", data[i]);
  printf("\n");
#endif
}


void
fpe_decrypt_all(unsigned char *key, unsigned char *nonce,
            JCOEF *data, int datalen,
            JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg)
{
  fpe_decrypt(key, nonce, data, datalen, minvalue, maxvalue, false);
}

void
fpe_decrypt_nonzero(unsigned char *key, unsigned char *nonce,
            JCOEF *data, int datalen,
            JCOEF minvalue, JCOEF maxvalue, int fcn_user_arg)
{
  fpe_decrypt(key, nonce, data, datalen, minvalue, maxvalue, true);
}


