
ifneq	($(KERNELRELEASE),)
obj-m	:= dram.o sys_reg.o

else
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)
default:	
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules 
	rm -r -f .tmp_versions *.mod.c .*.cmd *.o *.symvers *.order *.markers 
clean:
	rm -f *.ko
endif

