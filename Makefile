obj-m := asus_u36sd_fan.o

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

default: clean build copy dkms-add dkms-build dkms-install load
	@echo "Done"

build:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

unload:
	-/sbin/rmmod asus_u36sd_fan

load: unload
	/sbin/insmod asus_u36sd_fan.ko

clean: dkms-remove
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	-rm -rf /usr/src/asus_u36sd_fan-0.1

dkms-add:
	/usr/sbin/dkms add -m asus_u36sd_fan -v 0.1

dkms-build:
	/usr/sbin/dkms build -m asus_u36sd_fan -v 0.1

dkms-install:
	/usr/sbin/dkms install -m asus_u36sd_fan -v 0.1

dkms-remove:
	/usr/sbin/dkms remove asus_u36sd_fan/0.1 --all

copy:
	-mkdir /usr/src/asus_u36sd_fan-0.1
	cp -a * /usr/src/asus_u36sd_fan-0.1/
