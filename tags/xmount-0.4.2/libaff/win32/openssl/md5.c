/* pass-through MD5 implementation to use Microsoft crypto API */
#include "md5.h"
#include <stdio.h>


void  MD5_Init(MD5_CTX *context)
{
    if(CryptAcquireContext(&context->crypt_provider,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT) == 0){
	if( CryptAcquireContext( &context->crypt_provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) == 0 ){
	    fprintf(stderr,"Can't get Crypto Context\n");
	    exit(1);
	}
    }
    if(CryptCreateHash(context->crypt_provider,CALG_MD5,0,0,&context->hash) != 1){
	CryptReleaseContext(context->crypt_provider, 0 );
	fprintf(stderr,"Can't create hash\n");
	exit(1);
    }
}

/* Run your data through this. */
void MD5_Update(MD5_CTX* context, const unsigned char* data, const size_t len)
{
    if(CryptHashData(context->hash,(BYTE *)data,(DWORD)len,0) != 1){
	fprintf(stderr,"unable to update hash\n");
    }
}


/* Add padding and return the message digest. */
void MD5_Final(unsigned char digest[MD5_DIGEST_SIZE],MD5_CTX* context)
{
    DWORD hashLen = MD5_DIGEST_SIZE;
    if(CryptGetHashParam(context->hash,HP_HASHVAL,(BYTE *)digest,&hashLen,0)!=1){
	fprintf(stderr,"CryptGetHashParam failed\n");
    }
    CryptDestroyHash( context->hash ); context->hash = 0;
    CryptReleaseContext(context->crypt_provider, 0 ); context->crypt_provider=0;
}
  
void MD5(unsigned char *buffer,unsigned int len,unsigned char result[16])
{
    MD5_CTX md5;
    
    MD5_Init(&md5);
    MD5_Update(&md5,buffer,len);
    MD5_Final(result,&md5);
}
