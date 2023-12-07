#!/bin/sh

if ! [ $LV2_INSTALL_PATH ] ; then
LV2_INSTALL_PATH="`pwd`/dist"
fi
echo "target directory: $LV2_INSTALL_PATH"

gcc -g ayumi-lv2.c ayumi_synth.c ayumi.c -fPIC -lm -shared -o "$LV2_INSTALL_PATH/ayumi-lv2.so"

mkdir -p "$LV2_INSTALL_PATH"
cp ayumi-lv2.ttl manifest.ttl "$LV2_INSTALL_PATH"
