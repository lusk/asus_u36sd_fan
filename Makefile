obj-m := asus_u36sd_fan.o

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

default:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

install:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules_install

load:
	-/sbin/rmmod asus_u36sd_fan
	/sbin/insmod asus_u36sd_fan.ko

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

dkms-add:
	/usr/sbin/dkms add $(CURDIR)

dkms-build:
	/usr/sbin/dkms build asus_u36sd_fan/0.1

dkms-remove:
	/usr/sbin/dkms remove asus_u36sd_fan/0.1 --all

