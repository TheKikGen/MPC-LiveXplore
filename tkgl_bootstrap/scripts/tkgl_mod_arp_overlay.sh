#!/bin/sh
#
# __ __| |           |  /_) |     ___|             |           |
#    |   __ \   _ \  ' /  | |  / |      _ \ __ \   |      _` | __ \   __|
#    |   | | |  __/  . \  |   <  |   |  __/ |   |  |     (   | |   |\__ \
#   _|  _| |_|\___| _|\_\_|_|\_\\____|\___|_|  _| _____|\__,_|_.__/ ____/
#
# BOOTSTRAP script for MPC device.
# arp_overlay - Arp patterns and progression overlay setting on sdcard

source $TKGL_PATH_FILE

echo "*** $0 module">>$TKGL_LOG

# create an overlay for the Arp Patterns/Progressions and others
# That trick only works on ext4 part type, so it is installed on the sdcard here

AKAI_SME0=/usr/share/Akai/SME0
AKAI_SME0_OVR=$MOUNT_POINT/.SME0 # hidden

if [ ! -d $AKAI_SME0_OVR/overlay ]
then
  mkdir -p  "$AKAI_SME0_OVR/overlay"  "$AKAI_SME0_OVR/.work"
  mkdir -p  "$AKAI_SME0_OVR/overlay/Arp Patterns"
  mkdir -p  "$AKAI_SME0_OVR/overlay/Progressions"
  # Create links in the root directory to hidden directories
  # This avoids to pollute display when browsing the sdcard
  ln -s "$AKAI_SME0_OVR/overlay/Arp Patterns" "$MOUNT_POINT/Arp Patterns"
  ln -s "$AKAI_SME0_OVR/overlay/Progressions" "$MOUNT_POINT/Progressions"
fi

# mount the overlay
mount -t overlay overlay -o \
lowerdir=$AKAI_SME0,\
upperdir=$AKAI_SME0_OVR/overlay,\
workdir=$AKAI_SME0_OVR/.work \
$AKAI_SME0>>$TKGL_LOG
