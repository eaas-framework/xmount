/* Bring in SHA256 from OpenSSL or system libraries.
 * If we can't find it, use our own static implementation.
 * The static implementation results in code duplication, so
 * afflib_sha256.h is only included by the specific modules
 * that require SHA256
 */

/* bring in openssl/sha.h, which sometimes has SHA256 and sometimes doesn't.  */
#ifdef HAVE_OPENSSL_SHA_H
#include <openssl/sha.h>
#endif

/* The openssl/sha.h file that defines SHA256 also defines
 * SHA256_DIGEST_LENGTH. So if that symbol is defined, we don't
 * need to do anything else. Otherwise we still need to look for it
 */

#ifndef SHA256_DIGEST_LENGTH	
#ifdef HAVE_SHA256_H
#include <sha256.h>
#else
#ifdef HAVE_OPENSSL_FIPS_SHA_H
#include <openssl/fips_sha.h>
#endif
#endif

#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH 32
#endif

// sometimes they're in the library but not in the #include files
// So define them here.
#if  defined(HAVE_EVP_SHA256)
#ifdef __cplusplus
extern "C" {
#endif
    const EVP_MD *EVP_sha256(void);		
#ifdef __cplusplus
}
#endif
#endif
#endif
