#!/bin/bash

CWD=`pwd`
sudo tar xfvz osxdstroot.tar.gz
sudo mv osxdstroot dstroot
sudo cp xmount dstroot/usr/local/bin/
sudo cp xmount.1 dstroot/usr/local/share/man/man1/
DSTROOT="$CWD/dstroot"

FULL_PKG_NAME=`basename "$CWD"`
PKG_VERSION_LONG=`echo "$FULL_PKG_NAME" | cut -d"-" -f2`
PKG_VERSION_SHORT=`echo "$PKG_VERSION_LONG" | tr -d "."`

sed -i -e "s#OSXDSTROOT#$DSTROOT#g" xmount.pmdoc/01dstroot-contents.xml
sed -i -e "s#OSXDSTROOT#$DSTROOT#g" xmount.pmdoc/01dstroot.xml
sed -i -e "s/VERSION_LONG/$PKG_VERSION_LONG/g" xmount.pmdoc/01dstroot.xml
sed -i -e "s/VERSION/$PKG_VERSION_SHORT/g" xmount.pmdoc/01dstroot.xml
sed -i -e "s#OSXDSTROOT#$DSTROOT#g" xmount.pmdoc/index.xml
sed -i -e "s#CURRENT_PATH#$CWD#g" xmount.pmdoc/index.xml
sed -i -e "s/VERSION_LONG/$PKG_VERSION_LONG/g" xmount.pmdoc/index.xml
sed -i -e "s/VERSION/$PKG_VERSION_SHORT/g" xmount.pmdoc/index.xml

open xmount.pmdoc

