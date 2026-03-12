#!/bin/bash

TAP_DEV="tap0"
TAP_IP="10.42.42.2"

if [ "$1" == "down" ];then
  sudo ip link delete $TAP_DEV
  echo "TAP device $TAP_DEV removed."
else
  sudo ip tuntap add mode tap dev $TAP_DEV
  sudo ip link set dev $TAP_DEV up
  sudo ip addr add $TAP_IP/24 dev $TAP_DEV
  echo "TAP device $TAP_DEV is up with IP $TAP_IP"
  echo "Pinging an Ip in the same subnet to test it"
fi

