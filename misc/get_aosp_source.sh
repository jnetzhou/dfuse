#!/bin/sh

TOP_DIR=$(pwd)
REV=android-4.2.1_r1.2

mkdir -p upstream
cd upstream
if [ ! -e system/core ];
then
	git clone https://android.googlesource.com/platform/system/core
else
	echo "system/core already present, no clone, just check the revision"
fi
cd core
git checkout ${REV}

cd ${TOPDIR}
