#!/bin/bash

if [ $# -eq 0 ]; then
    echo "No arguments supplied"
    exit 1
fi

ROOT_IMG=$1

SOURCE=/usr/lib/systemd/system/sshd.service
DEST=/etc/systemd/system/multi-user.target.wants/sshd.service

# symbolic links are not implemented in the e2tools
# e2ln -s ${ROOT_IMG}:${SOURCE} ${DEST}

# so do a hardlink
e2ln ${ROOT_IMG}:${SOURCE} ${DEST}
