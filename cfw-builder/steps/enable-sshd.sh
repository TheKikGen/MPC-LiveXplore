#!/bin/bash

if [ $# -eq 0 ]; then
    echo "No arguments supplied"
    exit 1
fi

ROOT_IMG=$1

FILE=/etc/ssh/sshd_config
FILE_UID=0
FILE_GID=0
MODE=644

TMP_FILE=$(mktemp)

# copy to stdout
e2cp ${ROOT_IMG}:${FILE} - > ${TMP_FILE}

sed -i 's/^PermitRootLogin.*$/PermitRootLogin yes/' ${TMP_FILE}
sed -i 's/^PasswordAuthentication.*$/PasswordAuthentication yes/' ${TMP_FILE}
sed -i 's/^#PermitEmptyPasswords.*$/PermitEmptyPasswords yes/' ${TMP_FILE}

# copy it back
e2cp -O ${FILE_UID} -G ${FILE_GID} -P ${MODE} ${TMP_FILE} ${ROOT_IMG}:${FILE}
