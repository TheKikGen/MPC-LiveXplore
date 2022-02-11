#!/bin/bash

if [ $# -eq 0 ]; then
    echo "No arguments supplied"
    exit 1
fi

ROOT_IMG=$1

e2rm -r ${ROOT_IMG}:/usr/share/Akai/ACV5TestApp
e2rm -r ${ROOT_IMG}:/usr/share/Akai/ACV8TestApp
e2rm -r ${ROOT_IMG}:/usr/share/Akai/ACVATestApp
e2rm -r ${ROOT_IMG}:/usr/share/Akai/ACVBTestApp
e2rm -r ${ROOT_IMG}:/usr/share/Akai/ACVMTestApp
