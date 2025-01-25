#!bin/bash

module="scull"
device="scull"
mode="664"
# invoke insmod with all arguments we got
# and use a pathname, as newer modutils don't look in . by default
sudo /sbin/insmod ./$module.ko $* || exit 1
# remove stale nodes
rm -f /dev/${device}[0-3]
major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)
sudo mknod /dev/${device}0 c $major 0
sudo mknod /dev/${device}1 c $major 1
sudo mknod /dev/${device}2 c $major 2
sudo mknod /dev/${device}3 c $major 3
# give appropriate group/permissions, and change the group.
# Not all distributions have staff, some have "wheel" instead.
# group="staff"
# grep -q '^staff:' /etc/group || group="wheel"
# chgrp $group /dev/${device}[0-3]
# chmod $mode /dev/${device}[0-3]
echo "Value printed after running the command: $(ls -l /dev/ | grep 'scull')"
