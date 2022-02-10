#!/bin/bash

if [ $# -eq 0 ]; then
    echo "No arguments supplied"
    exit 1
fi

cleaning() {
  rm -rf ${TMP_FILE}
}

trap cleaning EXIT HUP INT QUIT TERM STOP PWR

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
TEMPLATE_DIR=${SCRIPT_DIR}/templates

ROOT_IMG=$1

TEMPLATE=${TEMPLATE_DIR}/az01-launch-MPC
FILE=/usr/bin/az01-launch-MPC
FILE_UID=0
FILE_GID=0
MODE=755

TMP_FILE=$(mktemp)

# copy template
cp ${TEMPLATE} ${TMP_FILE}

# copy to stdout and remove fist line and change "$@" to $ARGV
e2cp ${ROOT_IMG}:${FILE} - | sed -e '1d' | sed -e 's/\"\$@\"/\$ARGV/g' >> ${TMP_FILE}

# copy it back
e2cp -O ${FILE_UID} -G ${FILE_GID} -P ${MODE} ${TMP_FILE} ${ROOT_IMG}:${FILE}
