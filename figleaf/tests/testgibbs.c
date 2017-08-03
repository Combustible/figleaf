#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include <sodium.h>

#include <unistd.h>
#include <getopt.h>
#include <libgen.h>  // for basename()

#include "jpeglib.h"
#include "jutil.h"

//#include "fisheryates.h"
//#include "tpe.h"
//#include "fpe.h"
//#include "shuffle.h"
//#include "bounce.h"
//#include "cascade.h"
#include "gibbs.h"

char *optarg;
int optind, opterr, optopt;

JCOEF
calc_sum(JCOEF *array, int arraylen)
{
  JCOEF sum = 0;
  int i = 0;
  for(i=0; i < arraylen; i++)
    sum += array[i];
  return sum;
}


int main(int argc, char* argv[])
{
  int mode = -1;
  int blocksize = 64;
  char *input_filename = NULL;
  char *output_filename = NULL;
  FILE* infile = NULL;
  FILE* outfile = NULL;
  unsigned char *passphrase = NULL;

  struct jpeg_decompress_struct jpegdec;
  struct jpeg_compress_struct jpegenc;
  struct jpeg_error_mgr jerr_dec, jerr_enc;
  struct jeasy *je = NULL;
  int rc = 0;

  char *optstring = "edi:o:b:p:m:";

  printf("FigLeaf image encryptor/decryptor starting up...\n");

  printf("Parsing command-line arguments\n");
  while((rc=getopt(argc, argv, optstring)) != -1) {
    /*
    printf("\trc = %c\n", rc);
    printf("\toptind = %d\n", optind);
    printf("\toptarg = [%s]\n", optarg);
    */

    switch(rc){
      case 'b': 
                blocksize = atoi(optarg);
                break;
      case 'p':
                passphrase = optarg;
                break;
          
    }
  }

  puts("Initializing libsodium");
  if(sodium_init()==-1)
    err(1,"Failed to initialize libsodium");

  if(passphrase == NULL) {
    errx(1,"Empty passphrase");
  }

  unsigned char key[crypto_stream_KEYBYTES];
  unsigned char salt[crypto_pwhash_scryptsalsa208sha256_SALTBYTES];  // say that 5 times fast
  // Salt = Hash(filename)
  crypto_generichash(salt, sizeof salt, basename(input_filename), strlen(basename(input_filename)), NULL, 0);
  // Yuck this code looks horrible and unreadable, but woo it's using scrypt underneath
  size_t opslimit = crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE;
  size_t memlimit = crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE;
  if (crypto_pwhash_scryptsalsa208sha256
      (key, sizeof key, passphrase, strlen(passphrase), salt,
       opslimit, memlimit) != 0) {
      /* out of memory */
      errx(1,"Key derivation failed, out of memory");
  }

  unsigned char buf[65];
  sodium_bin2hex(buf, sizeof buf, key, sizeof key);
  printf("Slightly less insecure key is [%s]\n", buf);


  JCOEF range = 0;
  JCOEF increment = 20;

  //for(range = 5; range < 100; range += randombytes_random() % increment) {
  for(range = 2; range < 300; range *= 2) {

    printf("==========================================================================\n");
    printf("Range = %d\n", range);

    unsigned char nonce[crypto_stream_NONCEBYTES];
    int tweakinput[] = {range};
    crypto_generichash(nonce, sizeof nonce, (unsigned char *) tweakinput, sizeof tweakinput, NULL, 0);

    // Generate random data from libsodium
    uint32_t *randomness = (uint32_t *) malloc(blocksize * sizeof(uint32_t));
    randombytes_buf(randomness, blocksize*sizeof(uint32_t));

    JCOEF *block = (JCOEF *) malloc(blocksize * sizeof(JCOEF));
    int i = 0;
    for(i=0; i < blocksize; i++)
      block[i] = randomness[i] % range;
    puts("Plaintext:");
    print_block(block);
    printf("Sum = %hd\n", calc_sum(block, blocksize));

    // Make a copy so we can verify the decryption
    JCOEF *backup = (JCOEF *) malloc(blocksize * sizeof(JCOEF));
    memcpy(backup, block, blocksize * sizeof(JCOEF));

    // Call Gibbs to encrypt the block
    gibbs_encrypt_block(key, nonce, block, blocksize, 0, range);
    puts("Ciphertext:");
    print_block(block);
    printf("Sum = %hd\n", calc_sum(block, blocksize));

    // Call Gibbs to decrypt the block
    gibbs_decrypt_block(key, nonce, block, blocksize, 0, range);
    puts("Decrypted:");
    print_block(block);
    printf("Sum = %hd\n", calc_sum(block, blocksize));

    // Make sure the decrypted plaintext is the same as the original
    for(i=0; i < blocksize; i++)
      if(block[i] != backup[i])
        printf("ERROR!\ti=%d\t%hd vs %hd\n", i, block[i], backup[i]);
   }

  return 0;
}


