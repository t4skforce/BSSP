DRIVER=cdrv

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m := $(DRIVER).o

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else

	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -f ./test ./clear ./resize

insmod:
	sudo insmod $(DRIVER).ko
	sleep 1
	sudo chmod 666 /dev/mydev[0-4]
	ls -l /dev/mydev[0-4]

rmmod:
	sudo rmmod -f $(DRIVER)

test:
	gcc -Wall -o test test.c
	gcc -Wall -o clear ioctl_clear.c
	gcc -Wall -o resize ioctl_resize.c
	
endif