(
  cd libaff
  ./configure --disable-qemu --enable-libewf=no --enable-fuse=no --disable-shared --enable-static --enable-s3=no --enable-threading && make -j$1
)
(
  cd libewf
  ./configure --enable-v1-api --disable-v2-api --disable-shared --enable-static && make -j$1
)
