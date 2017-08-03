#ifndef KDF_H
#define KDF_H

int
kdf_scrypt(unsigned char *key, int keylen,
           unsigned char *passphrase, int passphraselen,
           unsigned char *salt, int saltlen);

int
kdf_hash(unsigned char *key, int key_len,
         unsigned char *passphrase, int passphrase_len,
         unsigned char *salt, int salt_len);

int
kdf_allzero(unsigned char *key, int keylen,
            unsigned char *passphrase, int passphraselen,
            unsigned char *salt, int saltlen);

#endif
