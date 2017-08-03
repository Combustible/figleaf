#ifndef _FIGLEAF_COMMON_H
#define _FIGLEAF_COMMON_H

JCOEF* get_DCT_coefficients_by_freq(struct jeasy *je, int color, int freq);
void   set_DCT_coefficients_by_freq(struct jeasy *je, int color, int freq, JCOEF *buf);

JCOEF* get_DC_coefficients(struct jeasy *je, int color);
void set_DC_coefficients(struct jeasy *je, int color, JCOEF *buf);

void print_coefs(JCOEF *buf, int width, int height);

#endif
