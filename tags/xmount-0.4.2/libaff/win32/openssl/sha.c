/* pass-through SHA-1 implementation to use Microsoft crypto API */
#include "sha.h"
#include <stdio.h>


void  SHA1_Init(SHA1_CTX *context)
{
    if(CryptAcquireContext(&context->crypt_provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) == 0){
	if( CryptAcquireContext( &context->crypt_provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) == 0 ){
	    fprintf(stderr,"Can't get Crypto Context\n");
	    exit(1);
	}
    }
    if(CryptCreateHash(context->crypt_provider,CALG_SHA1,0,0,&context->hash) != 1){
	CryptReleaseContext(context->crypt_provider, 0 );
	fprintf(stderr,"Can't create hash\n");
	exit(1);
    }
}

/* Run your data through this. */
void SHA1_Update(SHA1_CTX* context, const unsigned char* data, const size_t len)
{
    if(CryptHashData(context->hash,(BYTE *)data,(DWORD)len,0) != 1){
	fprintf(stderr,"unable to update hash\n");
    }
}


/* Add padding and return the message digest. */
void SHA1_Final(unsigned char digest[SHA1_DIGEST_SIZE],SHA1_CTX* context)
{
    DWORD hashLen = SHA1_DIGEST_SIZE;
    if(CryptGetHashParam(context->hash,HP_HASHVAL,(BYTE *)digest,&hashLen,0)!=1){
	fprintf(stderr,"CryptGetHashParam failed\n");
    }
    CryptDestroyHash( context->hash );  context->hash = 0;
    CryptReleaseContext(context->crypt_provider, 0 ); context->crypt_provider=0;
}
  
