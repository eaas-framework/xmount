/* Fake out OpenSSL's MD5 using Microsoft Crypto API */

#ifndef __MD5_H
#define __MD5_H

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <wincrypt.h>

typedef struct {
    HCRYPTPROV crypt_provider;
    HCRYPTHASH hash;
} MD5_CTX;

#define MD5_DIGEST_SIZE 16

void MD5_Init(MD5_CTX* context);
void MD5_Update(MD5_CTX* context, const unsigned char* data, const size_t len);
void MD5_Final(unsigned char digest[MD5_DIGEST_SIZE],MD5_CTX* context);
void MD5(unsigned char *buffer,unsigned int len,unsigned char result[16]);

#ifdef __cplusplus
}
#endif

#endif /* __MD5_H */
