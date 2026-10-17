#!/bin/sh
#
# __ __| |           |  /_) |     ___|             |           |
#    |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
#    |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
#   _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
#
# autorun static script for MPC device. 
# https://github.com/TheKikGen/MPC-LiveXplore

SCRIPT_NAME=$(readlink -f "$0")
SCRIPT_DIR=$(dirname "$SCRIPT_NAME")

TKGL_SUB_SCRIPT_NAME=$SCRIPT_DIR/tkgl_sub_script.sh

echo "The KikGen Labs- Autorun for MPC family"

if [ -f "$TKGL_SUB_SCRIPT_NAME" ]
then
     . "$TKGL_SUB_SCRIPT_NAME"    
     EXIT_CODE=$?
else
    echo "Looks like there is not $SCRIPT_DIR/tkgl_sub_script.sh file here ! - exiting"
    exit 1
fi

echo "tkgl_sub_script.sh has ended with code $EXIT_CODE."
exit $EXIT_CODE
