#include <stdlib.h>
#include <stdio.h>

#include <jpeglib.h>
#include <random.h>


void fisheryates_shuffle(unsigned char *key, unsigned char *nonce,
                         JCOEF *data, int datalen)
{
  int i;
  int r = 0;
  unsigned int *randomness = random_uints(key, nonce, datalen);

  for(i=datalen-1; i > 0; i--){
    unsigned int j = randomness[r++] % (i+1);
    // swap elements i and j
    //printf("Swapping elements\t%d\tand\t%d\n", i, j);
    JCOEF tmp = data[i];
    data[i] = data[j];
    data[j] = tmp;
  }

  free(randomness);
}

void fisheryates_unshuffle(unsigned char *key, unsigned char *nonce,
                           JCOEF *data, int datalen)
{
  int i;
  int r = 0;
  unsigned int *randomness = random_uints(key, nonce, datalen);
  unsigned int *j = (unsigned int *) malloc(datalen * sizeof(unsigned int));


  for(i=datalen-1; i > 0; i--)
    j[i] = randomness[r++] % (i+1);

  for(i=1; i < datalen; i++) {
    // swap elements i and j[i]
    JCOEF tmp = data[i];
    data[i] = data[j[i]];
    data[j[i]] = tmp;
  }
  free(j);
  free(randomness);
}


