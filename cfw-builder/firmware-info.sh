#!/bin/bash

if [ $# -ne 2 ]; then
    echo "wrong number of arguments"
    exit 1
fi

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

FIRMWARE_IMG=$(echo $2 |sed 's/^.*\///')

if [ ! -f "${SCRIPT_DIR}/../firmwares/${FIRMWARE_IMG}" ]; then
    echo "error: firmwares/${FIRMWARE_IMG} does not exists"
    exit 1
fi
FIRMWARE="${SCRIPT_DIR}/../firmwares/${FIRMWARE_IMG}"

FIRMWARE_DATABASE_JSON=${SCRIPT_DIR}/firmwares.json
SHA512SUM=$(sha512sum "${FIRMWARE}" | sed 's/\s.*//')

#echo FIRMWARE_DATABASE_JSON: ${FIRMWARE_DATABASE_JSON}
#echo SHA512SUM: ${SHA512SUM}

case $1 in
  "model")
        FIRMWARE_MODEL=$(jq '.[] | select(.sha512sum=="'${SHA512SUM}'")' "${FIRMWARE_DATABASE_JSON}" | jq -r ."model" | awk '{print tolower($0)}')

        if [ -z "${FIRMWARE_MODEL}" ]
        then
            echo "error: unknown MPC or Force firmware - might be already a CFW or a new firmware (please update firmwares.json)" >&2
            exit 1
        fi

        echo ${FIRMWARE_MODEL}
        exit
    ;;

  "info")
        ${SCRIPT_DIR}/../imgmaker/mpcimg info "${FIRMWARE}"

        FIRMWARE=$(jq '.[] | select(.sha512sum=="'${SHA512SUM}'")' "${FIRMWARE_DATABASE_JSON}")

        if [ -z "${FIRMWARE}" ]
        then
            echo "error: unknown MPC or Force firmware - might be already a CFW or a new firmware (please update firmwares.json)" >&2
            exit 1
        fi

        echo ${FIRMWARE}
        exit
    ;;

  *)
        echo "error: unknown command $2" >&2
        exit 1
    ;;
esac



