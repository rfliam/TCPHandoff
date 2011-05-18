KERNEL_DIR = /lib/modules/$(shell uname -r)/build
#KERNEL_DIR = /usr/src/kernels/linux-2.6.18.1/
all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
	mkdir -p build/
	mv -f *.o *.mod.c *.ko Module.markers Module.symvers build/
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
	rm -rf build/

install:	build/ktcphafe.ko
	sudo /sbin/insmod build/ktcphafe.ko
	/sbin/lsmod | less

uninstall:
	sudo /sbin/rmmod ktcphafe

BUILD_DIR := build

obj-m := ktcphafe.o

ktcphafe-objs := tcpha_fe.o tcpha_fe_server.o tcpha_fe_client_connection.o tcpha_fe_poll.o tcpha_fe_connection_processor.o tcpha_fe_http.o
