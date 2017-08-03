#include <stdint.h>
#include <err.h>
#include <assert.h>
#include <sodium.h>
#include <string.h>

#include <jpeglib.h>
#include "util.h"
#include "gibbs.h"
#include "random.h"
#include "fisheryates.h"

int gibbs_num_rounds = 5;

void gibbs_sample(unsigned char *key, unsigned char *nonce,
                  JCOEF *x, int xlen,
                  JCOEF vmin, JCOEF vmax, uint32_t sum)
{
  int i = 0;
  JCOEF xmin = 0;
  JCOEF xmax = vmax-vmin;
  uint32_t *randomness = random_uints(key, nonce, xlen);
  JCOEF budget = x[xlen-1];

  int subtotal = 0;

  int print = 0;

  //if(sum > 10)
  //  print = 1;

  if(print)
    printf("GIBBS\n");

  for(i=0; i < xlen; i++) {
    if(i < xlen-1)
      budget += x[i];

    if(print){
      printf("GIBBS");
      printf("\ti=%d", i);
      printf("\ts=%d", sum);
      printf("\tt=%d", subtotal);
      printf("\t[%3hd,%3hd]", vmin, vmax);
      printf("\tx=%3hd", x[i]);
      printf("\tb=%3hd", budget);
    }


    JCOEF ymin = max(xmin, budget-xmax);
    JCOEF ymax = min(min(xmax, budget),budget-vmin);
    JCOEF yrange = ymax-ymin+1;

    if(print){
      printf("\t[%3hd,%3hd]", ymin, ymax);
    }

    int delta = randomness[i] % yrange;

    if(print){
      printf("\td=%3d", delta);
    }

    JCOEF y = 0;
    if(i == xlen-1) {
      y = budget;
    }
    else if(yrange <= 0) {
      errx(1,"Gibbs sampling range is empty");
    }
    else if(yrange == 1) {
      y = ymin;
    }
    else {
      assert(x[i] >= vmin);
      assert(x[i] <= vmax);
      assert(budget <= 2*vmax);
      assert(x[i] >= ymin);
      assert(x[i] <= ymax);
      y = (x[i]-ymin+delta) % yrange + ymin;
    }

    if(print){
      printf("\ty=%3hd", y);
      printf("\n");
    }

    x[i] = y;
    budget -= y;
    subtotal += y;
  }
  free(randomness);
}



void
gibbs_encrypt_block(unsigned char *key, unsigned char *block_nonce,
                    JCOEF *block, int blocklen,
                    JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  unsigned int sum = 0;
  int round = 0;

  sum = preprocess_make_positive(block, blocklen, vmin, vmax);
  //for(int i=0; i < blocklen; i++)
  //  sum += block[i];

  for(round=0; round < gibbs_num_rounds; round++) {
  //for(round=0; round < 1; round++) {
    unsigned char shuffle_nonce[crypto_stream_NONCEBYTES];
    unsigned char resample_nonce[crypto_stream_NONCEBYTES];

    // TODO Compute the round nonces from the round number and the block nonce
    // Fake it for now...
    memset(shuffle_nonce, (unsigned char) round, crypto_stream_NONCEBYTES);
    memset(resample_nonce, 0xff ^ (unsigned char) round, crypto_stream_NONCEBYTES);

    fisheryates_shuffle(key, shuffle_nonce, block, blocklen);
    gibbs_sample(key, resample_nonce, block, blocklen, 0, vmax-vmin, sum);
  }

  postprocess_restore_range(block, blocklen, vmin, vmax);

}



void gibbs_reverse_sample(unsigned char *key, unsigned char *nonce,
                          JCOEF *x, int xlen,
                          JCOEF vmin, JCOEF vmax, uint32_t sum)
{
  int i = 0;
  JCOEF xmin = 0;
  JCOEF xmax = vmax-vmin;
  uint32_t *randomness = (uint32_t *) random_uints(key, nonce, xlen);
  JCOEF budget = x[xlen-1];

  int subtotal = 0;

  int print = 0;

  //if(sum > 10)
  //  print = 1;

  if(print)
    printf("GIBBS\n");

  for(i=xlen-2; i >= 0; i--) {
    budget += x[i];

    if(print){
      printf("GIBBS");
      printf("\ti=%d", i);
      printf("\ts=%d", sum);
      printf("\tt=%d", subtotal);
      printf("\t[%3hd,%3hd]", vmin, vmax);
      printf("\tx=%3hd", x[i]);
      printf("\tb=%3hd", budget);
    }


    JCOEF ymin = max(xmin, budget-xmax);
    JCOEF ymax = min(min(xmax, budget),budget-vmin);
    JCOEF yrange = ymax-ymin+1;

    if(print){
      printf("\t[%3hd,%3hd] (%3hd)", ymin, ymax, yrange);
    }

    int delta = randomness[i] % yrange;
    int delta_inverse = yrange - delta;
    if(print){
      printf("\td=%3d, d'=%3d", delta, delta_inverse);
    }
    assert((delta+delta_inverse) % yrange == 0);

    JCOEF y = 0;
    if(i == xlen-1) {
      y = budget;
    }
    else if(yrange <= 0) {
      errx(1,"Gibbs sampling range is empty");
    }
    else if(yrange == 1) {
      y = ymin;
    }
    else {
      assert(x[i] >= vmin);
      assert(x[i] <= vmax);
      assert(budget <= 2*vmax);
      assert(x[i] >= ymin);
      assert(x[i] <= ymax);
      y = (x[i]-ymin+delta_inverse) % yrange + ymin;
    }

    if(print){
      printf("\ty=%3hd", y);
      printf("\n");
    }

    x[i] = y;
    budget -= y;
    subtotal += y;
  }
  x[xlen-1] = sum - subtotal;
  free(randomness);
}



void
gibbs_decrypt_block(unsigned char *key, unsigned char *block_nonce,
                    JCOEF *block, int blocklen,
                    JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  unsigned int sum = 0;
  int round = 0;

  sum = preprocess_make_positive(block, blocklen, vmin, vmax);
  //for(int i=0; i < blocklen; i++)
  //  sum += block[i];

  for(round=gibbs_num_rounds-1; round >= 0; round--) {
  //for(round=0; round < 1; round++) {
    unsigned char shuffle_nonce[crypto_stream_NONCEBYTES];
    unsigned char resample_nonce[crypto_stream_NONCEBYTES];

    // TODO Compute the round nonces from the round number and the block nonce
    // Fake it for now...
    memset(shuffle_nonce, (unsigned char) round, crypto_stream_NONCEBYTES);
    memset(resample_nonce, 0xff ^ (unsigned char) round, crypto_stream_NONCEBYTES);

    gibbs_reverse_sample(key, resample_nonce, block, blocklen, 0, vmax-vmin, sum);
    fisheryates_unshuffle(key, shuffle_nonce, block, blocklen);
  }

  postprocess_restore_range(block, blocklen, vmin, vmax);

}
