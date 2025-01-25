#!/bin/bash

module="scull"
device="scull"

# Unload the module
sudo /sbin/rmmod $module

# Remove device nodes
sudo rm -f /dev/${device}[0-3]

echo "Value printed after running the command: $(ls -l /dev/ | grep 'scull')"
