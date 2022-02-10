#!/bin/bash

#DEBUG_SCRIPT=true # if enabled, the output is create in the ./out directory

if [ $# -ne 2 ]; then
    echo "wrong number of arguments"
    exit 1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

REFERENCE_FIRMWARE_IMG=$(echo $1 |sed 's/^.*\///')
OTHER_FIRMWARE_IMG=$(echo $2 |sed 's/^.*\///')

if [ ! -f "${SCRIPT_DIR}/../firmwares/${REFERENCE_FIRMWARE_IMG}" ]; then
    echo "error: firmwares/${REFERENCE_FIRMWARE_IMG} does not exists"
    exit 1
fi
REFERENCE_FIRMWARE="${SCRIPT_DIR}/../firmwares/${REFERENCE_FIRMWARE_IMG}"

if [ ! -f "${SCRIPT_DIR}/../firmwares/${OTHER_FIRMWARE_IMG}" ]; then
    echo "error: firmwares/${OTHER_FIRMWARE_IMG} does not exists"
    exit 1
fi
OTHER_FIRMWARE="${SCRIPT_DIR}/../firmwares/${OTHER_FIRMWARE_IMG}"

# echo REFERENCE_FIRMWARE: ${REFERENCE_FIRMWARE}
# echo OTHER_FIRMWARE: ${OTHER_FIRMWARE}

if [ -z "$DEBUG_SCRIPT" ]
then
    TMP_DIR=$(mktemp -d)
else
    TMP_DIR=${SCRIPT_DIR}/../out
fi

rm -rf ${TMP_DIR}
mkdir -p ${TMP_DIR}

${SCRIPT_DIR}/../imgmaker/mpcimg extract "${REFERENCE_FIRMWARE}" ${TMP_DIR}/reference
${SCRIPT_DIR}/../imgmaker/mpcimg extract "${OTHER_FIRMWARE}" ${TMP_DIR}/other

cd ${TMP_DIR}

rm -rf reference
mkdir -p reference
cd reference
7z x ../rootfs_reference.img
find . -type f -print0 | sort -z | xargs -r0 sha256sum >../reference.txt
cd ..

rm -rf other
mkdir -p other
cd other
7z x ../rootfs_other.img
find . -type f -print0 | sort -z | xargs -r0 sha256sum >../other.txt
cd ..

diff reference.txt other.txt >"${SCRIPT_DIR}/../firmwares/${REFERENCE_FIRMWARE_IMG}-${OTHER_FIRMWARE_IMG}.diff"

if [ -z "$DEBUG_SCRIPT" ]
then
    rm -rf reference
    rm -rf other
    rm -f reference.txt other.txt
    rm -f rootfs_reference.img
    rm -f rootfs_other.img
    rm -rf ${TMP_DIR}
fi
