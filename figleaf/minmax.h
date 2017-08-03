#ifndef _MINMAX_H
#define _MINMAX_H

#include <jpeglib.h>

void
minmax_set_debug(int dbg);

void
minmax_from_sample(JCOEF *array, int arraylen,
                   int min_param, int max_param,
                   JCOEF *array_min, JCOEF *array_max,
                   JCOEF *array_avg);

void
minmax_signed_integer(int num_bits, int *min, int *max);

void
minmax_poweroftwo(JCOEF *array, int array_len,
                  int min_power, int max_power,
                  JCOEF *array_min, JCOEF *array_max,
                  JCOEF *array_avg);

void
minmax_average_plusminus_poweroftwo(JCOEF *array, int array_len,
                                    int min_power, int max_power,
                                    JCOEF *array_min, JCOEF *array_max,
                                    JCOEF *array_avg);

void
minmax_bitmask(JCOEF *array, int array_len,
               int min_bits, int max_bits,
               JCOEF *array_min, JCOEF *array_max,
               JCOEF *array_avg);

void
minmax_fixedbase_plus_poweroftwo(JCOEF *array, int array_len,
                                 int num_basebits, int max_power,
                                 JCOEF *array_min, JCOEF *array_max,
                                 JCOEF *array_avg);

void
minmax_raw(JCOEF *array, int array_len,
           int min_power, int max_power,
           JCOEF *array_min, JCOEF *array_max,
           JCOEF *array_avg);

#endif
