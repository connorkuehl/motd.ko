#!/bin/sh

module="motd"
device="motd"

/sbin/rmmod motd || exit 1

rm -f /dev/${device}0
