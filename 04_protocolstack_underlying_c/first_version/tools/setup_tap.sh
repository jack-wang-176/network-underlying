#!/bin/bash

TAP_DEV="tap0"
HOST_IP="10.42.42.1" 

if [ "$1" == "down" ];then
  sudo ip link delete $TAP_DEV
  echo "TAP device $TAP_DEV removed."
else
  sudo ip tuntap add mode tap dev $TAP_DEV
  sudo ip link set dev $TAP_DEV up
  sudo ip addr add $HOST_IP/24 dev $TAP_DEV
  echo "TAP device $TAP_DEV is up with HOST IP $HOST_IP"
fi