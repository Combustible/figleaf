#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <jpeglib.h>

#include "util.h"

uint32_t
preprocess_make_positive(JCOEF *array, int arraylen,
                         JCOEF vmin, JCOEF vmax)
{
  int i = 0;
  uint32_t sum = 0;
  for(i=0; i < arraylen; i++) {
    array[i] -= vmin;
    sum += array[i];
  }
  return sum;
}

void
postprocess_restore_range(JCOEF *array, int arraylen,
                          JCOEF vmin, JCOEF vmax)
{
  int i = 0;
  for(i=0; i < arraylen; i++) {
    array[i] += vmin;
  }
}
