#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <sodium.h>

#include "jpeglib.h"
#include "fpe.h"

int main(int argc, char *argv[])
{
  int i = 0;
  int buflen = 200;
  short buf[buflen];

  int vmin = -7;
  int vmax = 7;
  int range = vmax-vmin+1;

  printf("vmin = %d\nvmax = %d\n", vmin, vmax);
  printf("range = %d\n", range);

  randombytes_buf(buf, sizeof(buf));
  for(i=0; i < buflen; i++)
    buf[i] = (unsigned short)buf[i] % range + vmin;

  printf("Plaintext:\n");
  for(i=0; i < buflen; i++) {
    printf("%3d ", buf[i]);
  }
  printf("\n");

  int keylen = 256/8;
  unsigned char key[keylen];
  int noncelen = 192/8;
  unsigned char nonce[noncelen];

  fpe_encrypt(key, nonce, buf, buflen, vmin, vmax, false);

  /*
  printf("Ciphertext:\n");
  for(i=0; i < buflen; i++) {
    printf("%3d ", buf[i]);
  }
  printf("\n");
  */


  fpe_decrypt(key, nonce, buf, buflen, vmin, vmax, false);

  printf("Decrypted:\n");
  for(i=0; i < buflen; i++) {
    printf("%3d ", buf[i]);
  }
  printf("\n");


  return 0;
}
