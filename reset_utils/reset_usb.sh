#!/bin/bash

bus_id=$(lsusb | grep Plustek | sed -e 's/.*Bus \(.*\) Device.*/\1/') 
dev_id=$(lsusb | grep Plustek | sed -e 's/.*Device \(.*\): ID.*/\1/')
echo "resetting /dev/bus/usb/$bus_id/$dev_id"
sudo /home/smapa/Scanpy/resetusb /dev/bus/usb/$bus_id/$dev_id

# for cam in /dev/video*
# do
# p=$(udevadm info --query=path $cam)
# bus=$(echo $p | awk -F '/' '{print $(NF-3)}')
# echo -n $bus | sudo tee /sys/bus/usb/drivers/usb/unbind
# echo
# #sleep 1
# echo -n $bus | sudo tee /sys/bus/usb/drivers/usb/bind
# echo
# done;
sleep 1