#ifndef _UTIL_H
#define _UTIL_H

#include <assert.h>
#include <jpeglib.h>

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#define abs(x) ((x) > (0) ? (x) : (((typeof(x))(-1))*(x)))

/** Tripping one of these asserts means there is a bug someplace */
#define ASSERTF(condition, fmt, ...)                                          \
  do {                                                                        \
    if (!(condition))                                                         \
      printf("%s:%d %s() " fmt "\n", __FILE__, __LINE__,                      \
             __func__, ##__VA_ARGS__);                                        \
    assert(condition);                                                        \
  } while (0)

#define ASSERT(condition)                                                     \
  do {                                                                        \
    if (!(condition))                                                         \
      printf("%s:%d %s(): expression '" #condition "' evaluated to 0\n",      \
             __FILE__, __LINE__, __func__);                                   \
    assert(condition);                                                        \
  } while (0)

unsigned int
preprocess_make_positive(JCOEF *array, int arraylen,
                         JCOEF vmin, JCOEF vmax);

void
postprocess_restore_range(JCOEF *array, int arraylen,
                          JCOEF vmin, JCOEF vmax);

#endif
