# scull module
obj-m := scull.o
scull-objs := main.o util.o


all:
	@echo "Kernel version: $(shell uname -r)"
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
