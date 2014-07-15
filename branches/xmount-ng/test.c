#include <stdlib.h>

#include "xmount_cache.h"
#include "xmount_log.h"
#include "xmount_options.h"
#include "xmount_macros.h"

int main() {
  XMOptions.Debug=1;
  XMOptions.OrigImageSize=128849018880;
  XMOptions.OverwriteCache=0;
  XMOptions.OrigImageHashSize=1024*1024*10;
  XMOUNT_STRSET(XMOptions.pOrigImageHash,"abcdefghijklmnop")
  
  XMOUNT_STRSET(XMOptions.pCacheFile,"./test.cache")
  
  TXMCacheFile XMCacheFile;

  xmcache_open(&XMCacheFile,XMOptions.pCacheFile);
  
  char *w_buf, *r_buf;
  XMOUNT_MALLOC(w_buf,char*,xmcache_get_blocksize(&XMCacheFile));
  XMOUNT_MALLOC(r_buf,char*,xmcache_get_blocksize(&XMCacheFile));
  memset(w_buf,0x12,xmcache_get_blocksize(&XMCacheFile));
  memset(r_buf,0x00,xmcache_get_blocksize(&XMCacheFile));
  
  if(xmcache_image_write(&XMCacheFile,
                         w_buf,
                         0,
                         0,
                         xmcache_get_blocksize(&XMCacheFile))!=
     xmcache_get_blocksize(&XMCacheFile))
  {
    LOG_ERROR("Couldn't write block\n")
  } else {
    LOG_ERROR("Wrote block successfully\n")
  }
  
  
  if(xmcache_image_read(&XMCacheFile,
                        r_buf,
                        0,
                        0,
                        xmcache_get_blocksize(&XMCacheFile))!=
     xmcache_get_blocksize(&XMCacheFile))
  {
    LOG_ERROR("Couldn't read block\n")
  } else {
    LOG_ERROR("Read block successfully\n")
  }
  
  
  
  if(memcmp(w_buf,r_buf,xmcache_get_blocksize(&XMCacheFile))!=0) {
    LOG_ERROR("No match!\n")
  } else {
    LOG_ERROR("Match!\n")
  }
  
  
  
  
  
  
  
  xmcache_close(&XMCacheFile);


  return 0;
}
