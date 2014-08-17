#!/bin/bash

control_c() {
  echo
  echo Exiting
  exit 0
}

trap control_c SIGINT

if [ -z "$1" ]; then
  echo "Usage: $0 <jobs>"
  exit 1
fi

JOBS=$1

(
  cd libxmount_input/libxmount_input_aff/
  echo
  echo "Extracting libaff"
  rm -rf libaff &>/dev/null
  tar xfz libaff-*.tar.gz
  echo
  read -p "Ready to configure LIBAFF?"
  cd libaff
  ./bootstrap.sh
  CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --disable-qemu --disable-libewf --disable-fuse --disable-s3 --disable-shared --enable-static --enable-threading
  echo
  read -p "Ready to compile LIBAFF?"
  make -j$JOBS
)
(
  cd libxmount_input/libxmount_input_ewf
  echo
  echo "Extracting libewf"
  rm -rf libewf &>/dev/null
  tar xfz libewf-*.tar.gz
  echo
  read -p "Ready to configure LIBEWF?"
  cd libewf
  CFLAGS="-fPIC" ./configure --disable-v1-api --disable-shared --enable-static --without-libbfio --without-libfuse --without-openssl
  echo
  read -p "Ready to compile LIBEWF?"
  make -j$JOBS
)

