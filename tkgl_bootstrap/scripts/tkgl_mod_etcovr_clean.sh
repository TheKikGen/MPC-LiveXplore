#!/bin/sh
#
# __ __| |           |  /_) |     ___|             |           |
#    |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
#    |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
#   _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
#
# BOOTSTRAP script for MPC device.
# etcovr_clean : clean the /etc overlay : passworrds files and ssh config

source $TKGL_PATH_FILE

echo "*** $0 module">>$TKGL_LOG

# /etc overlay on the internal system sd card
ETC_OVR="/media/az01-internal/system/etc/overlay"

# clean sshd config on the /etc overlay so we are sure to use the right one
rm "$ETC_OVR/ssh/sshd_config">>$TKGL_LOG

# Erase any passwd/group/shadow file on the etc/overlay
# 20-12-05 Note : from the 3.0.6 version, root account has a password on Force.
rm "$ETC_OVR/shadow">>$TKGL_LOG
rm "$ETC_OVR/shadow-">>$TKGL_LOG
rm "$ETC_OVR/passwd">>$TKGL_LOG
rm "$ETC_OVR/passwd-">>$TKGL_LOG
rm "$ETC_OVR/group">>$TKGL_LOG
rm "$ETC_OVR/group-">>$TKGL_LOG

echo "/etc overlay cleansing done.">>$TKGL_LOG
