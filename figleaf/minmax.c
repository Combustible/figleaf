#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <err.h>

#include "util.h"
#include <jpeglib.h>

/* Byron - March 17, 2017
 *
 * I've reworked all three of the minmax functions available. Here's a brief
 * description of each:
 *
 * minmax_poweroftwo():
 *  This function examines the provided data and computes a range of the form
 *  [-2**b:2**b - 1] where b is a value between 0<=b<=maxpower
 *
 * minmax_average_plusminus_poweroftwo():
 *  This function does the same as the above function, except that it first
 *  computes the average of the provided data and centers the range around
 *  the average value. The result is [vmax:vmin] = [-2**b + avg:2**b + avg - 1]
 *
 * minmax_fixedbase_plus_poweroftwo():
 *  This function is similar to the .._average_..() function, but instead of
 *  using the average as an offset, it chooses a constant that is a multiple
 *  of some provided base value (2 ** num_basebits). This provides more
 *  stable ranges when the average shifts around by a small amount. If it is
 *  not possible to cover the entire [min:max] range of observed samples
 *  given the number of bits available, this function attempts to minimize
 *  the range of values that fall outside the range
 *
 * *NOTE*
 *  These functions now automatically truncate any values to fit within the
 *  returned ranges! After calling one of these functions you can be sure
 *  that all values encountered later will be within the range [vmin:vmax]
 */




int minmax_debug = 0;

void
minmax_set_debug(int dbg)
{
  minmax_debug = dbg;
}

#define MINMAX_DEBUG(fmt, ...)                               \
  do {                                                       \
    if (minmax_debug)                                        \
      printf("%s:%d %s() " fmt, __FILE__, __LINE__,     \
             __func__, ##__VA_ARGS__);                       \
  } while (0)

void
minmax_from_sample(JCOEF *array, int arraylen,
                   int min_param, int max_param,
                   JCOEF *array_min, JCOEF *array_max,
                   JCOEF *array_avg)
{
  int i = 0;
  int avg = 0;
  *array_min = max_param;
  *array_max = min_param;
  for (i = 0; i < arraylen; i++) {
    if (array[i] > *array_max)
      *array_max = array[i];
    if (array[i] < *array_min)
      *array_min = array[i];
    avg += array[i];
  }

  if (array_avg != NULL)
    *array_avg = (JCOEF)(avg / arraylen);
}

void
minmax_raw(JCOEF *array, int array_len,
           int min_power, int max_power,
           JCOEF *array_min, JCOEF *array_max,
           JCOEF *array_avg)
{
  /* Get the observed min/max values over this block */
  minmax_from_sample(array, array_len, INT16_MIN, INT16_MAX,
                     array_min, array_max, array_avg);
}

/**
 * Returns the minimum and maximum values possible for a signed integer
 * given the specified number of bits
 *
 * Note that the max value is one less than the min for signed integers
 * For example, 6 bits can store -32 to 31
 */
void
minmax_signed_integer(int num_bits, int *min, int *max)
{
  if (num_bits == 0) {
    *min = 0;
    *max = 0;
  } else if (num_bits > 0) {
    *min = -1 * (1 << (num_bits - 1));
    *max = (1 << (num_bits - 1)) - 1;
  } else {
    err(1, "num_bits must be >= 0");
  }
}

void
minmax_poweroftwo(JCOEF *array, int array_len,
                  int min_power, int max_power,
                  JCOEF *array_min, JCOEF *array_max,
                  JCOEF *array_avg)
{
  JCOEF observed_min;
  JCOEF observed_max;

  if (max_power < 1)
    err(1, "max_power must be >= 1");

  /* Get the observed min/max values over this block */
  minmax_from_sample(array, array_len, INT16_MIN, INT16_MAX,
                     &observed_min, &observed_max, array_avg);

  /* Compute the min/max values that consume only as many bits as the sample */
  int num_bits = min_power;
  int tmp_min = 0, tmp_max = 0;
  while (num_bits <= max_power) {
    minmax_signed_integer(num_bits, &tmp_min, &tmp_max);

    if ((observed_min < tmp_min) || (observed_max > tmp_max))
      num_bits++;
    else
      break;
  }

  /*
  if(observed_max < 0 || observed_min > 0)
    printf("MINMAX\tAverage = %4hd\tmin = %4hd\tmax = %4hd\t--> %4d\t%4d\n",
           *array_avg, observed_min, observed_max, tmp_min, tmp_max);
  */

  if ((observed_min < tmp_min) || (observed_max > tmp_max)) {
    /*
     * Reached the maximum number of allowable bits, but some samples are still
     * outside this range
     *
     * Need to modify those data points to fit within the newly constrained range
     */
    MINMAX_DEBUG("WARNING: Insufficient max_power to cover values in block!\n");
    MINMAX_DEBUG("  Overflowing values will be truncated!\n");
    MINMAX_DEBUG("  observed_max = %d but returning array_max = %d\n", observed_max, tmp_max);
    MINMAX_DEBUG("  observed_min = %d but returning array_min = %d\n", observed_min, tmp_min);

    for (int i = 0; i < array_len; i++) {
      if (array[i] > tmp_max)
        array[i] = tmp_max;
      if (array[i] < tmp_min)
        array[i] = tmp_min;
    }
  }

  *array_min = tmp_min;
  *array_max = tmp_max;
}

void
minmax_average_plusminus_poweroftwo(JCOEF *array, int array_len,
                                    int min_power, int max_power,
                                    JCOEF *array_min, JCOEF *array_max,
                                    JCOEF *array_avg)
{
  JCOEF observed_min;
  JCOEF observed_max;

  if (max_power < 1)
    err(1, "max_power must be >= 1");

  /* Get the observed min/max values over this block */
  minmax_from_sample(array, array_len, INT16_MIN, INT16_MAX,
                     &observed_min, &observed_max, array_avg);

  /*
   * Compute min/max values centered around the average that
   * spans an few bits as possible
   */
  int num_bits = 0;
  int tmp_min = 0, tmp_max = 0;
  while (num_bits <= max_power) {
    minmax_signed_integer(num_bits, &tmp_min, &tmp_max);

    tmp_min += *array_avg;
    tmp_max += *array_avg;

    if ((observed_min < tmp_min) || (observed_max > tmp_max))
      num_bits++;
    else
      break;
  }

  if ((observed_min < tmp_min) || (observed_max > tmp_max)) {
    /*
     * Reached the maximum number of allowable bits, but some samples are still
     * outside this range
     *
     * Need to modify those data points to fit within the newly constrained range
     */
    MINMAX_DEBUG("WARNING: Insufficient max_power to cover values in block!\n");
    MINMAX_DEBUG("  Overflowing values will be truncated!\n");
    MINMAX_DEBUG("  observed_max = %d but returning array_max = %d\n", observed_max, tmp_max);
    MINMAX_DEBUG("  observed_min = %d but returning array_min = %d\n", observed_min, tmp_min);

    for (int i = 0; i < array_len; i++) {
      if (array[i] > tmp_max)
        array[i] = tmp_max;
      if (array[i] < tmp_min)
        array[i] = tmp_min;
    }
  }

  *array_min = tmp_min;
  *array_max = tmp_max;
}


void
minmax_bitmask(JCOEF *array, int array_len,
               int min_bits, int max_bits,
               JCOEF *array_min, JCOEF *array_max,
               JCOEF *array_avg)
{
  JCOEF observed_min;
  JCOEF observed_max;
  int num_bits = 0;

  if (min_bits < 0)
    err(1, "min_bits must be >= 0");
  if (max_bits < 1)
    err(1, "max_bits must be >= 1");
  if (max_bits > 10)
    err(1, "max_bits must be <= 10");

  /* Get the observed min/max values over this block */
  minmax_from_sample(array, array_len, INT16_MIN, INT16_MAX,
                     &observed_min, &observed_max, array_avg);

  /* This function only really works for numbers
   * that share the same sign bit.  For everything
   * else, we fall back to the power-of-two method.
   * */
  if( observed_min < 0 && observed_max >= 0 ) {
    //MINMAX_DEBUG("--- Falling back to power-of-two ---\n");
    minmax_poweroftwo(array, array_len, 0, max_bits+1,
                      array_min, array_max, array_avg);
    /*
    MINMAX_DEBUG("min = %4hd\tmax = %4hd\tavg = %4hd\n",
                 *array_min, *array_max, *array_avg);
    */
    return;
  }

  /* Now find which bits are always the same,
   * and how many bits on the LSB end can vary.
   * */
  num_bits = max_bits;
  int mask = 0;
  while(num_bits >= min_bits) {
    mask = 1 << num_bits;
    if( (observed_min & mask) != (observed_max & mask) )
      break;
    else
      num_bits -= 1;
  }

  mask = (1<<(num_bits+1)) - 1;
  int tmp_min = observed_min & ~mask;
  int tmp_max = tmp_min | mask;

  if(tmp_min < -pow(2,10))
    tmp_min = -pow(2,10);
  if(tmp_max > pow(2,10))
    tmp_max = pow(2,10);

  /*
  if(observed_max < 0 || observed_min > 0)
    printf("Average = %4hd\tmin = %4hd\tmax = %4hd\t-->\t%4d\t%4d\n",
                 *array_avg, observed_min, observed_max, tmp_min, tmp_max);
  */

  if ((observed_min < tmp_min) || (observed_max > tmp_max)) {
    /*
     * Reached the maximum number of allowable bits, but some samples are still
     * outside this range
     *
     * Need to modify those data points to fit within the newly constrained range
     */
    MINMAX_DEBUG("WARNING: Insufficient max_power to cover values in block!\n");
    MINMAX_DEBUG("  Overflowing values will be truncated!\n");
    MINMAX_DEBUG("  observed_max = %d but returning array_max = %d\n", observed_max, tmp_max);
    MINMAX_DEBUG("  observed_min = %d but returning array_min = %d\n", observed_min, tmp_min);

    for (int i = 0; i < array_len; i++) {
      if (array[i] > tmp_max)
        array[i] = tmp_max;
      if (array[i] < tmp_min)
        array[i] = tmp_min;
    }
  }

  *array_min = tmp_min;
  *array_max = tmp_max;
}


void
minmax_fixedbase_plus_poweroftwo(JCOEF *array, int array_len,
                                 int num_basebits, int max_power,
                                 JCOEF *array_min, JCOEF *array_max,
                                 JCOEF *array_avg)
{
  JCOEF observed_min;
  JCOEF observed_max;
  JCOEF tmp_min;
  JCOEF tmp_max;

  if (max_power < 1)
    err(1, "max_power must be >= 1");

  /* Get the observed min/max values over this block */
  minmax_from_sample(array, array_len, INT16_MIN, INT16_MAX,
                     &observed_min, &observed_max, array_avg);

  /* MINMAX_DEBUG("num_basebits=%d max_power=%d observed_min=%d observed_max=%d\n", */
  /*              num_basebits, max_power, observed_min, observed_max); */

  // First get a lower bound on our min.
  uint16_t base = (uint16_t)(1 << num_basebits);
  tmp_min = base * (observed_min / base);
  // tmp_min might be lower than the observed min if it was negative when we did integer division
  if (tmp_min > observed_min)
    tmp_min -= base;

  // Next figure out how many bits are needed to cover the range starting from that value
  int num_bits = 1;
  tmp_max = tmp_min;
  do {
    tmp_max = tmp_min + (1 << num_bits) - 1;

    if (observed_max > tmp_max)
      num_bits++;
    else
      break;
  } while (num_bits <= max_power);

  if ((observed_min < tmp_min) || (observed_max > tmp_max)) {
    /*
     * Reached the maximum number of allowable bits, but some samples are still
     * outside this range
     */

    /*
     * If we can't cover the entire range with the allowable number of bits, try to move
     * the range to cover as much of the range as possible while still maintaining
     * a multiple of 2**base_bits
     */
    /* MINMAX_DEBUG("Orig range = %d:%d - observed range = %d:%d\n", */
    /*              tmp_min, tmp_max, observed_min, observed_max); */
    int last_out_of_range = max(0, tmp_min - observed_min) * max(0, tmp_min - observed_min) +
                            max(0, observed_max - tmp_max) * max(0, observed_max - tmp_max);
    while (1) {
      /*
       * For both of these values, positive values indicate the amount out of range
       * and should be minimized, negative results are good (room to move)
       *
       * Use the square of the count of the out of range so that if the current range is
       * much smaller than the observed range, we'll find something in the middle that
       * balances the tails
       */
      int count_over_max = observed_max - tmp_max;
      int count_under_min = tmp_min - observed_min;
      int new_out_of_range;
      int delta;

      if (count_over_max > count_under_min) {
        delta = (1 << num_basebits);
      } else {
        delta = (-1) * (1 << num_basebits);
      }
      new_out_of_range = (max(0, (tmp_min + delta) - observed_min) *
                          max(0, (tmp_min + delta) - observed_min)) +
                         (max(0, observed_max - (tmp_max + delta)) *
                          max(0, observed_max - (tmp_max + delta)));

      /* MINMAX_DEBUG("new_out_of_range=%d last_out_of_range=%d\n", new_out_of_range, last_out_of_range); */

      if (last_out_of_range <= new_out_of_range)
        break;

      tmp_max += delta;
      tmp_min += delta;
      last_out_of_range = new_out_of_range;
    }
    /* MINMAX_DEBUG("New range = %d:%d - observed range = %d:%d\n", tmp_min, tmp_max, observed_min, observed_max); */

    /* Need to modify those data points to fit within the newly constrained range */
    MINMAX_DEBUG("WARNING: Insufficient max_power to cover values in block!\n");
    MINMAX_DEBUG("  Overflowing values will be truncated!\n");
    MINMAX_DEBUG("  observed_max = %d but returning array_max = %d\n", observed_max, tmp_max);
    MINMAX_DEBUG("  observed_min = %d but returning array_min = %d\n", observed_min, tmp_min);

    for (int i = 0; i < array_len; i++) {
      if (array[i] > tmp_max)
        array[i] = tmp_max;
      if (array[i] < tmp_min)
        array[i] = tmp_min;
    }
  }

  *array_min = tmp_min;
  *array_max = tmp_max;
}

