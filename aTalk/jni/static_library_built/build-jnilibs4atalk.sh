#!/bin/bash

echo "### Building aTalk static libraries for vpx, openssl, and ffmpeg ###"

pushd openssl || exit
  echo "### Building openssl jni library ###"
  ./build-openssl4android.sh
popd || return

pushd libvpx || exit
  echo "### Building libvpx jni library ###"
  ./build-vpx4android.sh
popd

pushd ffmpeg-x264 || exit
  echo "### Building ffmpeg-x264 jni library ###"
 ./build-ffmpeg4android.sh
popd

