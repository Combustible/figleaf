#include <string.h>
#include <err.h>
#include <sodium.h>

#include "util.h"
#include "kdf.h"

int
kdf_scrypt(unsigned char *key, int key_len,
           unsigned char *passphrase, int passphrase_len,
           unsigned char *salt, int salt_len)
{
  // Yuck this code looks horrible and unreadable, but woo it's using scrypt underneath
  size_t opslimit = crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE;
  size_t memlimit = crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE;

#if 0
  /* Old libsodium-1.0.5 on CentOS 7 doesn't seem to have these... */
  if(key_len < crypto_pwhash_scryptsalsa208sha256_BYTES_MIN)
    err(1, "Requested key length is too short");

  if(key_len > crypto_pwhash_scryptsalsa208sha256_BYTES_MAX)
    err(1, "Requested key length is too long");
#endif

  // We don't put any constraints on how much
  // (or how little) data was provided for our salt.
  // For example, our "salt" here might be just a filename.
  // So we must first hash the original "salt" to
  // get a salt that we can feed into the KDF.
  unsigned char salt_hash[crypto_pwhash_scryptsalsa208sha256_SALTBYTES] = {0};
  if(salt != NULL)
    crypto_generichash(salt_hash, sizeof salt_hash,
                       salt, salt_len, NULL, 0);

  if (crypto_pwhash_scryptsalsa208sha256
      (key, key_len, (char *)passphrase, passphrase_len, salt_hash,
       opslimit, memlimit) != 0) {
      /* out of memory */
      errx(1,"Key derivation failed; Probably ran out of memory");
  }

  return 0;
}

int
kdf_hash(unsigned char *key, int key_len,
         unsigned char *passphrase, int passphrase_len,
         unsigned char *salt, int salt_len)
{
  unsigned char out[crypto_hash_sha256_BYTES];
  crypto_hash_sha256_state state;

  crypto_hash_sha256_init(&state);

  crypto_hash_sha256_update(&state, passphrase, passphrase_len);
  crypto_hash_sha256_update(&state, salt, salt_len);

  crypto_hash_sha256_final(&state, out);

  int num_bytes = min(key_len, crypto_hash_sha256_BYTES);
  memcpy(key, out, num_bytes);

  return 0;
}

//#define USE_HKDF 1
#ifdef USE_HKDF
int
kdf_hkdf(unsigned char *key, int key_len,
         unsigned char *passphrase, int passphrase_len,
         unsigned char *salt, int salt_len)
{
  // Krawczyk's HMAC-based Extract-and-Expand Key Derivation Function (HKDF)
  // https://tools.ietf.org/html/rfc5869
  // This is less secure than using scrypt or Argon2,
  // but it's also much faster.
  // Intended for use in experiments where we need to
  // encrypt lots of images, but we don't really care
  // about protection against a real adversary.

  unsigned char prk[crypto_auth_hmacsha256_BYTES];
  unsigned char salt_hash[crypto_auth_hmacsha256_BYTES];

  // Hash the salt to get a key for the first HMAC
  crypto_hash_sha256(salt_hash, salt, salt_len);

  // Extract -- Use the (hashed) salt as the HMAC key
  // to derive our pseudorandom key from the passphrase.
  crypto_auth_hmacsha256(prk, passphrase, passphrase_len, salt_hash);

  // Expand -- Use the pseudorandom key to generate
  // the required amout of key material.
  int num_blocks = key_len / crypto_auth_hmacsha256_BYTES + 1;
  unsigned char *okm = (unsigned char *) calloc(num_blocks, crypto_auth_hmacsha256_BYTES);

  unsigned char *t = NULL;
  unsigned char *t_prev = NULL;
  for(i=1; i <= num_blocks; i++) {
    /* FIXME
    crypto_auth_hmacsha256(...
    */
  }

  memcpy(key, okm, key_len);
  free(okm);
  return 0;
}
#endif

int
kdf_allzero(unsigned char *key, int keylen,
            unsigned char *passphrase, int passphraselen,
            unsigned char *salt, int saltlen)
{
  // Set the key to all zeros
  memset(key, 0x00, keylen);
  return 0;
}


