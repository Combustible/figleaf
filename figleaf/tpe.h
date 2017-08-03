#ifndef _TPE_H
#define _TPE_H

#include <jpeglib.h>
#include <jutil.h>

#include "figleaf.h"


void
tpe_process_block(unsigned char *key,
                  struct jeasy *je, int color,
                  int xmin, int ymin,
                  struct figleaf_context *ctx);

void
tpe_process_image(unsigned char *key,
                  struct jeasy *je,
                  struct figleaf_context *ctx);

#endif
