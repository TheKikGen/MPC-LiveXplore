#!/bin/bash

if [ $# -eq 0 ]; then
    echo "No arguments supplied"
    exit 1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
TEMPLATE_DIR=${SCRIPT_DIR}/templates

ROOT_IMG=$1

DELETE_FILE_AUTHORIZED_KEYS=/root/.ssh/authorized_keys

FILE_UID=0
FILE_GID=0
MODE=644

e2cp -O ${FILE_UID} -G ${FILE_GID} -P ${MODE} ${TEMPLATE_DIR}/profile ${ROOT_IMG}:/root/.profile
e2cp -O ${FILE_UID} -G ${FILE_GID} -P ${MODE} ${TEMPLATE_DIR}/tkgl_logo ${ROOT_IMG}:/root/.tkgl_logo

e2rm ${ROOT_IMG}:${DELETE_FILE_AUTHORIZED_KEYS}
