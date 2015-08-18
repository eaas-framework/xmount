#!/bin/bash

if [ "$(whoami)" != "root" ]; then
  echo "ERROR: This script has to be run as root!"
  exit 1
fi

CWD=`dirname "$0"`
DSTROOT="$CWD/dstroot"
FULL_PKG_NAME=`basename "$CWD"`
PKG_VERSION_LONG=`echo "$FULL_PKG_NAME" | cut -d"-" -f2`
PKG_VERSION_SHORT=`echo "$PKG_VERSION_LONG" | tr -d "."`

# Create new dstroot folder
rm -rf "$DSTROOT" &>/dev/null
mkdir -p "$DSTROOT"/usr/local/bin
mkdir -p "$DSTROOT"/usr/local/lib/xmount
mkdir -p "$DSTROOT"/usr/local/share/man/man1

# Populate dstroot with files
cp "$CWD"/build/src/xmount "$DSTROOT"/usr/local/bin/
cp "$CWD"/build/libxmount_input/libxmount_input_aaf/libxmount_input_aaff.dylib "$DSTROOT"/usr/local/lib/xmount/
cp "$CWD"/build/libxmount_input/libxmount_input_aewf/libxmount_input_aewf.dylib "$DSTROOT"/usr/local/lib/xmount/
cp "$CWD"/build/libxmount_input/libxmount_input_aff/libxmount_input_aff.dylib "$DSTROOT"/usr/local/lib/xmount/
cp "$CWD"/build/libxmount_input/libxmount_input_ewf/libxmount_input_ewf.dylib "$DSTROOT"/usr/local/lib/xmount/
cp "$CWD"/build/libxmount_input/libxmount_input_raw/libxmount_input_raw.dylib "$DSTROOT"/usr/local/lib/xmount/
cp "$CWD"/xmount.1 "$DSTROOT"/usr/local/share/man/man1/

# Patch 01dstroot-contents.xml
sed -i -e "s#PMDOC_DSTROOT#$DSTROOT#g" "$CWD"/xmount.pmdoc/01dstroot-contents.xml

# Patch 01dstroot.xml
sed -i -e "s#PMDOC_DSTROOT#$DSTROOT#g" "$CWD"/xmount.pmdoc/01dstroot.xml
sed -i -e "s/PMDOC_VERSION/$PKG_VERSION_SHORT/g" "$CWD"/xmount.pmdoc/01dstroot.xml

# Patch index.xml
sed -i -e "s#PMDOC_CWD#$CWD#g" "$CWD"/xmount.pmdoc/index.xml
sed -i -e "s/PMDOC_LVERSION/$PKG_VERSION_LONG/g" "$CWD"/xmount.pmdoc/index.xml
sed -i -e "s/PMDOC_VERSION/$PKG_VERSION_SHORT/g" "$CWD"/xmount.pmdoc/index.xml

open "$CWD"/xmount.pmdoc

