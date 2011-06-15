KERNEL_DIR = /lib/modules/$(shell uname -r)/build
#KERNEL_DIR = /usr/src/kernels/linux-2.6.18.1/
all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
	mkdir -p build/
	mv -f *.o *.mod.c *.ko Module.markers Module.symvers build/

run:
	sudo /etc/init.d/netconsole restart
	make install

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
	rm -rf build/

install:	build/ktcphabe.ko
	sudo /sbin/insmod build/ktcphabe.ko

uninstall:
	sudo /sbin/rmmod ktcphabe

test:
	make install
	make uninstall
	make test
	make uninstall

BUILD_DIR := build

obj-m := ktcphabe.o

ktcphabe-objs := tcpha_be.o tcpha_be_fe_connection.o tcpha_be_handoff_connection.o
