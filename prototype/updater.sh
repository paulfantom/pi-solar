#!/bin/bash

# Script is meant to be run in crontab every 30 min.
# Superuser permissions are needed

REMOTE_FILE=https://raw.githubusercontent.com/paulfantom/pi-solar/master/prototype/main.py

function roroot() {
  mount -o remount,ro /
}
trap roroot EXIT 

rm /tmp/remote_main.py &>/dev/null
wget "${REMOTE_FILE}" -O /tmp/remote_main.py &>/dev/null
if diff /opt/solar/main.py /tmp/remote_main.py &>/dev/null ; then
    exit 0
fi

echo "Updating solar program..."
mount -o remount,rw /
cp /tmp/remote_main.py /opt/solar/main.py
systemctl restart solar
