/*
 * Threaded hash object.
 */

#include <openssl/evp.h>
#ifdef HAVE_ERR_H
#include <err.h>
#endif

/* threaded EVP hash object */
class threaded_hash {
private:;
#ifdef HAVE_PTHREAD
    pthread_mutex_t mutex;
#endif
    size_t size;
    EVP_MD_CTX ctx;
    u_char *buf;
    static void *update_worker(void *arg){
	threaded_hash *t = (threaded_hash *)arg;
	EVP_DigestUpdate(&t->ctx,t->buf,t->size);
	free(t->buf);
#ifdef HAVE_PTHREAD
	pthread_mutex_unlock(&t->mutex);
#endif
    }
    int be_threaded;

public:;
    threaded_hash(const EVP_MD * (*func)(),int be_threaded){
#ifdef HAVE_PTHREAD
	if(pthread_mutex_init(&mutex,0)) err(1,"pthread_mutex");
#endif
	EVP_DigestInit(&ctx,func());
	this->be_threaded = be_threaded;
    }
    ~threaded_hash(){
#ifdef HAVE_PTHREAD
	pthread_mutex_destroy(&mutex);
#endif
    }

    void update(const u_char *buf,size_t size){
	this->buf = (u_char *)malloc(size);
	this->size = size;
	memcpy(this->buf,buf,size);

#ifdef HAVE_PTHREAD
	pthread_t thread;
	pthread_mutex_lock(&mutex);
	if(be_threaded){
	    pthread_create(&thread,NULL,update_worker,(void *)this);
	    return;
	}
#endif
	update_worker((void *)this);
    }
    void final(u_char *mdbuf,unsigned int md_len){
#ifdef HAVE_PTHREAD
	pthread_mutex_lock(&mutex);
#endif
	EVP_DigestFinal(&ctx,mdbuf,&md_len);
#ifdef HAVE_PTHREAD
	pthread_mutex_unlock(&mutex);
#endif
    }
};


