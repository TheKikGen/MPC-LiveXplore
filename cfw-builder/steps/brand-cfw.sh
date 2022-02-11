#!/bin/bash

if [ $# -ne 3 ]; then
    echo "wrong number of arguments"
    exit 1
fi

ROOT_IMG=$1
FIRMWARE_VERSION=$2
BUILD_TIME=$3

FILE=/root/cfw-version
FILE_UID=0
FILE_GID=0
MODE=644

# create cfw-version file
echo ${FIRMWARE_VERSION} ${BUILD_TIME} | e2cp - ${ROOT_IMG}:${FILE}