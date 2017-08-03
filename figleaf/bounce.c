#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <jpeglib.h>
#include <jutil.h>

#include "util.h"
#include "bounce.h"
#include "random.h"

JCOEF
bounce(JCOEF plaintext, JCOEF delta, JCOEF vmin, JCOEF vmax)
{
  int intervalsize = (vmax - vmin) + 1;

  printf("bounce0\t%hd\t%hd\t%hd\t%hd\n", plaintext, delta, vmin, vmax);
  assert(intervalsize > 0);
  assert(plaintext >= vmin);
  assert(plaintext <= vmax);

  plaintext -= vmin;

  if(vmin==vmax) {
    return vmin;
  }

  JCOEF midpoint = 0;
  if(intervalsize % 2 == 0)
    midpoint = intervalsize/2 - 1;
  else
    midpoint = (intervalsize-1) / 2;

  JCOEF t = 0;
  if(plaintext % 2 == 0)
    t = midpoint - (plaintext/2);
  else
    t = midpoint + (plaintext+1)/2;

  JCOEF ciphertext = ((t+delta) % intervalsize) + vmin;

#if 1
  printf("bounce()");
  printf("\t%hd\t%hd", vmin, vmax);
  printf("\tp=%hd", plaintext);
  printf("\tm=%hd\tt=%hd\td=%hd", midpoint, t, delta);
  printf("\t-->\tc=%hd", ciphertext);
  printf("\n");
#endif

  assert(ciphertext >= vmin);
  assert(ciphertext <= vmax);

  return ciphertext;
}

void
bounce_encrypt_block(unsigned char *key, unsigned char *nonce,
                     JCOEF *block, int blocklen,
                     JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  int i = 0;
  int ps = 0;      // Plaintext sum
  int cs = 0;      // Ciphertext sum

  // Pre-Processing
  for(i=0; i<blocklen; i++){
    // Make sure we're in a sane (and positive) range
    block[i] -= vmin;
    // Tally up the sum
    ps += block[i];
  }
  cs = ps;

  JCOEF orig_vmin = vmin;
  vmax = vmax-vmin;
  vmin = 0;


  unsigned short *randomness = random_ushorts(key, nonce, blocklen);
  JCOEF *keystream = (JCOEF *) malloc(blocklen*sizeof(JCOEF));
  for(i=0; i<blocklen; i++){
    keystream[i] = randomness[i] % (vmax-vmin+1);
  }

  for(i=0; i<blocklen; i++){
    JCOEF plain = block[i];
    JCOEF cipher = 0;
    JCOEF xmin = max(vmin, cs - (blocklen-i)*vmax);
    JCOEF xmax = min(vmax, cs - (blocklen-i)*vmin);

    if(xmin==xmax) {
      cipher = xmin;
    }
    else {
      cipher = bounce(plain, keystream[i], xmin, xmax);
    }

    block[i] = cipher;

    cs -= cipher;
    ps -= plain;

    //printf("B-ENC\t%3d\t%3hd ---> %3hd\n", i, plain, cipher);
    //
    printf("BOUNCE");
    printf(" i=%d", i);
    printf("\t%hd\t%hd", xmin, xmax);
    printf("\tp=%hd", plain);
    printf("\tk=%hd", keystream[i]);
    printf("\t-->\tc=%hd", cipher);
    printf("\n");
  }

  // Post-Processing
  vmin = orig_vmin;
  for(i=0; i<blocklen; i++){
    // Map back into whatever range we started in
    block[i] += vmin;
  }

  free(keystream);
  free(randomness);

  //err(1,"OK done with one bounce block.  Debug me plz.");

} // end bounce_encrypt_block




JCOEF
unbounce(JCOEF ciphertext, unsigned char *key)
{
  // TODO Implement this from cascade.py
  return ciphertext;
}



void
bounce_decrypt_block(unsigned char *key, unsigned char *nonce,
                     JCOEF *block, int blocklen,
                     JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  // TODO Implement this from cascade.py/bounce.py
}
