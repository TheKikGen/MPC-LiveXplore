#!/bin/bash

#DEBUG_SCRIPT=true # if enabled, the output is create in the ./out directory

if [ $# -ne 1 ]; then
    echo "wrong number of arguments"
    exit 1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

FIRMWARE_IMG=$(echo $1 |sed 's/^.*\///')

if [ ! -f "${SCRIPT_DIR}/../firmwares/${FIRMWARE_IMG}" ]; then
    echo "error: firmwares/${FIRMWARE_IMG} does not exists"
    exit 1
fi
FIRMWARE="${SCRIPT_DIR}/../firmwares/${FIRMWARE_IMG}"
#echo FIRMWARE: ${FIRMWARE}

MODEL=$(${SCRIPT_DIR}/firmware-info.sh model "${FIRMWARE}")

if [ -z "$MODEL" ]
then
    # todo - add override here
    exit 1
fi

MAKE_MODEL=make-${MODEL}
#echo MAKE_MODEL: ${MAKE_MODEL}

if [ -z "$DEBUG_SCRIPT" ]
then
    TMP_DIR=$(mktemp -d)
else
    TMP_DIR=${SCRIPT_DIR}/../out
fi

if [ "$DEBUG_SCRIPT" ]
then
    rm -rf ${TMP_DIR}
fi

mkdir -p ${TMP_DIR}

# debug
# comment out the extract if you debug the script
${SCRIPT_DIR}/../imgmaker/mpcimg extract "${FIRMWARE}" ${TMP_DIR}/cfw

FIRMWARE_FULL_VERSION=$(${SCRIPT_DIR}/../imgmaker/mpcimg info "${FIRMWARE}" | grep 'inmusic,version' | sed 's/.*:\s*//')
FIRMWARE_VERSION=$(echo ${FIRMWARE_FULL_VERSION} | sed 's/\.[^.]*$//')
BUILD_TIME=$(date +"%Y%m%d_%H%M%S")
CFW_IMG=$(echo ${FIRMWARE_IMG} | sed 's/\.[^.]*$//' ).${BUILD_TIME}_cfw.img

if [ "$DEBUG_SCRIPT" ]
then
    echo FIRMWARE_IMG: ${FIRMWARE_IMG}
    echo CFW_IMG: ${CFW_IMG}
    echo FIRMWARE_FULL_VERSION: ${FIRMWARE_FULL_VERSION}
    echo FIRMWARE_VERSION: ${FIRMWARE_VERSION}
fi

ROOTFS=${TMP_DIR}/rootfs_cfw.img

# debug
# do an exit here if you want to save the vanilla firmware for debugging the script
# exit 1

${SCRIPT_DIR}/steps/clean-root-pw-in-shadow.sh "${ROOTFS}" || exit 1
${SCRIPT_DIR}/steps/enable-ssh-root-login-empty-pw.sh "${ROOTFS}" || exit 1
${SCRIPT_DIR}/steps/enable-sshd.sh "${ROOTFS}" || exit 1
${SCRIPT_DIR}/steps/prepare-launch-mpc-script.sh "${ROOTFS}" || exit 1
${SCRIPT_DIR}/steps/remove-acvtestapp.sh "${ROOTFS}" || exit 1
${SCRIPT_DIR}/steps/prepare-root-folder-for-ssh.sh "${ROOTFS}" || exit 1
${SCRIPT_DIR}/steps/install-arp-patterns.sh "${ROOTFS}" || exit 1
${SCRIPT_DIR}/steps/install-fx-racks.sh "${ROOTFS}" || exit 1
${SCRIPT_DIR}/steps/brand-cfw.sh "${ROOTFS}" "${FIRMWARE_FULL_VERSION}" "${BUILD_TIME}" || exit 1

# debug
# remove the slow make-mpc if debugging
${SCRIPT_DIR}/../imgmaker/mpcimg ${MAKE_MODEL} "${ROOTFS}" "${TMP_DIR}/${CFW_IMG}" "${FIRMWARE_VERSION}"
if [ -f "${TMP_DIR}/${CFW_IMG}" ]; then
    sha512sum "${TMP_DIR}/${CFW_IMG}" > "${TMP_DIR}/${CFW_IMG}".sha512sum.txt
fi

mv "${TMP_DIR}/${CFW_IMG}" "${SCRIPT_DIR}/../firmwares"
mv "${TMP_DIR}/${CFW_IMG}".sha512sum.txt "${SCRIPT_DIR}/../firmwares"

if [ -z "$DEBUG_SCRIPT" ]
then
    rm -rf ${TMP_DIR}
fi

