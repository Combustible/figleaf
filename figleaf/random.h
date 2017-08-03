#ifndef _RANDOM_H
#define _RANDOM_H

#include <stdint.h>

uint32_t* random_uints(unsigned char *key, unsigned char *nonce, int len);

uint16_t* random_ushorts(unsigned char *key, unsigned char *nonce, int len);

#endif
