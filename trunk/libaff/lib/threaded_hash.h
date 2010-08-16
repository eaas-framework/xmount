/*
 * Threaded hash object.
 * Note that this just has a second thread for hashing one block, then it blocks. 
 * We could chain the blocks together, but we don't.
 */

#ifndef THREADED_HASH_H
#define THREADED_HASH_H
#include <openssl/evp.h>
#ifdef HAVE_ERR_H
#include <err.h>
#endif

#include <signal.h>
#include <assert.h>
#include <sys/types.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif

#include <string>
#include <queue>

/* Currently this doesn't thread. */
/* threaded EVP hash object */
class threaded_hash {
    static const u_int MAX_BYTES_IN_WORKLIST = 1024*1024*16; // don't use more than 16MB
private:;
#ifdef HAVE_PTHREAD
    class buffer {
	u_int flags;
    public:
	static const int SHOULD_MALLOC = 0x0001;
	static const int SHOULD_FREE = 0x0002;
	buffer():flags(0),buf(0),bufsize(0){ }
	buffer(const u_char *buf,size_t bufsize,int flags){
	    if(flags & SHOULD_MALLOC){
		this->buf = (u_char *)malloc(bufsize);
		memcpy(this->buf,buf,bufsize);
	    }
	    else {
		this->buf = (u_char *)buf;
	    }
	    this->bufsize = bufsize;
	    this->flags   = flags;
	}
	void done(){
	    if(this->flags & SHOULD_FREE) free(buf);
	}
	u_char *buf;
	size_t bufsize;
	bool   should_free;
    };
    /* These variables are all protected by the mutex */
    std::queue<buffer> worklist;
    size_t   bytes_in_worklist;	// how many do we have
public:
    uint64_t max_bytes_in_worklist;	// how big did it get?
private:
    pthread_t worker_id;			// the worker that is hashing, or 0
    pthread_mutex_t mutex;			// protects worklist and working
    pthread_cond_t wakeup_worker;
    pthread_cond_t wakeup_producer;	// if worklist gets to big, the producer must sleep
    bool be_threaded;
    /* END OF MUTEX AREA */
#endif
    const EVP_MD *md;			// null means hash object is not valid
    EVP_MD_CTX ctx;			// hash context
    mutable u_char *hashbuf;		// null if needs to be calculated
    mutable char *hexbuf;		// mull if needs to be calculated
public:
    std::string name(){return std::string(EVP_MD_name(md));}
    static bool iszero(const u_char *buf,size_t bufsize);


    /** The worker thread needs to be a static function because it is run in its own thread.
     * It does the work on the worklist when there is work to do.
     * If we are not multi-threaded then the update is simply called.
     */

#ifdef HAVE_PTHREAD
    static void *worker(void *arg){
	threaded_hash *t = (threaded_hash *)arg;
	while (1){
	    /* Wait until there is no work to do */
	    pthread_mutex_lock(&t->mutex);
	    while(t->worklist.size()==0){
		pthread_cond_signal(&t->wakeup_producer); // make sure the producer is awake
		pthread_cond_wait(&t->wakeup_worker,&t->mutex); // and wait for more data
	    }
	    class buffer b = t->worklist.front();  /* get the next bit of work */
	    t->worklist.pop();
	    t->bytes_in_worklist -= b.bufsize;
	    t->hashed_bytes += b.bufsize;
	    pthread_mutex_unlock(&t->mutex);
	    if(b.bufsize==0){		// we are done
		break;
	    }
	    EVP_DigestUpdate(&t->ctx,b.buf,b.bufsize);
	    b.done();			// done with this buffer
	}
	return 0;
    }
#endif

public:;
    u_int  hash_size;			// MD5 is 16
    size_t hashed_bytes;		// number of bytes that have been hashed
    threaded_hash(const EVP_MD *md,bool be_threaded){
	this->md          = md;
	this->hashed_bytes= 0;
	this->hashbuf     = 0;
	this->hexbuf      = 0;
	this->hash_size   = 0;
	if(md==0){
	    if(EVP_get_digestbyname("md5")==0){
		fprintf(stderr,"fatal: Call OpenSSL_add_all_digests() prior to calling EVP_get_digestbyname()\n");
		exit(1);
	    }
	    return;		// invalid MD
	}
	EVP_DigestInit(&ctx,md);
	this->hash_size  = EVP_MD_size(md);
#ifdef HAVE_PTHREAD
	this->worker_id   = 0;
	this->bytes_in_worklist = 0;
	this->max_bytes_in_worklist = 0;
	this->be_threaded = be_threaded;
	pthread_mutex_init(&this->mutex,0);
	pthread_cond_init(&this->wakeup_worker,0);
	pthread_cond_init(&this->wakeup_producer,0);
	if(be_threaded) launch();	// create another one
#endif
    }
#ifdef HAVE_PTHREAD
    void launch(){
	pthread_create(&worker_id,NULL,worker,(void *)this);
	assert(worker_id!=0);
    }
    void push(class buffer &b) {
	pthread_mutex_lock(&mutex);
	bytes_in_worklist += b.bufsize;
	if(bytes_in_worklist > max_bytes_in_worklist) max_bytes_in_worklist = bytes_in_worklist;
	worklist.push(b);
	pthread_cond_signal(&wakeup_worker);
	pthread_mutex_unlock(&mutex);
    }
#endif
    static class threaded_hash *new_threaded_hash(const char *name,bool be_threaded){
	return new threaded_hash(EVP_get_digestbyname(name),be_threaded);
    }
    
    ~threaded_hash(){
#ifdef HAVE_PTHREAD
	if(worker_id){			// thread is still present; just kill it
	    class buffer b;		// send through a finish
	    push(b);
	    pthread_join(worker_id,0);	// wait for the thread to finish
	    worker_id = 0;
	}
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&wakeup_worker);
	pthread_cond_destroy(&wakeup_producer);
#endif
	if(md) EVP_MD_CTX_cleanup(&ctx);
	if(hashbuf) free(hashbuf);
	if(hexbuf)  free(hexbuf);
    }
    /** Return if this hash object is usable */
    bool valid(){
	return this->md!=0;
    }

    /** reset the hash object */
    void clear(){
	if(this->md==0) return;
	final();			// make sure that the hashing is done
	EVP_MD_CTX_cleanup(&ctx);
	EVP_DigestInit(&ctx,md);
	this->hashed_bytes = 0;
	if(hashbuf){ free(hashbuf);hashbuf=0;}
	if(hexbuf){ free(hexbuf);hexbuf=0;}
#ifdef HAVE_PTHREAD
	if(be_threaded) launch();
#endif
    }

    void update(const u_char *buf,size_t bufsize){
	if(this->md==0) return;		// no MD set
	if(bufsize==0)  return;		// nothing to do

	/** For the multi-threaded application, copy over the data to be hashed.
	 * Then lock the mutex and start a worker
	 * thread that will do the actual hash. The mutex will unlock when done.
	 * A more efficient implementation would use a thread pool and not constantly
	 * create and destroy the mutexes.
	 */
#ifdef HAVE_PTHREAD
	if(worker_id){
	    pthread_mutex_lock(&mutex);
	    if(bytes_in_worklist > MAX_BYTES_IN_WORKLIST){
		/* If too much in the worklist, wait until it clears before we allocate more */
		pthread_cond_wait(&wakeup_producer,&mutex);
	    }
	    pthread_mutex_unlock(&mutex);
	    class buffer b(buf,bufsize,buffer::SHOULD_FREE|buffer::SHOULD_MALLOC);
	    push(b);
	    return;
	}
#endif
	EVP_DigestUpdate(&ctx,buf,bufsize);
	hashed_bytes += bufsize;
    }
    /** If the hash hasn't been calculated,
     * Perform the final and return a pointer to the buffer.
     */
    u_char *final(){
	if(this->md==0) return 0;
	if(this->hashbuf==0){
#ifdef HAVE_PTHREAD
	    if(worker_id!=0){		// make sure the other thread has stopped.
		class buffer b;		// send through a finish
		push(b);
		pthread_join(worker_id,0); // wait for the worker to be done
		worker_id = 0;		   // the thread is gone
	    }
#endif
	    this->hashbuf     = (u_char *)calloc(hash_size,1);
	    EVP_DigestFinal(&ctx,this->hashbuf,&hash_size);
	}
	return this->hashbuf;
    }

    void final(u_char *mdbuf,unsigned int md_len){
	if(this->md==0) return;
	memcpy(mdbuf,final(),MIN(md_len,hash_size));
    }
    /** Return the hash buffer */
    u_char *hash(){ return final(); }

    /** Returns the length of the hash in bytesn*/
    size_t len(){return hash_size;}

    /** Return the hex of the hash buffer, null terminated */
    const char *hexhash(){
	if(hexbuf==0){
	    this->hexbuf      = (char *)calloc(hash_size*2+1,1);
	    u_char *hashbuf = final();
	    for(u_int i=0;i<hash_size;i++){
		sprintf(hexbuf+i*2,"%02x",hashbuf[i]);
	    }
	}
	return hexbuf;
    }
    bool operator<( threaded_hash &s2);
    bool operator==( threaded_hash &s2);
};

inline bool threaded_hash::iszero(const u_char *buf,size_t bufsize){
    for(u_int i=0;i<bufsize;i++){
	if(buf[i]!=0) return false;
    }
    return true;
}

inline bool threaded_hash::operator<( threaded_hash &s2) {
    if(this->md==0 || s2.md==0) return false;
    if(this->hash_size != s2.hash_size) return false;
    return memcmp(hash(),s2.hash(),hash_size) < 0;
}

inline bool threaded_hash::operator==( threaded_hash &s2) {
    if(this->md==0 || s2.md==0) return false;
    if(this->hash_size != s2.hash_size) return false;
    return memcmp(hash(),s2.hash(),s2.hash_size) == 0;
}



#endif
