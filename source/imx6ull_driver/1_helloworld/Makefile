# -m 表示编译成模块
obj-m += hello.o
# 以绝对路径的方式指向内核源码路径
KDIR:=/home/pdg/ARM-Linux/current_uboot_linux/linux-imx-rel_imx_4.1.15_2.1.0_ga
# Makefile 所在路径
PWD?=$(shell pwd)
# 进入到Makefile路径，并使用该目录下的源码编译驱动模块
all:
	make -C $(KDIR) M=$(PWD) modules
# 删除产生的文件
clean:
	rm -r *.ko *.mod.o *.mod.c *.symvers *.order