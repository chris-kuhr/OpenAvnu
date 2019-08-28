#!/bin/bash
# Simple script to run igb_avb

if [ "$#" -eq "0" ]; then 
    echo "please enter network interface name as parameter. For example:"
    echo "sudo ./run_igb.sh enp8s0"
    exit -1
fi

rmmod igb
modprobe i2c_algo_bit
modprobe dca
modprobe ptp
insmod /home/christoph/source_code/github-kuhr/OpenAvnu.git/lib/igb_avb/kmod/igb_avb.ko

ethtool -i $1
