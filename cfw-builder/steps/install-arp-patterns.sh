#!/bin/bash

if [ $# -eq 0 ]; then
    echo "No arguments supplied"
    exit 1
fi

cleaning() {
  rm -rf ${TMP_DIR}
}

trap cleaning EXIT HUP INT QUIT TERM STOP PWR

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
APR_PATTERNS_ZIP="${SCRIPT_DIR}/../../assets/TKGL_Arp Patterns.zip"
APR_PATTERNS_SUB_FOLDER="TKGL_Arp Patterns"

ROOT_IMG=$1

DEST_DIR="/usr/share/Akai/SME0/Arp Patterns"
FILE_UID=0
FILE_GID=0
MODE=777

TMP_DIR=$(mktemp -d)

unzip -q -j "${APR_PATTERNS_ZIP}" "${APR_PATTERNS_SUB_FOLDER}/*" -d ${TMP_DIR}

# install files
e2cp -O ${FILE_UID} -G ${FILE_GID} -P ${MODE} "${TMP_DIR}"/* "${ROOT_IMG}:${DEST_DIR}"
