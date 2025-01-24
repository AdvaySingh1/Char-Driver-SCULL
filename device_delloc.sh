#!/bin/bash

module="scull"
device="scull"

# Unload the module
/sbin/rmmod $module

# Remove device nodes
rm -f /dev/${device}[0-3]