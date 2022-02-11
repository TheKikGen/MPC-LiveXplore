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
FX_RACKS_ZIP="${SCRIPT_DIR}/../../assets/TKGL_FX Racks TKGL.zip"
FX_RACKS_SUB_FOLDER="TKGL_FX Racks TKGL"

ROOT_IMG=$1

DEST_DIR="/usr/share/Akai/SME0/FX Racks"
FILE_UID=0
FILE_GID=0
DIRMODE=777
FILEMODE=777

TMP_DIR=$(mktemp -d)

unzip -q "${FX_RACKS_ZIP}" -d ${TMP_DIR}


DIR_COUNT_START=$(e2ls -l "${ROOT_IMG}:${DEST_DIR}" | wc -l)


LEN=$(expr length "${TMP_DIR}/${FX_RACKS_SUB_FOLDER}" + 1)

find "${TMP_DIR}/${FX_RACKS_SUB_FOLDER}" -mindepth 1 -type d | while read DIR
do
  SUBDIR=${DIR:${LEN}}
  e2mkdir -O ${FILE_UID} -G ${FILE_GID} -P ${DIRMODE} "${ROOT_IMG}:${DEST_DIR}/${SUBDIR}"
  e2cp -O ${FILE_UID} -G ${FILE_GID} -P ${FILEMODE} "${DIR}"/* "${ROOT_IMG}:${DEST_DIR}/${SUBDIR}"
done

DIR_COUNT_FINISH=$(e2ls -l "${ROOT_IMG}:${DEST_DIR}" | wc -l)

if [ "${DIR_COUNT_START}" != "${DIR_COUNT_FINISH}" ]; then
    echo "warning: The count of the FX Racks folder changed from ${DIR_COUNT_START} to ${DIR_COUNT_FINISH}."
    echo "         Maybe the firmware reorganized the folder structure or you added custom folders."
fi