#ifndef FIGLEAF_H
#define FIGLEAF_H

#include <jpeglib.h>
#include "jutil.h"
//#include "minmax.h"
//#include "tpe.h"
//#include "kdf.h"

typedef int (*key_derivation_fcn)(unsigned char *, int, unsigned char *, int, unsigned char *, int);

typedef void (*minmax_fcn)(JCOEF*, int, int, int, JCOEF*, JCOEF*, JCOEF*);

typedef void (*block_crypto_fcn)(unsigned char *, unsigned char *, JCOEF *, int, JCOEF, JCOEF, int);

typedef enum {FIGLEAF_MODE_INVALID=0, FIGLEAF_MODE_ENCRYPT=1, FIGLEAF_MODE_DECRYPT=2} crypto_op;

struct figleaf_context {
  char *tpe_method_name; // For identifying which module we're using
  crypto_op mode;        // Encrypt or Decrypt?
  int blocksize;         // Dimensions of a thumbnail block

  /* Do we have a unique salt for each file? */
  /* And if so, where do we get it from? */
  char *salt_source;

  /* Key Derivation Function */
  /* eg PBKDF2, scrypt, ...  */
  key_derivation_fcn kdf;

  /* Functions for handling DC coefficients */
  minmax_fcn       DC_minmax_fcn;  // Minmax function
  block_crypto_fcn DC_crypto_fcn;  // For DC coefficients

  /* Functions for handling AC coefficients */
  minmax_fcn       AC_minmax_fcn;  // Minmax function
  block_crypto_fcn AC_crypto_fcn;  // For AC coefficients

  /* Quantization tables for re-compressing before encrypting */
  /* Need one table for each component of the image (eg YUV)  */
  JQUANT_TBL *quant_tables[MAX_COMPS_IN_SCAN]; 

  /* Optional argument for TPE functions */
  int fcn_user_arg;

};

#endif
