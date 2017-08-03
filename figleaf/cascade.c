#include <err.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <sodium.h>

#include <jpeglib.h>
#include <jutil.h>

#include "util.h"
#include "bounce.h"
#include "cascade.h"
#include "random.h"

#define DEBUG_CASCADE 1


void
my_assert(int b, const char *str)
{
  if (!b)
    err(1, "%s\n", str);
}




unsigned int
cascade_encrypt_recurse(JCOEF *keystream,
                      JCOEF *block, int blocklen,
                      unsigned int start_index, unsigned int end_index,
                      unsigned int subsum, unsigned int offset,
                      JCOEF vmin, JCOEF vmax)
{

  printf("CASCADE\t%u\t%u\t%u\t%u\t%hd\t%hd\n", start_index, end_index, subsum, offset, vmin, vmax);

  if(start_index == end_index){
    block[start_index] = subsum;
    return offset;
  }

  JCOEF delta = vmax - vmin;
  unsigned int midpoint = (end_index + start_index) / 2;
  int max_right = min(subsum - (midpoint-start_index+1) * vmin, (end_index-midpoint)*vmax);
  int min_right = max(subsum - (midpoint-start_index+1) * vmax, (end_index-midpoint)*vmin);

  unsigned int right_sum = 0;
  unsigned int left_sum = 0;

  printf("\tmax_right = %d\n\tmin_right = %d\n", max_right, min_right);

  if(max_right - min_right >= delta) {
    unsigned int bounce_left = (max_right + min_right - delta + 1) / 2;
    unsigned int bounce_right = bounce_left + delta;
    assert(min_right <= bounce_left);
    assert(bounce_right <= max_right);
    assert(delta==bounce_right-bounce_left);
    right_sum = bounce(block[offset], keystream[offset], vmin, vmax) + bounce_left;
  }
  else {
    JCOEF z = (JCOEF) ((block[offset]-vmin) / (delta+1.0) * (max_right-min_right+1.0));
    right_sum = bounce(z, keystream[offset], vmin, vmax) + min_right;
  }
  assert(min_right <= right_sum);
  assert(right_sum <= max_right);

  offset += 1;
  left_sum = subsum - right_sum;
  offset = cascade_encrypt_recurse(keystream, block, blocklen, start_index, midpoint, left_sum, offset, vmin, vmax);
  return cascade_encrypt_recurse(keystream, block, blocklen, midpoint+1, end_index, right_sum, offset, vmin, vmax);

}


void
cascade_encrypt_block(unsigned char *key, unsigned char *nonce,
                      JCOEF *block, int blocklen,
                      JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{
  unsigned int i = 0;
  unsigned int subsum = 0;

  for(i=0; i < blocklen; i++) {
    // Argh.  DC values can be positive, negative, all over the place.
    // Let's make our lives simpler and work in the range [0,vmax-vmin+1]
    block[i] -= vmin;
    subsum += block[i];
  }
  puts("-- START PLAINTEXT BLOCK --");
  print_block(block);
  puts("-- END PLAINTEXT BLOCK --");

  uint32_t* randomness = random_uints(key, nonce, blocklen);
  JCOEF *keystream = (JCOEF *) malloc(blocklen*sizeof(JCOEF));
  for(i=0; i < blocklen; i++)
    keystream[i] = randomness[i] % (vmax-vmin+1);

  cascade_encrypt_recurse(keystream, block, blocklen, 0, blocklen-1, subsum, 0, 0, vmax-vmin+1);

  free(keystream);
  free(randomness);

  puts("-- START CIPHERTEXT BLOCK --");
  print_block(block);
  puts("-- END CIPHERTEXT BLOCK --");

  // My hack above subtracts vmin from every value
  // Now we hvae to put it back
  for(i=0; i < blocklen; i++) {
    block[i] += vmin;
  }

  err(1,"Done with one iteration.  Debug me plz.");
}


void
cascade_decrypt_block(unsigned char *key, unsigned char *nonce,
                      JCOEF *block, int blocklen,
                      JCOEF vmin, JCOEF vmax, int fcn_user_arg)
{

}

