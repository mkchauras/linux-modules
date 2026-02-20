obj-m += prefetch.o

EXTRA_CFLAGS = -I$(src)

KERNEL_SRC ?= /usr/lib/modules/$(shell uname -r)/build

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
