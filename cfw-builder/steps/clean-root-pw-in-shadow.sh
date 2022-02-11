#!/bin/bash

if [ $# -eq 0 ]; then
    echo "No arguments supplied"
    exit 1
fi

cleaning() {
  rm -rf ${TMP_FILE}
}

trap cleaning EXIT HUP INT QUIT TERM STOP PWR

ROOT_IMG=$1

FILE=/etc/shadow
FILE_UID=0
FILE_GID=0
MODE=600

TMP_FILE=$(mktemp)

# copy to stdout
e2cp ${ROOT_IMG}:${FILE} - > ${TMP_FILE}

# replace root password with empty password root::::::::
sed -i 's/root.*$/root::::::::/' ${TMP_FILE}

# copy it back
e2cp -O ${FILE_UID} -G ${FILE_GID} -P ${MODE} ${TMP_FILE} ${ROOT_IMG}:${FILE}
