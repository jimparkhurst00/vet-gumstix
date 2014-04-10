#############################################################################
#
#	Makefile for building the hello-module sample module
#
#############################################################################

#----------------------------------------------------------------------------
# call from kernel build system

obj-m	:= hello-module.o

#
#----------------------------------------------------------------------------

#----------------------------------------------------------------------------
#

PWD                 := $(shell pwd)

KERNELDIR 		    = ../
CROSS_COMPILE		= arm-linux-gnueabi- 

kernel-module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module.symvers

#
#----------------------------------------------------------------------------


