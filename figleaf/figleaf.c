#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include <sodium.h>

#include <unistd.h>
#include <getopt.h>
#include <libgen.h>  // for basename()
#include <sys/stat.h>
#include <glob.h>

#include <jpeglib.h>
#include <jutil.h>

//#include "fisheryates.h"
#include "figleaf.h"
#include "minmax.h"
#include "tpe.h"
#include "fpe.h"
#include "drpe.h"
#include "lsb.h"
#include "drpe_lsb.h"
#include "noop.h"
#include "shuffle.h"
#include "bounce.h"
#include "cascade.h"
#include "gibbs.h"
#include "mosaic.h"
#include "kdf.h"

extern char *optarg;
extern int optind, opterr, optopt;


EXTERN(boolean) read_quant_tables JPP((j_compress_ptr cinfo, char *filename,
                                       int scale_factor, boolean force_baseline));

int
figleaf_process_image(char *input_filename, char *output_filename,
                      char *passphrase, struct figleaf_context *ctx);

int isdir(const char *filename)
{
  struct stat st;
  stat(filename, &st);
  return S_ISDIR(st.st_mode);
}

char *
path_join(char *path, char *filename)
{
  size_t output_len = strlen(path) + 1 + strlen(filename) + 1;
  char *output_filename = (char *) calloc(output_len, sizeof(char));
  strcat(output_filename, path);
  strcat(output_filename, "/");
  strcat(output_filename, filename);
  return output_filename;
}

void print_usage(char *progname)
{
  printf("Usage: %s <-e|-d> -i input_path -o output_path -p passphrase [-b blocksize] [-m module] [-a arg] [-s]\n\n",
         progname);
  printf("  -e: Mode = encrypt\n"
         "  -d: Mode = decrypt\n"
         "  -i: Path to input file\n"
         "  -o: Path to output file\n"
         "  -p: Password to use for encryption\n"
         "  -b: Blocksize to use for thumbnail preservation\n"
         "  -m: Module (or method) to use for encryption/decryption\n"
         "  -a: Integer (int) argument to be passed to the encryption/decryption function\n"
         "  -s: If specified, input file name will be hashed and used to salt the password\n"
         "      This means that decryption will fail if the filename is changed\n");
}

int main(int argc, char *argv[])
{
  char *input_path = NULL;
  char *output_path = NULL;
  char *passphrase = NULL;
  char *quant_matrix_filename = NULL;

  int rc = 0;

  char *optstring = "edsi:o:b:p:m:a:q:";

  //printf("FigLeaf image encryptor/decryptor starting up...\n");

  // Configure our context structure that keeps all our configuration state
  struct figleaf_context *ctx = (struct figleaf_context *) calloc(1, sizeof(struct figleaf_context));

  //printf("Parsing command-line arguments\n");
  while ((rc = getopt(argc, argv, optstring)) != -1) {
    /*
    printf("\trc = %c\n", rc);
    printf("\toptind = %d\n", optind);
    printf("\toptarg = [%s]\n", optarg);
    */

    switch(rc){
      case 'e': // Encrypt
                if(ctx->mode > 0) {
                  print_usage(argv[0]);
                  errx(1,"Can't encrypt AND decrypt at the same time!");
                }
                ctx->mode = FIGLEAF_MODE_ENCRYPT;
                break;
      case 'd': // Decrypt
                if(ctx->mode > 0) {
                  print_usage(argv[0]);
                  errx(1,"Can't encrypt AND decrypt at the same time!");
                }
                ctx->mode = FIGLEAF_MODE_DECRYPT;
                break;
      case 's': // Use the filename as the salt
                ctx->salt_source = "filename";
                break;
      case 'b': // Blocksize
                ctx->blocksize = atoi(optarg);
                break;
      case 'm': // Encryption Module -- Which TPE construction do we want to use?
                ctx->tpe_method_name = optarg;
                break;
      case 'a': // Optional argument to the encryption module
                ctx->fcn_user_arg = atoi(optarg);
                break;
      case 'i': // Input source
                input_path = optarg;
                break;
      case 'o': // Output destination
                output_path = optarg;
                break;
      case 'p': // Passphrase
                passphrase = optarg;
                break;
      case 'q': // Quantization matrix
                quant_matrix_filename = optarg;
                break;
    }
  }

  if (ctx->mode <= 0) {
    print_usage(argv[0]);
    err(1, "Must specify either encryption or decryption");
  }
  if (input_path == NULL) {
    print_usage(argv[0]);
    err(1, "No input path");
  }
  if (output_path == NULL) {
    print_usage(argv[0]);
    err(1, "No output path");
  }
  if (ctx->tpe_method_name == NULL) {
    ctx->tpe_method_name = "drpe-lsb";
  }
  if (ctx->blocksize <= 0 || ctx->blocksize % 8) {
    err(1, "Blocksize must be a multiple of eight");
  }
  if (passphrase == NULL) {
    print_usage(argv[0]);
    err(1, "No passphrase");
  }
#if 0
  // FIXME - Having some trouble linking the read_quant_tables() from libjpeg
  // Is it included in the libjpeg.a that we're building???
  if (quant_matrix_filename != NULL) {
    struct jpeg_compress_struct tmp_cinfo;
    jpeg_create_compress(&tmp_cinfo);
    int scalefactor = 1;
    if (!read_quant_tables(&tmp_cinfo, quant_matrix_filename,
                           scalefactor, 0))
      err(1, "Couldn't read quantization tables");
    int c = 0;
    for (c = 0; c < MAX_COMPS_IN_SCAN; c++) {
      JQUANT_TBL *table = tmp_cinfo.quant_tbl_ptrs[c];
      if (table != NULL) {
        ctx->quant_tables[c] = (JQUANT_TBL *) malloc(sizeof(JQUANT_TBL));
        memcpy(ctx->quant_tables[c], table, sizeof(JQUANT_TBL));
      }
    }
    jpeg_destroy_compress(&tmp_cinfo);
  }
#endif

  if (ctx->tpe_method_name == NULL) {
    err(1, "No encryption/decryption module specified");
  }
  // Set up crypto function pointers
  // Are we encrypting or decrypting?
  if (ctx->mode == FIGLEAF_MODE_ENCRYPT) {
    //ctx->DC_crypto_fcn = cascade_encrypt_block;
    //ctx->DC_crypto_fcn = bounce_encrypt_block;
    if (!strcmp(ctx->tpe_method_name, "lsb")) {
      ctx->DC_crypto_fcn = lsb_encrypt_dc;
      ctx->AC_crypto_fcn = lsb_encrypt_ac;
      ctx->DC_minmax_fcn = minmax_poweroftwo;
      ctx->AC_minmax_fcn = minmax_poweroftwo;
    } else if (!strcmp(ctx->tpe_method_name, "noop")) {
      ctx->DC_crypto_fcn = noop_encrypt_block;
      ctx->AC_crypto_fcn = noop_encrypt_block;
      ctx->DC_minmax_fcn = NULL;
      ctx->AC_minmax_fcn = NULL;
    } else if (!strcmp(ctx->tpe_method_name, "drpe")) {
      ctx->DC_crypto_fcn = drpe_encrypt_decrypt_all;
      ctx->AC_crypto_fcn = drpe_encrypt_decrypt_all;
      ctx->DC_minmax_fcn = minmax_raw;
      ctx->AC_minmax_fcn = minmax_raw;
    } else if (!strcmp(ctx->tpe_method_name, "drpe-nz")) {
      ctx->DC_crypto_fcn = drpe_encrypt_decrypt_all;
      ctx->AC_crypto_fcn = drpe_encrypt_decrypt_nonzero;
      ctx->DC_minmax_fcn = minmax_raw;
      ctx->AC_minmax_fcn = minmax_raw;
    } else if (!strcmp(ctx->tpe_method_name, "drpe-lsb")) {
      ctx->DC_crypto_fcn = drpe_lsb_encrypt_all;
      ctx->AC_crypto_fcn = drpe_encrypt_decrypt_all;
      ctx->DC_minmax_fcn = minmax_raw;
      ctx->AC_minmax_fcn = minmax_raw;
    } else if (!strcmp(ctx->tpe_method_name, "drpe-lsb-nz")) {
      ctx->DC_crypto_fcn = drpe_lsb_encrypt_all;
      ctx->AC_crypto_fcn = drpe_encrypt_decrypt_nonzero;
      ctx->DC_minmax_fcn = minmax_raw;
      ctx->AC_minmax_fcn = minmax_raw;
    } else if (!strcmp(ctx->tpe_method_name, "mosaic")) {
      ctx->DC_crypto_fcn = mosaic_fuzzy_block;
      ctx->AC_crypto_fcn = mosaic_zero_block;
      //ctx->AC_crypto_fcn = fpe_encrypt_all;
      ctx->DC_minmax_fcn = NULL;
      ctx->AC_minmax_fcn = NULL;
    } else if (!strcmp(ctx->tpe_method_name, "shuffle")) {
      ctx->DC_crypto_fcn = shuffle_encrypt_block;
      ctx->AC_crypto_fcn = fpe_encrypt_nonzero;
      //ctx->AC_crypto_fcn = fpe_encrypt_all;
      ctx->DC_minmax_fcn = NULL;
      ctx->AC_minmax_fcn = NULL;
    } else if (!strcmp(ctx->tpe_method_name, "gibbs")) {
      ctx->DC_crypto_fcn = gibbs_encrypt_block;
      ctx->AC_crypto_fcn = fpe_encrypt_nonzero;
      //ctx->AC_crypto_fcn = fpe_encrypt_all;
      ctx->DC_minmax_fcn = minmax_bitmask;
      ctx->AC_minmax_fcn = minmax_poweroftwo;
    } else {
      err(1, "Invalid TPE module name, or no encryption module specified");
    }
    // One more sanity check:
    // If we're working on 8x8 blocks, we should really
    // just leave the DC coefficients alone.
    if (ctx->blocksize == 8) {
      ctx->DC_crypto_fcn = noop_encrypt_block;
      ctx->DC_minmax_fcn = NULL;
      // FIXME - What should we do about the AC's?
      // Here's one idea: preseve the first 1 bit,
      // and encrypt all lower bits.  So it leaks
      // a bit more, but it's always repeatable.
    }
  } else if (ctx->mode == FIGLEAF_MODE_DECRYPT) {
    //ctx->DC_crypto_fcn = cascade_decrypt_block;
    //ctx->DC_crypto_fcn = bounce_decrypt_block;
    if (!strcmp(ctx->tpe_method_name, "lsb")) {
      ctx->DC_crypto_fcn = lsb_decrypt_dc;
      ctx->AC_crypto_fcn = lsb_decrypt_ac;
      ctx->DC_minmax_fcn = minmax_poweroftwo;
      ctx->AC_minmax_fcn = minmax_poweroftwo;
    } else if (!strcmp(ctx->tpe_method_name, "noop")) {
      ctx->DC_crypto_fcn = noop_decrypt_block;
      ctx->AC_crypto_fcn = noop_decrypt_block;
      ctx->DC_minmax_fcn = NULL;
      ctx->AC_minmax_fcn = NULL;
    } else if (!strcmp(ctx->tpe_method_name, "drpe")) {
      ctx->DC_crypto_fcn = drpe_encrypt_decrypt_all;
      ctx->AC_crypto_fcn = drpe_encrypt_decrypt_all;
      ctx->DC_minmax_fcn = minmax_raw;
      ctx->AC_minmax_fcn = minmax_raw;
    } else if (!strcmp(ctx->tpe_method_name, "drpe-nz")) {
      ctx->DC_crypto_fcn = drpe_encrypt_decrypt_all;
      ctx->AC_crypto_fcn = drpe_encrypt_decrypt_nonzero;
      ctx->DC_minmax_fcn = minmax_raw;
      ctx->AC_minmax_fcn = minmax_raw;
    } else if (!strcmp(ctx->tpe_method_name, "drpe-lsb")) {
      ctx->DC_crypto_fcn = drpe_lsb_decrypt_all;
      ctx->AC_crypto_fcn = drpe_encrypt_decrypt_all;
      ctx->DC_minmax_fcn = minmax_raw;
      ctx->AC_minmax_fcn = minmax_raw;
    } else if (!strcmp(ctx->tpe_method_name, "drpe-lsb-nz")) {
      ctx->DC_crypto_fcn = drpe_lsb_decrypt_all;
      ctx->AC_crypto_fcn = drpe_encrypt_decrypt_nonzero;
      ctx->DC_minmax_fcn = minmax_raw;
      ctx->AC_minmax_fcn = minmax_raw;
    } else if (!strcmp(ctx->tpe_method_name, "mosaic")) {
      err(1, "Can't decrypt mosaiced images");
    } else if (!strcmp(ctx->tpe_method_name, "shuffle")) {
      ctx->DC_crypto_fcn = shuffle_decrypt_block;
      ctx->AC_crypto_fcn = fpe_decrypt_nonzero;
      //ctx->AC_crypto_fcn = fpe_decrypt_all;
      ctx->DC_minmax_fcn = NULL;
      ctx->AC_minmax_fcn = NULL;
    } else if (!strcmp(ctx->tpe_method_name, "gibbs")) {
      ctx->DC_crypto_fcn = gibbs_decrypt_block;
      ctx->AC_crypto_fcn = fpe_decrypt_nonzero;
      //ctx->AC_crypto_fcn = fpe_decrypt_all;
      ctx->DC_minmax_fcn = minmax_bitmask;
      ctx->AC_minmax_fcn = minmax_poweroftwo;
    } else {
      err(1, "Invalid TPE module name, or no decryption module specified");
    }
    // One more sanity check:
    // If we're working on 8x8 blocks, we should really
    // just leave the DC coefficients alone.
    if (ctx->blocksize == 8) {
      ctx->DC_crypto_fcn = noop_decrypt_block;
      ctx->DC_minmax_fcn = NULL;
      // FIXME - What should we do about the AC's?
      // Here's one idea: preseve the first 1 bit,
      // and encrypt all lower bits.  So it leaks
      // a bit more, but it's always repeatable.
    }
  } else {
    errx(1, "Invalid mode -- Mode must be either ENCRYPT (-e) or DECRYPT (-d)");
  }

  // Set up the key derivation function
  // TODO: Make this configurable as a command-line option
  ctx->kdf = kdf_hash;


  if (isdir(input_path)) {
    printf("Input path [%s] is a directory\n", input_path);
    if (!isdir(output_path)) {
      err(1, "Input path is a directory, but output path is not");
    }
    glob_t globber;
    char *pattern = path_join(input_path, "*.jpg");
    int glob_flags = GLOB_NOSORT | GLOB_TILDE;
    rc = glob(pattern, glob_flags, NULL, &globber);
    if (rc == 0) { // Success!
      // Now we go through, find all the matching filenames,
      // derive the corresponding output filenames, and
      // encrypt/decrypt all the input files
      size_t i = 0;
      for (i = 0; i < globber.gl_pathc; i++) {
        char *input_filename = globber.gl_pathv[i];
        printf("Found input file [%s]\n", input_filename);
        char *input_basename = basename(input_filename);
        char *output_filename = path_join(output_path, input_basename);
        printf("\tOutput file will be [%s]\n", output_filename);
        // Now process input_filename into output_filename
        figleaf_process_image(input_filename, output_filename,
                              passphrase, ctx);
        free(output_filename);
      }
    } else if (rc == GLOB_NOSPACE) {
      err(1, "Ran out of memory while matching filenames");
    } else if (rc == GLOB_ABORTED) {
      err(1, "Read error while matching filenames");
    } else if (rc == GLOB_NOMATCH) {
      err(1, "No matching input files");
    } else {
      err(1, "Unknown error while matching input filenames");
    }
    globfree(&globber);
    free(pattern);
  } else {
    char *input_filename = input_path;
    char *output_filename = NULL;
    printf("Input path [%s] is NOT a directory\n", input_path);
    if (isdir(output_path)) {
      char *input_basename = basename(input_filename);
      char *output_filename = path_join(output_path, input_basename);
      printf("\tOutput file will be [%s]\n", output_filename);
    } else {
      output_filename = output_path;
      printf("\tUsing specified output file [%s]\n", output_filename);
    }
    // Process input_filename into output_filename
    figleaf_process_image(input_filename, output_filename,
                          passphrase, ctx);
  }

  free(ctx);
  return 0;
}




int
figleaf_process_image(char *input_filename, char *output_filename,
                      char *passphrase, struct figleaf_context *ctx)
{
  FILE *infile = NULL;
  FILE *outfile = NULL;

  struct jpeg_decompress_struct jpegdec;
  struct jpeg_compress_struct jpegenc;
  struct jpeg_error_mgr jerr_dec, jerr_enc;
  struct jeasy *je = NULL;

  //printf("Opening file [%s] for reading\n", input_filename);
  infile = fopen(input_filename, "rb");
  if (infile == NULL) {
    err(1, "Couldn't open file [%s] for reading", input_filename);
  }

  //printf("Opening file [%s] for writing\n", output_filename);
  outfile = fopen(output_filename, "wb");
  if (outfile == NULL) {
    err(1, "Couldn't open file [%s] for writing", output_filename);
  }

  //puts("Creating JPEG decompression object");
  jpegdec.err = jpeg_std_error(&jerr_dec);
  jpeg_create_decompress(&jpegdec);
  //puts("Setting JPEG input to be our input file");
  jpeg_stdio_src(&jpegdec, infile);

  //puts("Creating JPEG compression object");
  jpegenc.err = jpeg_std_error(&jerr_enc);
  jpeg_create_compress(&jpegenc);
  //puts("Setting JPEG output to be our output file");
  jpeg_stdio_dest(&jpegenc, outfile);

  //puts("Reading JPEG header");
  (void) jpeg_read_header(&jpegdec, TRUE);

  // Copy all the JPEG params from the decoder struct into the encoder struct
  //puts("Copying JPEG parameters");
  jpeg_copy_critical_parameters(&jpegdec, &jpegenc);

  // Use Provos's easy interface to get at the DCT block data
  //puts("Creating JPEG Easy struct");
  je = jpeg_prepare_blocks(&jpegdec);

  // And for a sanity check, let's have a look at one of the blocks
  //puts("Here's block (0,0)");
  //print_block(je->blocks[0][0]);

  //puts("Initializing libsodium");
  if (sodium_init() == -1)
    err(1, "Failed to initialize libsodium");

  if (passphrase == NULL) {
    errx(1, "Empty passphrase");
  }

  unsigned char key[crypto_stream_KEYBYTES];
  char *key_salt = NULL;
  int salt_length = 0;
  if (ctx->salt_source != NULL && !strcmp(ctx->salt_source, "filename")) {
    key_salt = basename(input_filename);
    salt_length = strlen(key_salt);
  }

  if (ctx->kdf(key, sizeof key,
               (unsigned char *)passphrase, strlen(passphrase),
               (unsigned char *)key_salt, salt_length)
      != 0)
    err(1, "Key derivation failed");

#if 0
  char buf[65];
  sodium_bin2hex(buf, sizeof buf, key, sizeof key);
  printf("Derived key is [%s]\n", buf);
#endif

  // Now run whichever operation we've decided to do
  puts("Running crypto functions on the input image");
  tpe_process_image(key, je, ctx);

  // Copy DCT coefficients into the output image (that is, the JPEG compression object)
  //puts("Writing JPEG blocks back into JEasy");
  jpeg_return_blocks(je, &jpegdec);
  //puts("Freeing JEasy structure");
  jpeg_free_blocks(je);

  // Copy the actual DCT coefficients from the decoder to the encoder
  //puts("Copying DCT coefficients");
  jvirt_barray_ptr *coeffs = jpeg_read_coefficients(&jpegdec);
  jpeg_write_coefficients(&jpegenc, coeffs);

  // Save the output image, de-allocate compression data, close the output file
  //puts("Finishing JPEG compression with entropy coding");
  jpeg_finish_compress(&jpegenc);
  jpeg_destroy_compress(&jpegenc);
  fclose(outfile);
  // De-allocate decompression data, close the input file
  (void) jpeg_finish_decompress(&jpegdec);
  jpeg_destroy_decompress(&jpegdec);
  fclose(infile);

  //printf("Done writing output file [%s]\n", output_filename);
  return 0;
}


