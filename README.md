ASUS U36SD Fan Control (v.0.1)
========================================

This acpi driver, based on previous work by Giulio Piemontese
(http://github.com/gpiemont/asusfan/) and Dmitry Ursegov 
(http://code.google.com/p/asusfan/),
aims to provide a simple way to manage and monitor system fan speed 
of ASUS U36SD.

For more information on setting speeds and monitoring visit: (http://github.com/gpiemont/asusfan/)

Beware!
=======

Since the driver overrides the automatic fan speed handling it might damage your hardware!
I'm using it daily in production and had no problems so far, but use it at your own risk!
You've been warned :)

Installation:
=============

You should be fine with the defaults, but you might want to first modify temperature zone table inside asus_u36sd_fan.c to suit your needs

* git clone https://github.com/lusk/asus_u36sd_fan.git
* cd asus_u36sd_fan-master
* sudo su
* mkdir /usr/src/asus_u36sd_fan-0.1
* cp -a * /usr/src/asus_u36sd_fan-0.1/
* dkms add -m asus_u36sd_fan -v 0.1
* dkms build -m asus_u36sd_fan -v 0.1
* dkms install -m asus_u36sd_fan -v 0.1
* add a line with asus_u36sd_fan in /etc/modules if you want it to be loaded automaticaly during boot

If you are using this..
=======================

...just say thanks in a short e-mail and we're even, ok? :D
