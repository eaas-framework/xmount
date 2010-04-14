/* Fake out OpenSSL's SHA1 using Microsoft Crypto API */

#ifndef __SHA1_H
#define __SHA1_H

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <wincrypt.h>

typedef struct {
    HCRYPTPROV crypt_provider;
    HCRYPTHASH hash;
} SHA1_CTX;

typedef SHA1_CTX SHA_CTX;

#define SHA1_DIGEST_SIZE 20

void SHA1_Init(SHA1_CTX* context);
void SHA1_Update(SHA1_CTX* context, const unsigned char* data, const size_t len);
void SHA1_Final(unsigned char digest[SHA1_DIGEST_SIZE],SHA1_CTX* context);

#ifdef __cplusplus
}
#endif

#endif /* __SHA1_H */
