#!/bin/sh
#
# __ __| |           |  /_) |     ___|             |           |
#    |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
#    |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
#   _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
#
# BOOTSTRAP script for MPC device.
# Telnet launcher module
source $TKGL_PATH_FILE

# Use Busybox telnetd,a light telnet server.
echo "*** $0 module">>$TKGL_LOG
busybox telnetd>>$TKGL_LOG
