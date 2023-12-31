# 基础知识

## 内存

### 页表

+ 一级页表工作原理：

  32位虚拟地址：0x12345678，高12位(0x123)用于索引页表项，24位(0x45678)表示页内偏移。

  以0x123为索引找到一级页表项(数值为0x812XXXXXX)，则0x812000000为映射到的1MB物理地址页。

  以0x812替代0x123得到物理地址0x81245678。

+ 二级页表工作原理：

  32位虚拟地址：0x12345678，高12位(0x123)用于索引一级页表，后8位(0x45)用于索引二级页表。

  以0x123为索引找到一级页表项，从内容中获得对应的二级页表基地址，以及页大小。

  根据对应的二级页表基地址，以0x45为索引找到二级页表项，获得4K物理页基地址(0x83712000)。

  根据找到的物理页基地址，加上页内偏移(0x678)得到真实物理地址(0x83712678)。



一级页表项里的内容，末尾两位决定了它是指向一块物理内存，还是指问二级页表，如下图：

![image-20230630010633688](.\linux 驱动.assets\image-20230630010633688.png)



### mmap

应用程序调用mmap，最终会调用到驱动程序的 mmap。

![image-20230630014646124](.\linux 驱动.assets\image-20230630014646124.png)





### cache 与 buffer

![image-20230630015450655](.\linux 驱动.assets\image-20230630015450655.png)



+ 读取内存 addr 处的数据时：

  1. 先看看 cache 中有没有 addr 的数据，如果有就直接从 cache 里返回数据：这被称为 cache 命中。

  2. 如果 cache 中没有 addr 的数据，则从内存里把数据读入，注意：它不是仅仅读入一个数据，而是读入一行数据(cache line)。而 CPU 很可能会再次用到这个 addr 或 它附近的数据，这时就可以快速地从 cache 中获得数据。

+ 写通(write through)：

  1. 数据要**同时写入 cache 和内存**，所以 cache 和内存中的数据保持一致，但是它的写效率很低。可以通过 buffer 优化，buffer 会自动把 cache 中的数据写入内存。

  2. 有些 buffer 有“写合并”的功能，比如 CPU 连续写同一个word中的4个字节，写缓冲器会把这 4 个写操作合并成一个写操作。

+ 写回(write back)：

  1. 新数据只是写入 cache，**不会立刻写入内存**，cache 和内存中的数据并不一致。
  2. 新数据写入 cache 时，这一行 cache 被标为“脏”(dirty)；当cache 不够用时，才需要把脏的数据写入内存。

根据是否使用 cache 和 buffer ，可以有4种组合。



以下几种场景需要避免 cache 的使用：

+ 外设寄存器（register）
+ 显存（frame buffer）
+ DMA访问区域





## 中断

### 共享中断 与 非共享中断

![image-20230627223514668](.\linux 驱动.assets\image-20230627223514668.png)

共享中断：多个外设共用同一个中断源。

非共享中断：一个中断源只有一个外设触发。

在linux内核中，每个中断源都有一个整数与之对应。

### 中断号

中断号这个概念可能指**软件中断号**或者**硬件中断号**。

+ 硬件中断号

  以GPIO为例，GPIO1 和 GPIO2 都有 5号中断（硬件中断号），对于两者来说分别代表了不同的引脚。

+ 软件中断号

  linux 内核对于每一个硬件中断都分配了一个软件中断号，根据软件中断号可以区分每个硬件设备的中断。



### linux 中断向量表处理

linux 4.1.15 源码

```c
// arch\arm64\kernel\entry.S

// 异常向量表 源码，其中 ventry 是汇编宏，作用是使用 指令b 跳转
	.text
	.align	11
ENTRY(vectors)
	ventry	el1_sync_invalid		// Synchronous EL1t
	ventry	el1_irq_invalid			// IRQ EL1t
	ventry	el1_fiq_invalid			// FIQ EL1t
	ventry	el1_error_invalid		// Error EL1t

	ventry	el1_sync			// Synchronous EL1h
	ventry	el1_irq				// IRQ EL1h
	ventry	el1_fiq_invalid			// FIQ EL1h
	ventry	el1_error_invalid		// Error EL1h

	ventry	el0_sync			// Synchronous 64-bit EL0
	ventry	el0_irq				// IRQ 64-bit EL0
	ventry	el0_fiq_invalid			// FIQ 64-bit EL0
	ventry	el0_error_invalid		// Error 64-bit EL0

#ifdef CONFIG_COMPAT
	ventry	el0_sync_compat			// Synchronous 32-bit EL0
	ventry	el0_irq_compat			// IRQ 32-bit EL0
	ventry	el0_fiq_invalid_compat		// FIQ 32-bit EL0
	ventry	el0_error_invalid_compat	// Error 32-bit EL0
#else
	ventry	el0_sync_invalid		// Synchronous 32-bit EL0
	ventry	el0_irq_invalid			// IRQ 32-bit EL0
	ventry	el0_fiq_invalid			// FIQ 32-bit EL0
	ventry	el0_error_invalid		// Error 32-bit EL0
#endif
END(vectors)


// el0_irq 对应代码，省略了条件编译部分
	.align	6
el0_irq:
	kernel_entry 0	// 一些准备工作，如保存寄存器， 参数0为异常等级
el0_irq_naked:
	enable_dbg		// debug相关 展开后为：  msr	daifclr, #8
	ct_user_exit	// 与编译宏相关，可能为空
	irq_handler		// 跳转到中断处理
	b	ret_to_user // 返回用户空间的处理，会调用 kernel_exit
ENDPROC(el0_irq)

        
// IRQ SYNC 中断都会先调用 kernel_entry
    .macro	kernel_entry, el, regsize = 64
	sub	sp, sp, #S_FRAME_SIZE
	.if	\regsize == 32
	mov	w0, w0				// 如果为32位模式，清除 x0 高32位
	.endif
    // 保存寄存器 x0-x29
	stp	x0, x1, [sp, #16 * 0]
	stp	x2, x3, [sp, #16 * 1]
	stp	x4, x5, [sp, #16 * 2]
	stp	x6, x7, [sp, #16 * 3]
	stp	x8, x9, [sp, #16 * 4]
	stp	x10, x11, [sp, #16 * 5]
	stp	x12, x13, [sp, #16 * 6]
	stp	x14, x15, [sp, #16 * 7]
	stp	x16, x17, [sp, #16 * 8]
	stp	x18, x19, [sp, #16 * 9]
	stp	x20, x21, [sp, #16 * 10]
	stp	x22, x23, [sp, #16 * 11]
	stp	x24, x25, [sp, #16 * 12]
	stp	x26, x27, [sp, #16 * 13]
	stp	x28, x29, [sp, #16 * 14]

	.if	\el == 0				// EL0，异常等级为0，用户模式
	mrs	x21, sp_el0
	get_thread_info tsk			// Ensure MDSCR_EL1.SS is clear,
	ldr	x19, [tsk, #TI_FLAGS]		// since we can unmask debug
	disable_step_tsk x19, x20		// exceptions when scheduling.
	.else						// 异常等级不为0，特权模式
	add	x21, sp, #S_FRAME_SIZE
	.endif
	mrs	x22, elr_el1
	mrs	x23, spsr_el1
	stp	lr, x21, [sp, #S_LR]
	stp	x22, x23, [sp, #S_PC]
	/*
	 * 设置 syscallno 为默认值 -1 (后续真正系统调用时会覆盖).
	 */
	.if	\el == 0
	mvn	x21, xzr
	str	x21, [sp, #S_SYSCALLNO]
	.endif
	/*
	 * Registers that may be useful after this macro is invoked:
	 *
	 * x21 - aborted SP
	 * x22 - aborted PC
	 * x23 - aborted PSTATE
	*/
	.endm
        
// 中断处理 跳转到 handle_arch_irq，为 C语言定义的 函数指针全局变量
	.macro	irq_handler
	adrp	x1, handle_arch_irq
	ldr	x1, [x1, #:lo12:handle_arch_irq]
	mov	x0, sp
	blr	x1
	.endm
        
// 返回用户空间， 会调用 kernel_exit
ret_to_user:
	disable_irq				// disable interrupts
	ldr	x1, [tsk, #TI_FLAGS]
	and	x2, x1, #_TIF_WORK_MASK
	cbnz	x2, work_pending
	enable_step_tsk x1, x2
no_work_pending:
	kernel_exit 0, ret = 0
ENDPROC(ret_to_user)

// 退出内核，会恢复 kernel_entry 保存的寄存器，省略部分条件编译代码
    .macro	kernel_exit, el, ret = 0
	ldp	x21, x22, [sp, #S_PC]		// load ELR, SPSR
    // EL0
	.if	\el == 0
	ct_user_enter
	ldr	x23, [sp, #S_SP]		// load return stack pointer
	msr	sp_el0, x23
	alternative_insn						\
	"nop",								\
	"msr contextidr_el1, xzr; 1:",					\
	ARM64_WORKAROUND_845719
	.endif
   
	msr	elr_el1, x21			// set up the return data
	msr	spsr_el1, x22

	.if	\ret
	ldr	x1, [sp, #S_X1]			// preserve x0 (syscall return)
	.else
	ldp	x0, x1, [sp, #16 * 0]
	.endif

	ldp	x2, x3, [sp, #16 * 1]
	ldp	x4, x5, [sp, #16 * 2]
	ldp	x6, x7, [sp, #16 * 3]
	ldp	x8, x9, [sp, #16 * 4]
	ldp	x10, x11, [sp, #16 * 5]
	ldp	x12, x13, [sp, #16 * 6]
	ldp	x14, x15, [sp, #16 * 7]
	ldp	x16, x17, [sp, #16 * 8]
	ldp	x18, x19, [sp, #16 * 9]
	ldp	x20, x21, [sp, #16 * 10]
	ldp	x22, x23, [sp, #16 * 11]
	ldp	x24, x25, [sp, #16 * 12]
	ldp	x26, x27, [sp, #16 * 13]
	ldp	x28, x29, [sp, #16 * 14]
	ldr	lr, [sp, #S_LR]
	add	sp, sp, #S_FRAME_SIZE		// restore sp
	eret					// return to kernel
	.endm
```

ARM64架构的中断向量表定义于 arch\arm64\kernel\entry.S

中断处理为  C语言定义的全局函数指针 handle_arch_irq， 该 函数指针 定义于 arch\arm64\kernel\irq.c

C语言函数 set_handle_irq 用于赋值 handle_arch_irq，其被中断控制器（如GIC）的驱动代码调用。

中断控制器的驱动代码位于 drivers\irqchip\ 目录下，文件如  irq-gic.c、irq-gic-v3.c 等

```c
// drivers\irqchip\irq-gic.c
static void __exception_irq_entry gic_handle_irq(struct pt_regs *regs)
{
	u32 irqstat, irqnr;
	struct gic_chip_data *gic = &gic_data[0];
	void __iomem *cpu_base = gic_data_cpu_base(gic);

	do {
		irqstat = readl_relaxed(cpu_base + GIC_CPU_INTACK);
		irqnr = irqstat & GICC_IAR_INT_ID_MASK;

		if (likely(irqnr > 15 && irqnr < 1021)) {
			handle_domain_irq(gic->domain, irqnr, regs);
			continue;
		}
		if (irqnr < 16) {
			writel_relaxed(irqstat, cpu_base + GIC_CPU_EOI);
#ifdef CONFIG_SMP
			handle_IPI(irqnr, regs);
#endif
			continue;
		}
		break;
	} while (1);
}
```

## 系统调用

系统调用是操作系统提供给编程人员的接口。因为上层应用不能直接操作硬件，只能通过系统调用来请求操作系统的服务。

系统调用会伴随着系统调用号，系统调用号可以让内核区分请求的服务。

内核代码中 使用宏 __SYSCALL(x, y) 来把 **系统调用号x** 绑定到 **内核函数y**，用户空间代码 一般把系统调用函数syscall封装成其他函数。一般用户空间调用函数 xxx()，对应内核函数 sys_xxx()。

### 自定义系统调用

linux内核提供 宏 来定义系统调用相关内核函数。

```c
#include <linux/syscalls.h>
/* 用来定义内核函数的宏，无参数 */
#define SYSCALL_DEFINE0(sname)					\
	SYSCALL_METADATA(_##sname, 0);				\
	asmlinkage long sys_##sname(void)
/* 用来定义内核函数的宏，最多6个参数 */
#define SYSCALL_DEFINE1(name, ...) SYSCALL_DEFINEx(1, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE2(name, ...) SYSCALL_DEFINEx(2, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE3(name, ...) SYSCALL_DEFINEx(3, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE4(name, ...) SYSCALL_DEFINEx(4, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE5(name, ...) SYSCALL_DEFINEx(5, _##name, __VA_ARGS__)
#define SYSCALL_DEFINE6(name, ...) SYSCALL_DEFINEx(6, _##name, __VA_ARGS__)
```

创建系统调用步骤：

1. 在源文件中使用相应宏定义，定义并实现内核函数。
2. 在 include/uapi/asm-generic/unistd.h 定义系统调用号，并与内核函数绑定。
3. 把代码编译进内核，烧录到开发板。

应用程序使用系统调用步骤：

1. 调用函数 syscall()，第一个参数为系统调用号，其余可变参数由内核函数定义。



# 驱动基础

## 最简单驱动

```c
/* 必须包含的两个头文件 */
#include <linux/module.h>
#include <linux/init.h>

// 加载函数
static int helloworld_init(void)
{
    printk("helloworld_init\n");
    return 0;
}
// 卸载函数
static void helloworld_exit(void)
{
    printk("helloworld_exit\n");
}
/* 必须使用宏指定 加载函数、卸载函数、GPL声明 */
module_init(helloworld_init);
module_exit(helloworld_exit);
MODULE_LICENSE("GPL");
// 可选的 作者、版本等信息
MODULE_AUTHOR("pdg");
MODULE_VERSION("v1.0");
```

以上代码展示了一个内核模块最精简的框架，他不关联任何的硬件，也不创建设备文件。

驱动编译方式：

1. 把驱动编译进内核，内核启动时自动加载
2. 把驱动编译成模块，使用 insmod命令 动态加载

编译成模块的 Makefile 模板

```makefile
obj-m += helloworld.o	# -m 表示编译成模块
KDIR:=/path/to/linux-kernel # 以绝对路径的方式指向内核源码路径
PWD?=$(shell pwd)		# Makefile 所在路径

# 进入到Makefile路径，并使用该目录下的源码编译驱动模块
all:
	make -C $(KDIR) M=$(PWD) modules
# 删除产生的文件
clean:
	rm -r *.ko *.mod.o *.mod.c *.symvers *.order
```

路径指定的内核源码必须编译过，否则无法编译内核模块。

编译前需要设置 ARCH 和 CROSS_COMPILE 环境变量。

## 字符设备

```c
#include <linux/cdev.h> // 字符设备
#include <linux/fs.h> // 设备号 文件操作
struct cdev {
	struct kobject kobj;
	struct module *owner;
	const struct file_operations *ops; // 文件操作方法集
	struct list_head list;
	dev_t dev; // 设备号 12位主设备号 + 20位次设备号
	unsigned int count;
};
// 初始化 cdev结构体
void cdev_init(
    struct cdev *, // 需要初始化的结构体
    const struct file_operations * // 对应的文件操作
);

// 注册某个范围的设备号，成功返回0，失败返回负数
int register_chrdev_region(
    dev_t from,  // 设备号的起始值，含主、次设备号
    unsigned count, // 次设备号的数量
    const char *name // 设备的名称
);

// 系统自动分配设备号并注册， 成功返回0，失败返回负数
int alloc_chrdev_region(
    dev_t *dev, // 输出分配得到的 主设备号
    unsigned baseminor, // 次设备号的起始值，一般为0
    unsigned count, // 要申请的设备号数量
    const char *name // 设备的名字
);

// 把字符设备添加到系统中
int cdev_add(
    struct cdev *, // 已经初始化的字符设备
    dev_t, // 对应的设备号
    unsigned // 设备的数量，它们的次设备号是连续的
);

// 注销分配得到的设备号
int unregister_chrdev_region(
    dev_t dev,  // 要释放的设备号，含主、次设备号
    unsigned count, // 设备号的数量
);

// 把字符设备删除
void cdev_del(struct cdev *);
```

在模块初始化时 注册字符设备：

1. 创建 struct cdev结构体，可以动态分配或者直接定义
2. 初始化 struct cdev 结构体，绑定 file_operations
3. 获取设备号
4. 调用cdev_init和cdev_add添加字符设备
5. 可创建class或者device文件

在模块卸载时 注销字符设备：

1. 销毁class和device文件
2. 调用cdev_del删除字符设备（会释放cdev_alloc分配的内存）
3. 注销获取的设备号



次设备号怎么使用完全由驱动程序决定，一般是用来选择某个设备，也可以和主设备号结合，再用来找到其他驱动程序。

不论是添加字符设备(cdev_add)、创建设备文件(device_create)、销毁设备文件(device_destory) 都需要关联设备号

## 文件操作

```c
#include <linux/fs.h>

struct file_operations {
	struct module *owner; // 拥有该结构体的模块，一般是this module
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
	ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
	int (*iterate) (struct file *, struct dir_context *);
	unsigned int (*poll) (struct file *, struct poll_table_struct *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	int (*mremap)(struct file *, struct vm_area_struct *);
	int (*open) (struct inode *, struct file *);
	int (*flush) (struct file *, fl_owner_t id);
	int (*release) (struct inode *, struct file *);
	int (*fsync) (struct file *, loff_t, loff_t, int datasync);
	int (*aio_fsync) (struct kiocb *, int datasync);
	int (*fasync) (int, struct file *, int);
	int (*lock) (struct file *, int, struct file_lock *);
	ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
	unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
	int (*check_flags)(int);
	int (*flock) (struct file *, int, struct file_lock *);
	ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
	ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
	int (*setlease)(struct file *, long, struct file_lock **, void **);
	long (*fallocate)(struct file *file, int mode, loff_t offset,
			  loff_t len);
	void (*show_fdinfo)(struct seq_file *m, struct file *f);
#ifndef CONFIG_MMU
	unsigned (*mmap_capabilities)(struct file *);
#endif
};
```

owner 拥有该结构体的模块的指针，一般设置为 THIS_MODULE。

llseek 函数用于修改文件当前的读写位置。

read 函数用于读取设备文件。

write 函数用于向设备文件写入(发送)数据。

poll 是个轮询函数，用于查询设备是否可以进行非阻塞的读写。

unlocked_ioctl 函数提供对于设备的控制功能，与应用程序中的 ioctl 函数对应。

compat_ioctl 函数与 unlocked_ioctl 函数功能一样，区别在于在 64 位系统上的 32 位的应用程序调用将会使用此函数。在 32 位的系统上运行 32 位的应用程序调用的是unlocked_ioctl。

mmap 函数用于将将设备的内存映射到进程空间中(也就是用户空间)，一般帧缓冲设备会使用此函数，比如 LCD 驱动的显存，将帧缓冲(LCD 显存)映射到用户空间中以后应用程序就可以直接操作显存了，这样就不用在用户空间和内核空间之间来回复制。

open 函数用于打开设备文件，与应用程序中的 open 函数对应。

release 函数用于释放(关闭)设备文件，与应用程序中的 close 函数对应。

fasync 函数用于刷新待处理的数据，用于将缓冲区中的数据刷新到磁盘中。

aio_fsync 函数与 fasync 函数的功能类似，只是 aio_fsync 是异步刷新待处理的数据。



## 设备节点

```c
struct device_node {
    const char *name; 	/* 节点名字 */
    const char *type; 	/* 设备类型 */
    phandle phandle;
    const char *full_name; /* 节点全名 */
    struct fwnode_handle fwnode;
    struct property *properties; 	/* 属性 */
    struct property *deadprops; 	/* removed 属性 */
    struct device_node *parent; 	/* 父节点 */
    struct device_node *child; 		/* 子节点 */
    struct device_node *sibling;
    struct kobject kobj;
    unsigned long _flags;
    void *data;
#if defined(CONFIG_SPARC)
    const char *path_component_name;
    unsigned int unique_id;
    struct of_irq_controller *irq_trans;
#endif
};

// 创建类于 /sys/class/ ,本质上是宏定义
struct class *class_create(
    struct module *owner, // 拥有者，一般是 THIS_MODULE
    const char *name // 类的名字
);
// 销毁类，模块卸载时调用
void class_destroy(struct class *cls);

// 创建设备节点于 /dev/
struct device *device_create(
    struct class *cls, // class_create 返回的类
    struct device *parent, // 父节点，可为NULL
    dev_t devt, 	// 设备号
    void *drvdata, 	// 设备节点可能会使用的数据，一般为NULL
    const char *fmt, ... // 设备节点的名字，可变参数
);
// 删除节点，模块卸载时调用
void device_destroy(struct class *cls, dev_t devt);
```

可在/dev/目录下查看各种设备节点

+ 手动创建设备节点：

  可以使用命令 mknod + 节点文件 + 设备类型 + 主设备号 + 次设备号，创建设备节点文件 。

+ 自动创建设备节点：

  linux可以使用udev来自动在/sys/class/和/dev/文件夹下创建文件。模块加载时调用class_create和device_create 可以实现。

## 杂项设备

在Linux中，把难以归类的设备定义成杂项设备，杂项设备是特殊的字符设备，使用更简单，更节约设备号。

杂项设备与一般字符设备区别在于：

1. 其主设备号固定位10，不会浪费主设备号。
2. 杂项设备自动创建设备节点，不必要再调用 class_create device_create。

```c
#include <linux/miscdevice.h>

struct miscdevice  {
	int minor;			// 次设备号，赋值为宏 MISC_DYNAMIC_MINOR ，表示系统自动分配
	const char *name;	// 设备名称，也即是 /dev/ 目录下文件名称
	const struct file_operations *fops;	// 文件操作集合
	struct list_head list;
	struct device *parent;
	struct device *this_device;
	const struct attribute_group **groups;
	const char *nodename;
	umode_t mode;
};

// 注册杂项设备
int misc_register(struct miscdevice *misc);

// 卸载杂项设备
int misc_deregister(struct miscdevice *misc);

```



## 驱动技巧

### 文件私有数据

文件结构体 struct file 含有void * private_data; 成员，通过使用该成员，可以让打开的文件关联一组数据。

具体应用场景：使用一套代码，驱动主设备号相同但次设备号不同的设备。

1. 把全局变量用结构体打包起来，创建全局static两个实例，分别给次设备号不同的两个设备使用。
2. 模块加载时，分别创建两个次设备号不同的设备文件 于 /dev 目录下。
3. 打开文件时，根据 inode->i_cdev 可以判断是哪一个设备文件，从而给file->private_data赋值不同的实例。
4. 使用文件时，因为两个设备中 file->private_data中的数据不一样，所以同样的代码可以表现出不同的行为。

优点：

1. 把全局变量打包起来，做成不同的实例，不同的设备文件使用private_data关联不同的实例。
2. 驱动代码（file_operation）不必感知设备的差异，其表现差异取决于private_data中的数据。

### 驱动模块传参

驱动侧可使用3个API接受三种参数类型：

```c
#include <linux/moduleparam.h>
/* 
 * 1. 传递基本类型
 * name 为已经定义的变量标识符
 * type 为数据类型，如 int，char
 * pem 为参数权限
 */
module_param(name, type, pem);
/* 
 * 2. 传递数组
 * name 为已经定义的变量标识符
 * type 为数据类型，如 int
 * nump 用于输出数组元素个数，输入 int型指针
 * pem 为参数权限
 */
module_param_array(name, type, nump, pem);
/* 
 * 3. 传递字符串
 * name 为已经定义的变量标识符
 * string 为字符串缓冲区指针，如 buffer
 * len 为字符串缓冲区长度，如 sizeof(buffer)
 * pem 为参数权限
 */
module_param_string(name, string, len, pem);
/*
 * 定义参数信息，描述如何传参
 * _param 为已经定义的变量标识符
 * desc 为描述信息，输入字符串
 */
MODULE_PARM_DESC(_param, desc);


#include <linux/stat.h>
/* 参数读写权限宏定义 */
// usert 的 读(R)、写(W)、执行(X) 权限定义
#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
// group 的 读(R)、写(W)、执行(X) 权限定义
#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
// other 的 读(R)、写(W)、执行(X) 权限定义
#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001
// 以上宏定义的组合
#define S_IRWXUGO	(S_IRWXU|S_IRWXG|S_IRWXO)
#define S_IALLUGO	(S_ISUID|S_ISGID|S_ISVTX|S_IRWXUGO)
#define S_IRUGO		(S_IRUSR|S_IRGRP|S_IROTH)
#define S_IWUGO		(S_IWUSR|S_IWGRP|S_IWOTH)
#define S_IXUGO		(S_IXUSR|S_IXGRP|S_IXOTH)
```

安装驱动模块时，传参方式

```shell
# 假设模块 param.ko 有三个变量用于接受参数
# static int a; 
# static int array[5];
# static char str[10];
insmod param.ko a=1 array=1,2,3 str=nihao
```



# 运行调试

## 内核模块

```bash
insmod xxx.ko # 加载内核模块
insmod xxx.ko a=1 array=1,2,3 str=hello # 带参数地加载内核模块
lsmod # 查看已经载入的模块，也可以 cat /proc/modules
rmmod xxx.ko # 移除已经加载的内核模块

modinfo --help # 查看模块的作者、描述 等信息
	Usage: modinfo [-adlpn0] [-F keyword] MODULE
        -a              Shortcut for '-F author'
        -d              Shortcut for '-F description'
        -l              Shortcut for '-F license'
        -p              Shortcut for '-F parm'
        -F keyword      Keyword to look for
        -0              Separate output with NULs

modprobe --help # 对默认目录下的模块 进行 加载 等操作
	Usage: modprobe [-alrqvsDb] MODULE [SYMBOL=VALUE]...
        -a      Load multiple MODULEs
        -l      List (MODULE is a pattern)
        -r      Remove MODULE (stacks) or do autoclean
        -q      Quiet
        -v      Verbose
        -s      Log to syslog
        -D      Show dependencies
        -b      Apply blacklist to module names too

```



## 总线/设备文件

```shell
ls /sys/class/ # 查看 class_create 创建的文件夹

ls /dev/ # 查看 device_create 创建的文件(设备节点)，该文件可以使用 open close操作
ls -al /dev/xxx # 可以看出设备类型，主设备号、次设备号

ls /sys/bus/xxx/devices # 查看加载的各种总线(i2c platform..)设备 文件夹形式
ls /sys/bus/xxx/drivers # 查看加载的各种总线(i2c platform..)驱动 文件夹形式

cat /proc/devices # 查看所有系统 已经使用设备号
cat /proc/(pid)/maps # 查看进程使用的 虚拟地址
cat /proc/interrupts # 查看注册的中断
cat /proc/kmsg # 查看内核日志
cat /proc/sys/kernel/printk # 查看终端打印等级
```



## 设备树信息

```bash
ls /sys/firmware/devicetree/base # 查看设备树信息
ls /proc/device-tree # 查看设备树信息
```

所有设备树节点都以文件夹形式存在，可在此查看所有节点、节点属性及其取值。

设备树部分源码如下：

```css
	soc {
		#address-cells = <1>;
		#size-cells = <1>;
		compatible = "simple-bus";
		interrupt-parent = <&gpc>;
		ranges;
		aips1: aips-bus@02000000 {
			compatible = "fsl,aips-bus", "simple-bus";
			#address-cells = <1>;
			#size-cells = <1>;
			reg = <0x02000000 0x100000>;
			ranges;
				uart1: serial@02020000 {
					compatible = "fsl,imx6ul-uart",
						     "fsl,imx6q-uart", "fsl,imx21-uart";
					reg = <0x02020000 0x4000>;
					interrupts = <GIC_SPI 26 IRQ_TYPE_LEVEL_HIGH>;
					clocks = <&clks IMX6UL_CLK_UART1_IPG>,
						 <&clks IMX6UL_CLK_UART1_SERIAL>;
					clock-names = "ipg", "per";
					status = "disabled";
				};
			gpio1: gpio@0209c000 {
				compatible = "fsl,imx6ul-gpio", "fsl,imx35-gpio";
				reg = <0x0209c000 0x4000>;
				interrupts = <GIC_SPI 66 IRQ_TYPE_LEVEL_HIGH>,
					     <GIC_SPI 67 IRQ_TYPE_LEVEL_HIGH>;
				gpio-controller;
				#gpio-cells = <2>;
				interrupt-controller;
				#interrupt-cells = <2>;
			};
		};
	};
```

其中 gpio1 对应节点：

```bash
# imx6ull 节点 soc/aips-bus@02000000/gpio@0209c000
/sys/firmware/devicetree/base/soc/aips-bus@02000000/gpio@0209c000 # ls
#gpio-cells           interrupt-controller  phandle
#interrupt-cells      interrupts            reg
compatible            linux,phandle
gpio-controller       name
```

## 打印信息

linux有打印等级的概念，因此内核中printk打印的信息不一定出现在终端上。

数字可以取 0-7，共八个等级，数字越小优先级越高。

八个等级宏定义 位于 include\linux\kern_levels.h

```c
KERN_EMERG 		/* system is unusable */
KERN_ALERT 		/* action must be taken immediately */
KERN_CRIT 		/* critical conditions */
KERN_ERR 		/* error conditions */
KERN_WARNING 	/* warning conditions */
KERN_NOTICE 	/* normal but significant condition */
KERN_INFO 		/* informational */
KERN_DEBUG 		/* debug-level messages */
```



### 查看日志与打印等级

可以使用 dmesg命令 或 /proc/kmsg文件 查看日志打印。

cat /proc/sys/kernel/printk 会显示四个数字表示打印等级，分别表示：

1. 控制台日志等级（仅显示等级小于它的日志）
2. 默认消息等级（printk 默认的输出优先级）
3. 最低控制台日志等级
4. 默认控制台日志等级

四个等级定义位于内核源码 kernel/printk/printk.c



```shell
cat /proc/kmsg # 查看内核日志
cat /proc/sys/kernel/printk # 查看终端打印等级

dmesg # 显示内核打印信息 
 -C, --clear                 清除内核环形缓冲区(ring butter)
 -c, --read-clear            读取并清除所有消息
 -F, --file <文件>           用 文件 代替内核日志缓冲区
 -H, --human                 易读格式输出
 -k, --kernel                显示内核消息
 -l, --level <列表>          限制输出级别
 -T, --ctime                 显示易读的时间戳(可能不准确！)
支持的日志级别(优先级)：
   emerg - 系统无法使用
   alert - 操作必须立即执行
    crit - 紧急条件
     err - 错误条件
    warn - 警告条件
  notice - 正常但重要的条件
    info - 信息
   debug - 调试级别的消息

```

### 修改日志等级

+ 方法一

  通过make menuconfig 图形化配置界面修改 默认日志等级。配置路径为：Kernel hacking -> printk and dmesg options -> Default message log level()

+ 方法二

  在内核中调用printk的时候设置消息等级，如： printk(KERN_EMERG "hello\n");

+ 方法三

  修改文件配置打印等级，如： echo " 7 4 1 7 " > /proc/sys/kernel/printk

### 其他打印函数

```c
/* 打印内核调用堆栈 和 函数调用关系 */
void dump_stack(void);
/* 如果condition为真，打印函数调用关系 */
WARN(condition, const char *fmt, ...);
/* 如果condition为真，打印函数调用关系 */
WARN_ON(condition);
/* 触发内核oops，输出打印 */
BUG();
/* 如果condition为真，触发内核oops，输出打印 */
BUG_ON(condition);
/* 系统死机并输出打印 */
panic(const char *fmt, ...)
```

# 字符设备进阶

## 阻塞IO

```c
#include <linux/wait.h>

// 等待队列头
struct __wait_queue_head {
	spinlock_t		lock;	// 自旋锁
	struct list_head	task_list; // 链表头
};
typedef struct __wait_queue_head wait_queue_head_t;

#define WQ_FLAG_EXCLUSIVE	0x01
#define WQ_FLAG_WOKEN		0x02

// 等待队列项
struct __wait_queue {
	unsigned int		flags;
	void			*private;
	wait_queue_func_t	func;
	struct list_head	task_list;
};
typedef struct __wait_queue wait_queue_t;


// 定义并初始化 等待队列头
DECLARE_WAIT_QUEUE_HEAD(wq); // wq 类型为 wait_queue_head_t 类型

// 创建等待队列项 name为名称， tsk 表示属于哪个进程，一般为current（当前进程）
DECLARE_WAITQUEUE(name, tsk);

// 添加等待队列项 (可不调用)
void add_wait_queue(wait_queue_head_t *q, wait_queue_t *wait);
void add_wait_queue_exclusive(wait_queue_head_t *q, wait_queue_t *wait);

// 移除等待队列项
void remove_wait_queue(wait_queue_head_t *q, wait_queue_t *wait);


// 如果条件condition为假，那么线程会进入不可中断的休眠状态
wait_event(wq, condition); // wq 类型为 wait_queue_head_t 类型
// 如果条件condition为假，那么线程会进入可被中断的休眠状态
wait_event_interruptible(wq, condition); // wq 类型为 wait_queue_head_t 类型

// 唤醒 等待队列头中 的所有休眠线程
wake_up(wait_queue_head_t *wq);
// 唤醒 等待队列头中 的可中断的休眠线程
wait_up_interruptible(wait_queue_head_t *wq);
```

等待队列是内核实现阻塞和唤醒的内核机制，等待队列以循环链表为基础结构。

驱动程序编写：

1. 初始化等待队列头，并将条件（指示是否可读的变量）设置为假。
2. 在驱动函数 read中，如果无数据则调用wait_event，线程进入休眠。
3. 在调用write产生可读数据时，要把条件设置成真，然后调用wake_up唤醒等待队列中的线程。

## 非阻塞IO

```c
#include <linux/file.h>

struct file {
	struct path		f_path; // 文件路径
	const struct file_operations	*f_op; // 文件操作指针
	unsigned int 		f_flags; // 文件标志位，是否阻塞等
    ...

} __attribute__((aligned(4)));	/* lest something weird decides that 2 is OK */
```

驱动程序编写：

1. 在驱动函数 read write 中，判断 struct file 结构体成员中 flag 是否设置了非阻塞。
2. 如果设置了非阻塞，则可以在适当的时候返回 -EAGAIN;

## IO多路复用

```c
#include <linux/poll.h>
// 应用层调用 select 或者 poll 会调用驱动函数 poll
unsigned int (*poll) (struct file *, struct poll_table_struct *);

// 针对文件filp,把等待队列 wait_address 添加到 p 中
void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
```

驱动程序编写：

1. 驱动函数 poll 中，对可能引起设备文件状态变化的等待队列调用 poll_wait（是个非阻塞函数）。

   poll_wait 的作用是 关系设备变化的等待队列头 添加到 poll_table(回调函数poll的第二个参数)。

2. 驱动函数 poll 完成上述事情之后，返回表示状态的掩码(如 POLLIN 等)。

## 信号驱动IO

信号驱动IO的方式不需要应用程序去观察IO的状态，而是通过SIGIO信号来通知应用程序。

```c
#include <linux/fs.h>

// struct fasync_struct 内核结构体
struct fasync_struct {
	spinlock_t fa_lock;
	int magic;
	int fa_fd;
	struct fasync_struct *fa_next; // 单向链表
	struct file *fa_file;
	struct rcu_head	fa_rcu;
};

/*
 * 常在字符设备驱动 文件操作回调函数fasync 中调用
 * 前三个参数可以使用 fasync 的入参，最后一个参数存储在驱动中，后续传递给函数 kill_fasync
 * 该函数 返回负值表示错误，返回0表示没有变化，返回正数表示添加/删除项目
 */
int fasync_helper(int fd, struct file * filp, int on, struct fasync_struct **fapp);
/*
 * 当 *fp 非空时，该函数从内核中发送信号给进程
 * sig 为信号编号如 SIGIO
 * band 可读的时候设置POLLIN 可写的时候设置POLLOUT
 */
void kill_fasync(struct fasync_struct **fp, int sig, int band);
```



应用程序编写：

1. 使用 signal 注册信号处理函数。
2. 设置能够接受这个信号的进程， 使用 fcntl函数 的 F_SETOWN 命令。
3. 开启信号驱动IO，可以使用 fcntl函数 的F_SETFL命令设置FASYNC标志（会调用驱动fasync函数）。

驱动程序编写：

1. 注册 fasync 回调函数，并在函数内调用 fasync_helper 来操作 fasync_struct 结构体
2. 在合适的时机（如写入数据）时，调用 kill_fasync 发送信号给进程

## 异步IO

可以使用用户空间的glibc库实现，不依赖于内核。



## linux 内核定时器

### 内核节拍数

内核节拍数可以通过图形化界面来配置，频率一般是100-1000HZ，linux会使用变量jiffies 或者 jiffies_64 记录系统上电运行的节拍数。注意，当节拍为1000时，32位的 jiffies 大约50天就发生一次绕回。

### 定时器使用

内核定时器精度不高，不能作为高精度定时器使用。内核定时器不是周期运行的，超时后会关闭，如果需要周期运行，需要在定时处理中重新打开定时器。

Linux内核使用 timer_list 结构体表示内核定时器， timer_list 定义在文件 include/linux/timer.h 中

```c
struct timer_list {
     struct list_head entry;
     unsigned long expires; 	/* 定时器超时时间，单位是节拍数 */
     struct tvec_base *base;
     void (*function)(unsigned long); 	/* 定时处理函数 */
     unsigned long data; 	/* 要传递给 function 函数的参数 */
     int slack;
};

/* API函数 */

// 定义之后一定要使用 init 函数初始化，初始化之后才设置结构体成员
void init_timer(struct timer_list *timer);

// 设置完毕之后，向内核注册定时器并激活，注册之后的定时器才会运行
void add_timer(struct timer_list *timer);

// 修改定时值，如果定时器还没有激活的话，mod_timer 函数会激活定时器
int mod_timer(struct timer_list *timer, unsigned long expires);

// 立刻删除定时器，调用者需要等待其他处理器的定时处理器函数退出
// 返回值：0，定时器还没被激活；1，定时器已经激活。
int del_timer(struct timer_list * timer);

// 会等待其他处理器使用完定时器后 删除定时器。不能在中断上下文中使用。
// 返回值：0，定时器还没被激活；1，定时器已经激活。
int del_timer_sync(struct timer_list *timer);

/* jiffies 转换函数 */
int jiffies_to_msecs(const unsigned long j);
int jiffies_to_usecs(const unsigned long j);
u64 jiffies_to_nsecs(const unsigned long j);

long msecs_to_jiffies(const unsigned int m);
long usecs_to_jiffies(const unsigned int u);
unsigned long nsecs_to_jiffies(u64 n);
```

## lseek 操作

文件的读写都是基于文件当前偏移位置来操作的，APP通过调用 lseek 函数，可以修改读写位置。

 ```c
 #include <sys/types.h>
 #include <unistd.h>
 /*
  * 对于文件描述符filedes，设置其当前文件偏移，并返回新的偏移值。
  * 新的位置为基于whence位置偏移offset处，whence 可使用宏：
  * SEEK_SET：文件开头处
  * SEEK_CUR：文件当前偏移处
  * SEEK_END：文件末尾处
  */
 off_t lseek(int filedes, off_t offset, int whence);
 ```

调用 lseek 之后，函数 read write 的操作位置会发生改变，并且 read write 本身也会把 当前文件偏移 往后移。即在驱动中，read write 第四个参数 loff_t * 是个出入参。

在驱动中，对应的file_operations 回调函数为 llseek。

```c
#include <linux/fs.h>

/*
 * 对于文件结构体 filp，设置其当前文件偏移(filp->f_pos)，并返回新的偏移值。
 * 参数 offset 和 whence 透传自 应用层函数 lseek
 * 文件的当前偏移值 存储于 filp->f_pos
 * 此函数需要 设置 filp->f_pos 为新的偏移值，并且把新的偏移值返回
 */
loff_t llseek(struct file *filp, loff_t offset, int whence);
```

## ioctl 操作

对于设备文件的操作，比如LED等，除了使用 read write 控制之外，还可以使用 ioctl 控制。一般情况下 read write 是用来读写信息流的。

ioctl 的命令是驱动工程师和应用开发者约定的，linux没有预定义具体的命令，只规定了命令格式。

request 命令为32bits，分为四个部分：

1. 设备类型(8bits)，代表一类设备
2. 序列号(8bits)，同一设备类型中序列号不重复
3. 数据读写方向(2bits)，如只读(10)只写(01)读写(11)无数据(00)
4. 传递数据尺寸类型(14bits)，

```c
#include <sys/ioctl.h>
int ioctl(int fd, unsigned long request, ...);
/* ioctl 命令合成宏 */
_IO(type, nr);			// 合成没有数据传递的命令
_IOR(type, nr, size);	// 合成从驱动中读数据的命令
_IOW(type, nr, size);	// 合成往驱动中写数据的命令
_IOWR(type, nr, size); 	// 合成先写后读的命令

/*
 * type 为 8bits，可使用单字符如 'A'
 * nr 为序列号
 * size 为C语言类型关键字，如 int, 宏内部会调用sizeof()
 */

/* ioctl 命令分解宏 */
_IOC_DIR(nr);			// 获取读写方向字段
_IOC_TYPE(nr);			// 获设备类型字段
_IOC_NR(nr);			// 获取
_IOC_SIZE(nr);			// 获取数据尺寸类型字段
```

对应驱动代码 file_operations 中的 unlocked_ioctl

```c
// unlocked_ioctl 回调函数原型
long unlocked_ioctl(
    struct file *filp,
    unsigned int cmd,
    unsigned long arg
);
```



# 中断框架 

## 基础知识

硬件中断与linux中断框架：

![image-20230619012516471](.\linux 驱动.assets\image-20230619012516471.png)

### GIC-400

| 中断源分类         | 中断号    | 描述                                            |
| ------------------ | --------- | ----------------------------------------------- |
| SGI 软件触发中断   | 0 - 15    | 用于core之间相互通信                            |
| PPI 私有外设中断   | 16 - 31   | 每个core私有的中断，如调度使用的tick中断        |
| SPI 共享外设中断   | 32 - 1020 | 由外设触发，如按键、串口等中断，可分配给不同CPU |
| LPI 基于消息的中断 |           | 不支持 GIC-V1 GIC-V2                            |

每个CPU仅有四个中断接口：FIQ、IRQ、virtual FIQ、virtual IRQ

GIC中断控制器主要由两部分组成，CPU模块接口和仲裁单元

GIC中断处理过程：

1. GIC检测到一个中中断发生，将该中断标记为pending状态
2. 对于pending状态的中断，GIC仲裁单元会确定处理该中断的目标CPU
3. 对于每一个CPU，仲裁单元会从pending状态中断中选择最高优先级的中断，发送到目标CPU的CPU模块接口上
4. CPU模块接口会决定这个中断是否可以发送给CPU
5. 当一个CPU进入中断之后，会读取GIC_IAR寄存器来响应此中断
6. 当CPU完成中断，必须发送一个EOI信号给GIC控制器



Linux 系统中有硬件中断，也有软件中断。对硬件中断的处理有 2 个原则：

1. 不能嵌套
2. 越快越好

## 中断相关驱动API

```c
#include <linux/interrupt.h>
#include <linux/of_irq.h>

// 中断标志位枚举
enum {
	IRQ_TYPE_NONE		= 0x00000000,
	IRQ_TYPE_EDGE_RISING	= 0x00000001,
	IRQ_TYPE_EDGE_FALLING	= 0x00000002,
	IRQ_TYPE_EDGE_BOTH	= (IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING),
	IRQ_TYPE_LEVEL_HIGH	= 0x00000004,
	IRQ_TYPE_LEVEL_LOW	= 0x00000008,
	IRQ_TYPE_LEVEL_MASK	= (IRQ_TYPE_LEVEL_LOW | IRQ_TYPE_LEVEL_HIGH),
	IRQ_TYPE_SENSE_MASK	= 0x0000000f,
	IRQ_TYPE_DEFAULT	= IRQ_TYPE_SENSE_MASK,

	IRQ_TYPE_PROBE		= 0x00000010,

	IRQ_LEVEL		= (1 <<  8),
	IRQ_PER_CPU		= (1 <<  9),
	IRQ_NOPROBE		= (1 << 10),
	IRQ_NOREQUEST		= (1 << 11),
	IRQ_NOAUTOEN		= (1 << 12),
	IRQ_NO_BALANCING	= (1 << 13),
	IRQ_MOVE_PCNTXT		= (1 << 14),
	IRQ_NESTED_THREAD	= (1 << 15),
	IRQ_NOTHREAD		= (1 << 16),
	IRQ_PER_CPU_DEVID	= (1 << 17),
	IRQ_IS_POLLED		= (1 << 18),
};

// 中断返回值 枚举
enum irqreturn {
	IRQ_NONE		= (0 << 0),		// 中断与本设备无关
	IRQ_HANDLED		= (1 << 0),		// 本设备的中断处理了
	IRQ_WAKE_THREAD		= (1 << 1),	// 请求唤醒线程 处理线程化的下半部分
};
typedef enum irqreturn irqreturn_t;

// 中断处理函数原型（中断上半部分，中断线程化的下半部分）
typedef irqreturn_t (*irq_handler_t)(int irq, void *dev);

// 根据设备树节点获取 中断号
unsigned int irq_of_parse_and_map(
    struct device_node *node, 	// 设备树节点
    int index					// 获取第几个中断号，index从0开始计数
);

// 根据原理图确定引脚名，再查阅SOC手册可计算 gpio号，或者使用gpio子系统确定gpio号
// 输入 gpio号 ，返回软件中断号
int gpio_to_irq(unsigned int gpio);
// 输入 gpiod ，返回软件中断号
int gpiod_to_irq(const struct gpio_desc *desc);

// 设备向内核注册中断处理函数 API，由于可能会导致睡眠，不可以在中断上下文使用
int request_irq(
    unsigned int irq,  		// 软件中断号
    irq_handler_t handler, 	// 注册的中断处理函数
    unsigned long flags, 	// 中断标志，如 共享中断、电平触发、边缘触发等
    const char *name, 		// 自定义的设备中断名字，可在 /proc/irq 中看到
    void *dev 				// 传给中断处理函数的 参数
    						// 如果flags设置了共享中断，则该参数不可为NULL，用来区分设备
    						// 如果flags不为共享中断，该参数可以为 NULL
);

// 释放中断资源 API,删除某个中断处理函数
void free_irq(
    unsigned int, 		// 软件中断号
    void *				// 必须与request的时候一致！
);

```

## 中断内核源码

```c
/**
 * struct irq_data - per irq and irq chip data passed down to chip functions
 * @mask:		precomputed bitmask for accessing the chip registers
 * @irq:		interrupt number
 * @hwirq:		hardware interrupt number, local to the interrupt domain
 * @node:		node index useful for balancing
 * @state_use_accessors: status information for irq chip functions.
 *			Use accessor functions to deal with it
 * @chip:		low level interrupt hardware access
 * @domain:		Interrupt translation domain; responsible for mapping
 *			between hwirq number and linux irq number.
 * @parent_data:	pointer to parent struct irq_data to support hierarchy
 *			irq_domain
 * @handler_data:	per-IRQ data for the irq_chip methods
 * @chip_data:		platform-specific per-chip private data for the chip
 *			methods, to allow shared chip implementations
 * @msi_desc:		MSI descriptor
 * @affinity:		IRQ affinity on SMP
 *
 * The fields here need to overlay the ones in irq_desc until we
 * cleaned up the direct references and switched everything over to
 * irq_data.
 */
// 
struct irq_data {
	u32			mask;
	unsigned int		irq;				// 软件中断号
	unsigned long		hwirq;				// 硬件中断号
	unsigned int		node;
	unsigned int		state_use_accessors;
	struct irq_chip		*chip;				// 中断屏蔽、使能、禁止 等操作
	struct irq_domain	*domain;			// 用于找到下一级中断触发源的软件中断号，含下一级的硬件中断与软件中断的映射等数据
#ifdef	CONFIG_IRQ_DOMAIN_HIERARCHY
	struct irq_data		*parent_data;
#endif
	void			*handler_data;
	void			*chip_data;
	struct msi_desc		*msi_desc;
	cpumask_var_t		affinity;
};

// 用于描述中断中 设备的处理函数
struct irqaction {
	irq_handler_t		handler;	// 存放注册的 中断服务函数
	void				*dev_id; 	// 存放注册的 指针参数，另外共享中断卸载时，找到需要卸载的设备
	void __percpu		*percpu_dev_id;
	struct irqaction	*next;		// 链表，下一个设备的中断处理函数
	irq_handler_t		thread_fn;	// 中断线程化 下半部的服务函数
	struct task_struct	*thread;
	unsigned int		irq;		// 软件中断号
	unsigned int		flags;		// 存放注册的 flags，含 是否共享、触发方式等
	unsigned long		thread_flags;
	unsigned long		thread_mask;
	const char		*name;			// 存放注册的 name指针
	struct proc_dir_entry	*dir;
} ____cacheline_internodealigned_in_smp;

// 中断描述，用于描述每一个硬件中断，与软中断号一一对应
struct irq_desc {
	struct irq_data		irq_data;
	unsigned int __percpu	*kstat_irqs;
	irq_flow_handler_t	handle_irq;	// BSP工程师编写的处理函数，可以根据寄存器找出下一级的中断触发源，并执行下一级的 handle_irq
	struct irqaction	*action;	// IRQ action 链表，每项代表设备驱动注册的处理函数，如只有多个成员代表为本中断为共享中断
	unsigned int		status_use_accessors;
	unsigned int		core_internal_state__do_not_mess_with_it;
	unsigned int		depth;		/* nested irq disables */
	unsigned int		wake_depth;	/* nested wake enables */
	unsigned int		irq_count;	/* For detecting broken IRQs */
	unsigned long		last_unhandled;	/* Aging timer for unhandled count */
	unsigned int		irqs_unhandled;
	atomic_t		threads_handled;
	int			threads_handled_last;
	raw_spinlock_t		lock;
	struct cpumask		*percpu_enabled;
	int			parent_irq;
	struct module		*owner;
	const char		*name;
} ____cacheline_internodealigned_in_smp;

```

每个外设的外部中断，都是用 struct irq_desc 结构体描述的，所以也叫做中断描述符。 

linux管理中断方法可分为动态和静态。

如果定义了 CONFIG_SPACE_IRQ则使用静态方法，在ARM64上这个宏是默认打开的。

这个宏 在 .config 中定义的，所以可以使用 make menuconfig 图形化界面配置。

1. 静态方法

   使用静态全局变量数组 struct irq_desc irq_desc[NR_IRQS];

   其下标为硬件中断号，可通过 中断号 获取 中断描述符结构体。

   中断描述符 含有成员 action， 里面的中断服务函数（中断上半部分）。

2. 动态方法

   使用 基数树 radix tree



中断的管理是级联的，比如 GPIO的引脚触发的中断，会传递给GPIO控制器，进而传递给GIC，最后再传递给CPU。

在中断框架处理时，会先调用GIC对应**irq_desc结构体**的 **handle_irq函数**，找到GPIO控制器的软中断号，调用GPIO控制器的 **handle_irq函数**，以此类推。

当最后找到真正触发中断的软件中断后，会遍历该中断对应的 **struct irqaction 结构体**，并执行内部注册的**中断处理函数(irq_handler_t)**。



对于共享中断，**struct irq_desc 结构体**的 action 链表中有多个 **struct irqaction 结构体**。在共享中断发生时，每个链表成员相应的 **中断处理函数(irq_handler_t)**都会被框架执行。所以如果某个中断处理函数想忽略这个中断，可以在处理函数中直接返回 IRQ_NONE。

注册的 **中断处理函数(irq_handler_t)** 仅处理紧急事件，如果其他有较多的任务，可创建中断下半部分来进行处理。此时的 中断处理函数(irq_handler_t) 又叫**中断上半部分**。

## 设备树中的中断

中断有级联结构，该结构由设备树来指定。

+ 中断控制器节点：

arch\arm\boot\dts\imx6ull.dtsi

```json

intc: interrupt-controller@00a01000 {
    compatible = "arm,cortex-a7-gic";	// 通过中断控制器，GIC
    #interrupt-cells = <3>;				// 下级使用者需要用几个整数表示中断
    interrupt-controller;				// 表明本节点是中断控制器节点
    reg = <0x00a01000 0x1000>,
          <0x00a02000 0x100>;
};

soc {
    #address-cells = <1>;
    #size-cells = <1>;
    compatible = "simple-bus";
    interrupt-parent = <&gpc>; 		// 指定中断上级节点，该属性值会传递给子孙节点，直到被覆盖为止
    ranges;
}

gpio1: gpio@0209c000 {
    // 本节点没有指定 interrupt-parent属性，继承上级节点（gpio1->aips1->soc）的值
    compatible = "fsl,imx6ul-gpio", "fsl,imx35-gpio";
    reg = <0x0209c000 0x4000>;
    interrupts = <GIC_SPI 66 IRQ_TYPE_LEVEL_HIGH>,
             <GIC_SPI 67 IRQ_TYPE_LEVEL_HIGH>;
    gpio-controller;
    #gpio-cells = <2>;
    interrupt-controller;				// 本节点是 中断控制器
    #interrupt-cells = <2>;				// 使用本节点的中断，需要使用两个整数表示
};
```



+ 使用中断节点：

arch\arm\boot\dts\imx6ull-alientek-emmc.dts

```json
   edt-ft5x06@38 {
		compatible = "edt,edt-ft5306", "edt,edt-ft5x06";
		pinctrl-names = "default";
		pinctrl-0 = <&ts_int_pin &ts_reset_pin>;

		reg = <0x38>;
		interrupt-parent = <&gpio1>;		// 提供中断的父节点、中断控制器节点
		interrupts = <9 0>;					// 使用9号中断（硬件中断号），gpio1 规定了使用两个整数
		reset-gpios = <&gpio5 9 GPIO_ACTIVE_LOW>;
		irq-gpios = <&gpio1 9 GPIO_ACTIVE_LOW>;
		status = "okay";
	};
```



## 中断下半部分机制

中断处理中，比较耗时的操作放在中断下半部分执行，中断下半部分有  tasklet、软中断、工作队列、中断线程化等方式。

###  tasklet

内核中一般使用tasklet实现简短的中断下半部分。tasklet 本质上是 TASKLET_SOFTIRQ 软中断 的中断处理函数中运行的任务。每个cpu都有自己维护tasklet任务链表。

与一般软中断不同：一个 tasklet 同一时间只能在一个cpu上运行。

与一般BH不同：不同的 tasklet 同一时间可能运行在不同的cpu上。



```c
#include <linux/interrupt.h>

// tasklet 对象结构体
struct tasklet_struct {
	struct tasklet_struct *next; 	// 链表下一个节点
	unsigned long state;·			// 调度状态
	atomic_t count;					// 0:使能 非0:未使能
	void (*func)(unsigned long);	// 绑定中断下文的函数
	unsigned long data;				// func 的参数
};

// 使用宏来 定义 并初始化 tasklet对象
#define DECLARE_TASKLET(name, func, data) \
struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(0), func, data }

#define DECLARE_TASKLET_DISABLED(name, func, data) \
struct tasklet_struct name = { NULL, 0, ATOMIC_INIT(1), func, data }

// 动态初始化tasklet函数，初始化为使能状态（count=0）
void tasklet_init(
    struct tasklet_struct *t,		// 被初始化的结构体
    void (*func)(unsigned long),	// 下半部回调函数
    unsigned long data				// 传给 func 的参数
);

// tasklet关闭函数
// 内部实现：本质上是把成员 count 值+1
void tasklet_disable(struct tasklet_struct *t);

// tasklet使能函数
// 内部实现：本质上是把成员 count 值-1
void tasklet_enable(struct tasklet_struct *t);

// 把某个tasklet加入调度，如果他被调度选中时为使能状态，则会执行相应函数
// 内部实现：把tasklet加入到每个cpu维护的tasklet链表中,再触发TASKLET_SOFTIRQ软中断
void tasklet_schedule(struct tasklet_struct *t);

// 把某个已经使能、已经调度的 tasklet 移出调度，如果目标正在执行会等待其执行完成
void tasklet_kill(struct tasklet_struct *t);
```

+ 初始化步骤：
  + 取得中断号 并 注册中断上半部分 **request_irq**()
    + 在中断上半部分中，把 tasklet 加入调度 **tasklet_schedule**()
  + 可动态初始化 tasklet，默认为使能状态。 **tasklet_init**()
+ 反初始化步骤：
  + 把 tasklet 移出调度 **tasklet_kill**()
  + 释放中断 **free_irq**()


注意：

1. tasklet_kill 不能对已经加入调度但**未使能**的tasklet使用。
2. 未加入调度的tasklet不能被执行，加入之后能否执行取决于count值。
3. tasklet 绑定的函数**不可以使用任何引起休眠的函数**，否则会引起内核异常。

### 软中断

软中断也是实现中断下半部分的方法之一，但是软中断资源有限，中断号不多，一般用于网络设备驱动、块设备驱动当中。中断号越小，优先级约高。

内核开发者希望驱动工程师使用tasklet而不是软中断，所以 open_softirq()、 raise_softirq() 正常情况下不导出到符号表。而且添加枚举值意味着需要重新编译内核。

```c
#include <linux/interrupt.h>
/*
 * 请尽量不要新增软中断号。
 * 除非真的需要高频的线程调度，否则使用tasklet就能胜任绝大多数任务。
 */
// 内核定义软中断中断号，如果自己定义可在 NR_SOFTIRQS 之上添加自己的枚举值
enum
{
	HI_SOFTIRQ=0,
	TIMER_SOFTIRQ,
	NET_TX_SOFTIRQ,
	NET_RX_SOFTIRQ,
	BLOCK_SOFTIRQ,
	BLOCK_IOPOLL_SOFTIRQ,
	TASKLET_SOFTIRQ,
	SCHED_SOFTIRQ,
	HRTIMER_SOFTIRQ,
	RCU_SOFTIRQ, /* RCU应该保持为最后一个软中断 */
	NR_SOFTIRQS
};

/* 软中断 API */
// 软中断能力初始化，不需要在驱动代码中调用
void softirq_init(void);

// 创建一个软中断（该函数要使用宏 EXPORT_SYMBOL 导出符号）
// nr 为软中断号，action为对应处理函数
void open_softirq(int nr, void (*action)(struct softirq_action *));

// 触发一个软中断（该函数要使用宏 EXPORT_SYMBOL 导出符号）
// nr 为需要触发的软中断号
void raise_softirq(unsigned int nr);

// 关闭一个软中断
void raise_softirq_irqoff(unsigned int nr);
```



对于软中断来说，中断上下半部具有流程图如下:

![image-20230723122013462](./linux 驱动.assets/image-20230723122013462.png)

preempt_count 的作用是，在 多次中断发生时，中断下半部分不会嵌套执行。

中断下半部有如下性质：

1. 会执行各类软中断任务（其中含各种 tasklet），即多个中断共享同一个下半部分。
2. 中断下半部代码执行时 preempt_count 肯定为1
3. 中断下半部执行期间，可能突然被中断，执行对应的上半部（此时preempt_count 肯定大于1）



### 工作队列

工作队列是 实现中断下半部分的机制之一，与 tasklet 的区别是工作队列 处理过程中**可以进行休眠**，可以执行比tasklet 更耗时的工作。

工作队列分为共享工作队列和自定义工作队列，优先使用共享工作队列，如果共享工作队列不满足要求，可以使用自定义工作队列。

#### 共享工作队列

```c
#include <linux/workqueue.h>
// 工作队列回调原型
typedef void (*work_func_t)(struct work_struct *work);

// 工作队列中的工作
struct work_struct {
	atomic_long_t data;
	struct list_head entry;	// 
	work_func_t func;		// 回调函数，具体的工作
};


// 定义并初始化工作对象的宏
// n 为工作变量名， f 为工作函数
DECLARE_WORK(n, f);

// 初始化工作对象的宏
// _work为工作对象的指针，_func为工作函数
INIT_WORK(_work, _func);

// 把 work所含的工作 添加到 共享工作队列 中进行调度
bool schedule_work(struct work_struct *work);

// 取消一个已经调度的工作，如果正在执行，会等待执行完成
bool cancel_work_sync(struct work_struct *work);
```

共享工作队列使用：

+ 初始化步骤：
  + 创建并初始化 工作 **DECLARE_WORK()** 或者 **INIT_WORK()**
  + 取得中断号 并 注册中断服务函数 **request_irq**()
    + 在中断服务函数中，把 工作 加入工作队列调度 **schedule_work()**
+ 反初始化步骤：
  + 把 工作 移出调度 **cancel_work_sync()**
  + 释放中断 **free_irq**()



#### 自定义工作队列

```c
#include <linux/workqueue.h>

// 创建工作队列，作用于每个CPU，返回工作队列指针，失败返回NULL
struct workqueue_struct *create_workqueue(const char *name);

// 创建工作队列，作用于单个CPU，返回工作队列指针，失败返回NULL
struct workqueue_struct *create_singlethread_workqueue(const char *name);

// 把一个工作添加到 特定的工作队列 中进行调度
bool queue_work(struct workqueue_struct *wq, struct work_struct *work);

// 把一个工作添加到 特定的工作队列 中进行调度，并指定CPU
bool queue_work_on(int cpu, struct workqueue_struct *wq, struct work_struct *work);

// 刷新工作队列,告诉内核尽快处理
void flush_workqueue(struct workqueue_struct *wq);

// 删除自定义工作队列
void destroy_workqueue(struct workqueue_struct *wq);
```

自定义工作队列使用：

+ 初始化步骤：
  + 创建自定义工作队列 **create_workqueue()**
  + 创建并初始化 工作 **DECLARE_WORK()** 或者 **INIT_WORK()**
  + 取得中断号 并 注册中断服务函数 **request_irq**()
    + 在中断服务函数中，把 工作 加入工作队列调度 **schedule_work()**
+ 反初始化步骤：
  + 把 工作 移出调度 **cancel_work_sync()**
  + 刷新工作队列 **flush_workqueue()**
  + 删除工作队列 **destroy_workqueue()**
  + 释放中断 **free_irq**()

使用命令  ps -aux 查看，带 kworker 线程是内核线程，它们要去“工作队列”(work queue)上取出一个一个“工作”(work)，来执行它里面的函数。

#### 延时工作

```c
#include <linux/workqueue.h>
// 延时工作，封装了内核定时器 和 工作对象
struct delayed_work {
	struct work_struct work;	// 工作对象
	struct timer_list timer; 	// 内核定时器

	/* target workqueue and CPU ->timer uses to queue ->work */
	struct workqueue_struct *wq; 
	int cpu;
};

// 定义并初始化延时工作对象
// n 为对象标识符， f 为工作回调函数
DECLARE_DELAYED_WORK(n, f);

// 初始化延时工作对象
// _work 为工作对象指针， _func 为工作回调函数
INIT_DELAYED_WORK(_work, _func);		 // 初始化全局变量
INIT_DELAYED_WORK_ONSTACK(_work, _func); // 初始化栈上变量

// 延时后把 dwork 加入共享工作队列调度
bool schedule_delayed_work(
    struct delayed_work *dwork,
    unsigned long delay
);

// 延时后把 dwork 加入共享工作队列调度，绑定CPU
bool schedule_delayed_work_on(
    int cpu,
    struct delayed_work *dwork,
    unsigned long delay
);

// 延时后把 工作work 加入工作队列wq调度，绑定CPU
// 如果work已经在队列内则返回false，否则返回true
bool queue_delayed_work_on(
    int cpu,						// 目标CPU
    struct workqueue_struct *wq,	// 加入的工作队列
    struct delayed_work *work,		// 工作
    unsigned long delay				// 延迟时间 内核节拍数
);
```

#### 工作队列传参

```c
#include <linux/kernel.h>
#include <linux/workqueue.h>
/*
 * 根据结构体成员地址获取结构体首地址
 * ptr 为结构体成员地址
 * type 为结构体类型
 * member 为结构体成员名
 * 返回 type 类型的结构体地址
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})


typedef void (*work_func_t)(struct work_struct *work);
// 驱动中 自定义的结构体
struct work_data {
    struct work_struct work; // 工作对象
    /* 以下的成员都可作为参数 */
    int arg1;
    char arg2[10];
    ...
}
// 在工作中获取参数 示例函数
void test_work(struct work_struct *work)
{
    struct work_data *pdata;
    pdata = container_of(work, struct work_data, work);
    // 根据 struct work_data结构体地址 获取参数
    pdata->arg1;
    pdata->arg2;
    ...
}
```

回调函数 work_func_t 的形参work 其实是驱动中定义的 工作对象地址，于是可以通过使用 container_of 函数来帮助传参。

参数传递方式：

1. 把 工作结构体(struct work_struct) 定义在另一个结构体(struct work_data)内部
2. 初始化结构体(struct work_data)内参数成员，以及工作结构体
3. 在工作回调函数内部，使用 container_of(work, work_data, work) 获取结构体地址。



#### 并发管理工作队列

```c
#include <linux/workqueue.h>

/*
 * 工作队列 的 标志位 及其他参数 内容定义
 * 更多详细信息见： Documentation/workqueue.txt.
 */
enum {
	WQ_UNBOUND		= 1 << 1, /* 工作队列由 不与CPU绑定的线程 处理 */
	WQ_FREEZABLE		= 1 << 2, /* 与电源管理相关 */
	WQ_MEM_RECLAIM		= 1 << 3, /* 可防止内存不够导致线程创建失败 */
	WQ_HIGHPRI		= 1 << 4, /* 工作队列由高优先级线程处理 */
	WQ_CPU_INTENSIVE	= 1 << 5, /* 计算密集型任务可使用此标志 */
	WQ_SYSFS		= 1 << 6, /* sysfs 可见,见 wq_sysfs_register */

	/*
	 * Per-cpu工作队列通常是首选，因为由于缓存局部性，它们往往表现出更好的性能。Per-cpu工作队列排除了调度器选择执行工作线程的CPU，这有一个增加功耗的副作用。
	 * 调度程序认为CPU空闲，如果它没有任何任务要执行，并试图保持空闲的核心空闲，以节省电力;但是，例如，从空闲CPU上的中断处理程序调度的每个CPU的工作项将迫使调度器在打破空闲的CPU上执行该工作项，这反过来可能导致更多的调度选择，这将不利于降低功耗。
	 * 标记为WQ_POWER_EFFICIENT的工作队列默认是按cpu计算的，但如果工作队列被取消绑定。指定Power_efficient内核参数。Per-cpu工作队列被确定为对功耗有重大贡献的工作队列被识别并标记为此标志，启用power_efficient模式可以显著节省功耗，但代价是性能上的小损耗。
	 */
	WQ_POWER_EFFICIENT	= 1 << 7,

	__WQ_DRAINING		= 1 << 16, /* internal: workqueue is draining */
	__WQ_ORDERED		= 1 << 17, /* internal: workqueue is ordered */

	WQ_MAX_ACTIVE		= 512,	  /* 线程池最大线程数量限制 */
	WQ_MAX_UNBOUND_PER_CPU	= 4,	  /* 不绑定的线程数量限制 */
	WQ_DFL_ACTIVE		= WQ_MAX_ACTIVE / 2, /* 默认线程池数量 */
};

// 创建工作队列 宏定义，返回分配的指针
struct workqueue_struct *alloc_workqueue(
    const char *fmt,		// 工作队列的名称
    unsigned int flags,		// 属性标志位
    int max_active,			// 线程池最大线程数量
    ...
);
```

根据工作队列的flag参数，其任务可以分配给 CPU上运行的不同线程来执行：

+ CPU内 高优先级线程
+ CPU内 正常优先级线程
+ 不与 CPU 绑定的线程

### 中断线程化

中断线程化也是实现中断上半部分和下半部分的方式之一。

下半部分的任务会交给一个中断线程来处理，这个内核线程只服务于这个中断。

当发生中断的时候,内核会唤醒对应内核线程，然后由这个内核线程来执行下半部分的函数。

```c
/*
 * 这个调用 分配中断资源 并 启用中断线和中断处理函数。从这个调用开始，您的中断处理函数就可以被调用了。因为你的中断处理函数必须能够清除任何 板子触发的中断，你必须注意 初始化你的硬件 和 以正确的顺序设置 中断处理函数。
 * 如果你想为你的设备配置一个线程来处理中断，那么你需要提供 @handler 和 @thread_fn 参数。回调函数 @handler 在硬中断上下文中被调用，它必须检查中断是否来自设备。如果是，它需要在设备上禁用中断并返回 IRQ_WAKE_THREAD，这将唤醒处理程序线程并运行@thread_fn。这种拆分设计对于支持共享中断是必要的。
 * @handler 和 @thread_fn 不可同时为NULL
 * 参数 @dev_id 必须全局唯一，通常使用设备数据结构的地址。因为回调函数会接收到这个值，所以使用它是有意义的。如果你的中断是共享的，你必须传递一个非空dev_id，因为在释放中断时需要它来做区分。
 * 成功返回0，失败返回非0。
 */
int request_threaded_irq(
    unsigned int irq, 			// 软件中断号
    irq_handler_t handler,		// 中断上半部处理回调，可为NULL
    irq_handler_t thread_fn,	// 中断下半部回调，给中断线程处理，可为NULL
    unsigned long irqflags,		// 标志位：是否共享、触发方式
    const char *devname,		// 设备名称
    void *dev_id				// 传给回调的参数，也用来区分共享中断的处理函数
);
```

申请中断API函数 request_irq 的实现其实是封装了 request_threaded_irq，在使用中断线程化的时候，需要直接调用 request_threaded_irq 而不是 request_irq。



### 各种机制比较

+ tasklet
  + 处理函数原型：void (*func)(unsigned long data);
  + 调度方式：每个CPU维护自己的tasklet链表。每一个tasklet可以加到所有CPU的链表中，但同一个tasklet不会被多个CPU同时执行。
  + 优点：占用资源少
  + 缺点：不可睡眠
+ 软中断
  + 处理函数原型：void (*action)(struct softirq_action *irq);
  + 调度方式：触发软中断后，任选一个CPU执行对应的中断处理函数。
  + 优点：高频性能
  + 缺点：不可睡眠
+ 工作队列
  + 处理函数原型：void (*work_func_t)(struct work_struct *work);
  + 调度方式：每个工作队列对应一个内核处理线程，负责调度执行队列内的多个任务。
  + 优点：可睡眠、可任意传参、可设置线程属性（是否绑定CPU、是否高优先级等）
  + 缺点：
+ 中断线程化
  + 处理函数原型：irqreturn_t (*irq_handler_t)(int irq, void *dev);
  + 调度方式：每个任务都有专门的线程处理，多个线程均匀使用多个CPU。
  + 优点：可睡眠、可任意传参、合理分配CPU负载
  + 缺点：占用资源多



# 平台总线模型

平台总线是linux虚拟出来的总线，与SPI、IIC等实物总线不一样。

其主要思想是把驱动分为两部分：

1. 与硬件资源相关的 device（设备）
2. 与驱动逻辑的 driver（驱动）

两者在加载时，平台总线会进行匹配，匹配成功后共同工作控制硬件。

平台总线的优势是可以大大提高驱动代码的复用率。

## platform_device

```c
#include <linux/platform_device.h>

struct platform_device {
	const char	*name; 	// 需要能与驱动匹配
	int			id; 	// 生成的文件会带.id后缀，如果为-1则没有后缀
	bool		id_auto;
	struct device	dev;
	u32		num_resources; // resource 结构体数组长度
	struct resource	*resource; // 相关资源信息，如 内存地址、中断号等等

	const struct platform_device_id	*id_entry;
	char *driver_override; /* 用于匹配驱动，优先级高 */

	/* MFD cell pointer */
	struct mfd_cell *mfd_cell;

	/* arch specific additions */
	struct pdev_archdata	archdata;
};

// 与硬件相关的资源，查阅手册得到
struct resource {
	resource_size_t start; // 数值，其含义由资源类型决定
	resource_size_t end; // 数值，其含义由资源类型决定
	const char *name;
	unsigned long flags; // 标识资源类型，如 地址、中断号等
	struct resource *parent, *sibling, *child;
};

// 注册device到平台总线，可在模块初始化时调用
int platform_device_register(struct platform_device *);
// 从平台总线注销device，可在模块退出时调用
void platform_device_unregister(struct platform_device *);
```





## platform_driver

```c
#include <linux/platform_device.h>

// 用于表示一个平台驱动
struct platform_driver {
	int (*probe)(struct platform_device *); // 与设备匹配之后会执行，必须要实现
	int (*remove)(struct platform_device *); // 移除设备时会执行
	void (*shutdown)(struct platform_device *); // 关闭设备时调用
	int (*suspend)(struct platform_device *, pm_message_t state); // 进入睡眠时调用
	int (*resume)(struct platform_device *); // 从睡眠模式恢复时调用
	struct device_driver driver;	// 里面也有名字，函数指针，匹配优先级比id_table低
	const struct platform_device_id *id_table; // 优先匹配id_table名字
	bool prevent_deferred_probe;
};

// 构成 id_table结构体数组 的结构体定义
struct platform_device_id {
	char name[PLATFORM_NAME_SIZE]; // 用于匹配的名字
	kernel_ulong_t driver_data;
};

// platform_driver 内部有个成员 struct device_driver driver;
// struct device_driver 的作用类似于 struct platform_driver 的基类
struct device_driver {
	const char		*name; // 驱动的名字，用于与设备名字匹配
	struct bus_type	*bus; // 标识本驱动的设备属于哪一种总线
	struct module	*owner; // 模块 拥有者
	const char		*mod_name;
	bool suppress_bind_attrs;
	const struct of_device_id	*of_match_table;
	const struct acpi_device_id	*acpi_match_table;
	int (*probe) (struct device *dev);
	int (*remove) (struct device *dev);
	void (*shutdown) (struct device *dev);
	int (*suspend) (struct device *dev, pm_message_t state);
	int (*resume) (struct device *dev);
	const struct attribute_group **groups;

	const struct dev_pm_ops *pm;

	struct driver_private *p;
};

// 注册driver到平台总线，可在模块初始化时调用，实际上是个 宏函数
int platform_driver_register(struct platform_driver *);
// 从平台总线注销driver，可在模块卸载时调用
void platform_driver_unregister(struct platform_driver *);

```

## platform_driver 注册过程

```c

static int platform_drv_probe(struct device *_dev)
{
	struct platform_driver *drv = to_platform_driver(_dev->driver);
	struct platform_device *dev = to_platform_device(_dev);
	int ret;

	ret = of_clk_set_defaults(_dev->of_node, false);
	if (ret < 0)
		return ret;

	ret = dev_pm_domain_attach(_dev, true);
	if (ret != -EPROBE_DEFER) {
		ret = drv->probe(dev); // 调用回调
		if (ret)
			dev_pm_domain_detach(_dev, true);
	}

	if (drv->prevent_deferred_probe && ret == -EPROBE_DEFER) {
		dev_warn(_dev, "probe deferral not supported\n");
		ret = -ENXIO;
	}

	return ret;
}

int __platform_driver_register(struct platform_driver *drv,
				struct module *owner)
{
	drv->driver.owner = owner;
	drv->driver.bus = &platform_bus_type; // 总线类型为平台总线
	if (drv->probe)
		drv->driver.probe = platform_drv_probe;
	if (drv->remove)
		drv->driver.remove = platform_drv_remove;
	if (drv->shutdown)
		drv->driver.shutdown = platform_drv_shutdown;

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL_GPL(__platform_driver_register); // 导出符号
```

如果 platform_driver 注册了 probe remove shutdown 中的某一个，那么platform_driver 在注册的时候，其成员 device_driver 中对应的回调也会被初始化成。

## driver与device的匹配

```c
/* drivers/base/platform.c */

static const struct dev_pm_ops platform_dev_pm_ops = {
	.runtime_suspend = pm_generic_runtime_suspend,
	.runtime_resume = pm_generic_runtime_resume,
	USE_PLATFORM_PM_SLEEP_OPS
};

struct bus_type platform_bus_type = {
	.name		= "platform",
	.dev_groups	= platform_dev_groups,
	.match		= platform_match, // 平台总线匹配逻辑 
	.uevent		= platform_uevent,
	.pm		= &platform_dev_pm_ops,
};
EXPORT_SYMBOL_GPL(platform_bus_type);


/* include/linux/of_device.h */
static inline int of_driver_match_device(struct device *dev,
					 const struct device_driver *drv)
{
	return of_match_device(drv->of_match_table, dev) != NULL;
}

static int platform_match(struct device *dev, struct device_driver *drv)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct platform_driver *pdrv = to_platform_driver(drv);

	/* 如果 平台设备 有 driver_override, 则仅考虑 匹配 device_driver的 name */
	if (pdev->driver_override)
		return !strcmp(pdev->driver_override, drv->name);

	/* 尝试匹配 struct device_driver 结构体的 of_match_table 表，
	   以及设备树节点 compatible 属性，若成功则结束匹配 */
	if (of_driver_match_device(dev, drv))
		return 1;

	/* Then try ACPI style match */
	if (acpi_driver_match_device(dev, drv))
		return 1;

	/* 如果 平台驱动 存在id_table表，则仅考虑与 平台设备的name 比较 */
	if (pdrv->id_table)
		return platform_match_id(pdrv->id_table, pdev) != NULL;

	/*  匹配平台设备的 name 以及 struct device_driver 结构体的 name */
	return (strcmp(pdev->name, drv->name) == 0);
}
```



### probe函数以及实现常用内核API

```c
// 匹配成功会调用的probe函数
int (*probe)(struct platform_device *);

// 获取device的资源信息，返回的结构体可访问 start end 等字段
struct resource *platform_get_resource(
    struct platform_device *,	// 平台总线设备
    unsigned int,	// 匹配资源里面的 flags字段，表示资源类型
    unsigned int	// 
);
// 获取平台设备的中断资源
int platform_get_irq(
    struct platform_device *,
    unsigned int
);
// 
struct resource *platform_get_resource_byname(
    struct platform_device *,
    unsigned int,
    const char *
);

int platform_get_irq_byname(
    struct platform_device *,
    const char *
);
```

当加载 device模块 或 driver模块 时，平台总线会自动进行匹配，如果匹配成功，则会自动调用probe回调函数。

在probe函数中，driver可以获得device的resource信息，即地址、中断号等等信息。

# 并发与竞争

## 原子操作

```c
#include <linux/atomic.h>
#include <asm/atomic.h>
// 如果是32位，则使用 atomic32_t
static atomic64_t v = ATOMIC_INIT(1); // 原子整数初始化为数值1

// 原子运算，并返回结果
#define atomic64_dec_return(v) atomic64_sub_return(1,(v))
#define atomic64_inc_return(v) atomic64_add_return(1,(v))
// negative 结尾，结果为负数返回真，否则返回假
#define atomic64_add_negative(a, v) (atomic64_add_return((a), (v)) < 0)
// test 结尾，结果为0返回真，结果非0返回假
#define atomic64_sub_and_test(i,v) (atomic64_sub_return((i), (v)) == 0)
#define atomic64_inc_and_test(v) (atomic64_add_return(1, (v)) == 0)
#define atomic64_dec_and_test(v) (atomic64_sub_return(1, (v)) == 0)
```

## 自旋锁

```c
#include <linux/spinlock.h>

static spinlock_t lock;
DEFINE_SPINLOCK(spinlock_t *lock); // 定义并初始化自旋锁
void spin_lock_init(spinlock_t *lock); // 初始化自旋锁
void spin_lock(spinlock_t *lock); // 加锁
int spin_trylock(spinlock_t *lock); // 解锁
int spin_is_locked(spinlock_t *lock); // 尝试获取自旋锁，失败则返回0
void spin_unlock(spinlock_t *lock); // 自旋锁已经被获取则返回0，否则返回非0

// 与相关中断API
void spin_lock_irq(spinlock_t *lock); // 关闭中断并加锁
void spin_unlock_irq(spinlock_t *lock); // 打开中断并解锁
void spin_lock_irqsave(spinlock_t *lock); // 保存中断状态，关闭中断并加锁
void spin_unlock_irqrestore(spinlock_t *lock); // 回复保存的中断状态，打开中断并解锁
void spin_lock_bh(spinlock_t *lock); // 关闭下半部，加锁
void spin_unlock_bh(spinlock_t *lock); // 打开下半部，解锁

```

自旋锁多用于多核CPU，但单核CPU也可以使用。

自旋锁会防止进程切换，获取失败时也不会让出CPU。

自旋锁的持有时间应当尽可能短。

驱动中发生死锁会引起整个机器崩溃。

发生死锁的情况：

1. 进程获取自旋锁之后，调用会引起休眠或阻塞的函数

   假设 在单核情况下，进程A持有锁后 让出了CPU，接着进程B进入运行态并获取自旋锁。此时进程B既不能获取自旋锁也不能让出CPU，则发生了死锁。

2. 临界区内被中断打断，中断中又重新对同一个自旋锁加锁

3. 对同一个自旋锁加锁两次

## 信号量

```c
#include <linux/semaphore.h>

DEFINE_SEMAPHORE(sem); // 定义信号量，并设置值为1

void sema_init(struct semaphore *sem, int val); // 初始化信号量，并设置值为val
void down(struct semaphore *sem); // 获取信号量，不可被中断
void up(struct semaphore *sem); // 释放信号量，不可被中断

int down_interruptible(struct semaphore *sem); // 获取信号量，能被中断
int down_trylock(struct semaphore *sem); // 尝试获取信号量，成功返回0，否则返回非0，不会引起休眠
int down_timeout(struct semaphore *sem, long jiffies); // 获取了信号量，成功返回0，超时则返回错误-ETIME
```

信号量获取失败时，会引起调用者的休眠。

如果持有资源时间比较长，则信号量比自旋锁更适合。

## 互斥锁

```c
#include <linux/mutex.h>

EFINE_MUTEX(mutexname); // 定义互斥锁，并初始化
void mutex_init(struct mutex *lock); // 初始化互斥锁，不允许加锁后初始化
void mutex_destroy(struct mutex *lock); // 销毁互斥锁，空实现，可不调用
// 互斥锁使用
void mutex_lock(struct mutex *lock); // 加锁
int mutex_trylock(struct mutex *lock); // 尝试加锁，成功返回0，否则返回非0，不会引起休眠
void mutex_unlock(struct mutex *lock); //解锁


```

互斥锁与值为1的信号量相似，但互斥锁更简洁高效。

互斥锁加锁失败也会引起调用者休眠，所以中断中不可使用互斥锁。

同一时刻只能有一个线程获得互斥锁，且只能由持有者解锁。

不允许递归上锁和解锁。



# 设备树

## 设备树语法

### 设备树特殊节点

+ chosen

chosen 并不是一个真实的设备，chosen 节点主要是为了 uboot 向 Linux 内核传递数据，重点是 bootargs 参数。

uboot 在启动 Linux 内核的时候会将 bootargs 的值传递给 Linux内核，bootargs 会作为 Linux 内核的命令行参数。uboot 会自己在 chosen 节点里面添加了 bootargs 属性！并且设置 bootargs 属性的值为 bootargs 环境变量的值。

+ alias

aliases 节点的主要功能就是定义别名，定义别名的目的就是为了方便访问节点。

### 设备树常见属性

+ compatible

  字符串，用于将设备和驱动结合起来。

  根节点的compatible属性会被用来检测内核是否支持设备。使用设备树之后不再使用机器ID来判断内核是否能够启动了。

+ model

  字符串，用于描述模块的信息。

+ status

  字符串，可能为 "okay" "disabled" "fail" "fail-sss"，用于表示是否可操作。

+ #address-cells、#size-cells

  无符号整型，表示子节点reg属性中，内存地址、地址长度 分别占用几个字长。

+ reg

  一组或者若干组 (address，length)对，，一般都是某个外设的寄存器地址范围信息。

### 经典设备树结构

imx6ull.dtsi

+ root
  + aliaes
  + cpus
  + interrupt-controller
  + clocks
  + soc：描述了内部外设，比如ecspi1~4、uart1~8、usbphy1~2、i2c1~4 等等

## 设备树编译

```shell
# 编译设备树，输入为dts格式，输出为dtb格式，输出文件名为xxx.dtb 输入文件名为 xx.dts
dtc -I dts -0 dtb -o xxx.dtb xxx.dts

# 反编译设备树，输入为dtb格式，输出为dts格式，输出文件名为xxx.dts 输入文件名为 xx.dtb
dtc -I dtb -0 dts -o xxx.dts xxx.dtb
```

编译内核时，会把 设备树编译器dtc 编译出来。



## 内核处理dtb

设备树可能与linux内核分开存放（恩智浦），也可能打包到一起（瑞芯微）。

uboot启动内核时会把设备树内存地址传递给linux内核。

内核会把dtb每一个节点都转换成 device_node ，部分 device_node 会转换成 platform_device（平台总线模型的device部分）。 

相关内核函数： of_platform_default_populate_init

转换成 platform_device 的节点，要满足以下条件：

1. 节点含有有 compatible 属性，且值不包括"arm,primecell" 任意一个值。（会转换成amba设备）
2. 父节点为根节点，或者父节点的 compatible 属性包含"simple-bus", "simple-mfd", "isa","arm,amba-bus" 其中之一。
3. 父节点不是 I2C，SPI。



## 设备树相关内核API

Linux 内核给我们提供了一系列的函数来获取设备树中的节点或者属性信息，这一系列的函数都有一个统一的前缀“of_”，所以在很多资料里面也被叫做 OF 函数。这些 OF 函数原型都定义在 include/linux/of.h 文件中。

```c
#include <linux/of.h>

struct property {
	char	*name;	// 属性名，如"compatible"
	int	length;		// 属性值长度
	void	*value;	// 属性值
	struct property *next;	// 本节点的下一个属性
	unsigned long _flags;
	unsigned int unique_id;
	struct bin_attribute attr;
};

struct device_node {
	const char *name; // 节点的 "name" 属性值
	const char *type;
	phandle phandle;
	const char *full_name;	// 节点的名字，设备树大括号前
	struct fwnode_handle fwnode;

	struct	property *properties;
	struct	property *deadprops;	/* removed properties */
	struct	device_node *parent;
	struct	device_node *child;
	struct	device_node *sibling;
	struct	kobject kobj;
	unsigned long _flags;
	void	*data;
#if defined(CONFIG_SPARC)
	const char *path_component_name;
	unsigned int unique_id;
	struct of_irq_controller *irq_trans;
#endif
};

// 从from节点开始 根据 name属性 寻找节点，如果from 为NULL 表示从根节点查找
struct device_node *of_find_node_by_name(struct device_node *from, const char *name);
// 根据设备树的全路径开始查找
struct device_node *of_find_node_by_path(const char *path);
// 从from节点开始 根据 device_type compatible属性 寻找节点，如果from 为NULL 表示从根节点查找，如果type为NULL表示忽略该参数
struct device_node *of_find_compatible_node(struct device_node *from, const char *type, const char *compat);

```



# 热插拔

热插拔机制有devfs、udev、mdev，其中 mdev 是udev的简化版本，常用于嵌入式设备中。

mdev是基于uevent_helper机制，内核产生的uevent会调用uevent_helper所指的用户程序mdev来执行热插拔动作。



# pintctrl与gpio子系统

## pinctrl 子系统

pinctrl 子系统管理 200 个 IO 口的上拉下拉电阻，电流驱动能力，是硬件底层的存在。如果 pinctrl 将某个 pin  脚初始化成了普通 GPIO 而不是 IIC 或者 SPI，那么接下来我们就可以使用 gpio 子系统的 API 去操作 IO 口输出高低电平。

传统的配置 pin 的方式就是直接操作相应的寄存器，但是这种配置 方式比较繁琐、而且容易出问题(比如 pin 功能冲突)。pinctrl 子系统就是为了解决这个问题而引入的，pinctrl 子系统主要工作内容如下：

1. 获取设备树中 pin 信息。

2. 根据获取到的 pin 信息来设置 pin 的复用功能

3. 根据获取到的 pin 信息来设置 pin 的电气特性，比如上/下拉、速度、驱动能力等。

对于我们使用者来讲，只需要在设备树里面设置好某个 pin 的相关属性即可，其他的初始化工作均由 pinctrl 子系统来完成，pinctrl 子系统源码目录为 drivers/pinctrl。



pinctrl 子系统也是一个标准的 platform 驱动，也存在相应的设备树节点、驱动代码、驱动probe函数等。

以IMX6ULL为例，其驱动代码位于 drivers\pinctrl\freescale\pinctrl-imx6ul.c

+ 模块加载优先级

​	pinctrl 子系统 是许多模块正常工作的基础，需要比其他模块优先加载，所以 pinctrl 子系统不是用 宏定义module_init(XXX_init) 来声明入口函数，而是使用 arch_initcall(XXX_init) 去声明，保证在系统启动的时候它会优先加载。

+ 设置引脚复用时机

​	在其他模块（如 IIC 485 等）加载时，pinctrl子系统会在 该模块probe函数执行前，设置引脚的复用关系。

### pinctrl 设备树

pinctrl 的设备树可以分为 client 和sevice两部分。

其中client具备固定格式，而service因平台（瑞芯微、恩智浦……）的不同而不同。

```css
/* 客户端例子1 */
{
	pinctrl-names = "default"; /* 该属性表示设备状态，此处表示状态0为"default" */
	pinctrl-0 = <&pinctr_i2c2>; /* 该属性表示状态0对应的引脚在 节点pinctr_i2c2 中配置 */
	status = "okay";
	...
	led-gpios = <&gpio1 3 GPIO_ACTIVE_LOW> /* GPIO1_IO03 低电平有效，该属性名字自定义 */
}

/* 客户端例子2 */
{
	pinctrl-names = "default", "wake up"; /* 有两个状态 */
	pinctrl-0 = <&pinctrl_hog_1 &pinctrl_hog_2>; /* 状态0("default") 由两个节点配置 */
	pinctrl-1 = <&pinctrl_hog_3>; /* 状态1("wake up") 由节点pinctrl_hog_3配置 */
	...
}
```

```css
/* 
 服务端例子 恩智浦
 以 IMX6ULL 为例，其IO复用相关节点有：
 iomuxc: iomuxc@020e0000
 iomuxc_snvs: iomuxc-snvs@02290000
 gpr: iomuxc-gpr@020e4000
 分别于参考手册上的寄存器地址映射相对应。，位于 arch\arm\boot\dts\imx6ull.dtsi
*/

/* 以上节点(如 iomuxc)的子节点(如 pinctrl_i2c2)会定义引脚的具体配置，其关系如下 */
&iomuxc{
	/* 该节点位于 arch\arm\boot\dts\imx6ull-alientek-emmc.dts，与具体开发板相关 */
    /* 把原本为 uart5 的两个引脚,复用成 i2c 的引脚 */
    pinctrl_i2c2: i2c2grp {
    	/* 第一个宏(MX6UL_PAD_XXX) 为 5个数值 */
        /* 宏定义位于 arch\arm\boot\dts\imx6ul-pinfunc.h */
    	/* 后面的一个数值(0x400xxx) 表示电气属性，该节点内的值含义 可查手册 */
        fsl,pins = <
            MX6UL_PAD_UART5_TX_DATA__I2C2_SCL 0x4001b8b0
            MX6UL_PAD_UART5_RX_DATA__I2C2_SDA 0x4001b8b0
        >;
    };
}

/* 服务端例子 瑞芯微 */
&iomuxc{
	uart7m1_xfer:uart7m1-xfer{
		rockchip.pins = 
			/* 把gpio3的 c5和c4引脚,设置成复用4功能，电气属性为上拉 */
			<3 RK_PC5 4 &pcfg_pull_up>,
			<3 RK_PC4 4 &pcg_pull_up>;
	}
}
```



### pinctrl 内核代码

```c
#include <linux/pinctrl/pinctrl.h>
struct pinctrl_desc {
	const char *name;					// pinctrl结构体名字
	struct pinctrl_pin_desc const *pins; // 可控制的引脚
	unsigned int npins;			// pins 数组长度
	const struct pinctrl_ops *pctlops;	// 对引脚groups(如串口） 的操作集、可选项
	const struct pinmux_ops *pmxops;	// 对引脚复用配置 的操作集
	const struct pinconf_ops *confops;	// 对引脚配置（如上下拉）的操作集
	struct module *owner;		// 拥有者，往往赋值为 THIS_MODULE
};

// 根据构建好的 pinctrl_desc 生成一个设备

// 用于获取设备树中自己用 pinctrl 建立的节点的句柄
struct pinctrl *devm_pinctrl_get(struct device *dev);

// 用于选择其中一个 pinctrl 的状态
struct pinctrl_state * pinctrl_lookup_state(
    struct pinctrl *p,
    const char *name
);

// 在上一步获取到某个状态以后，这一步真正设置为这个状态
int pinctrl_select_state(
    struct pinctrl *p,
    struct pinctrl_state *s
);
```

pinctrl_desc 是内核定义的结构体，在芯片厂家的驱动代码，如 pinctrl的驱动probe 函数中，往往把它封装一层。

如 瑞芯微定义的 struct rockchip_pinctrl 结构体，就封装了 struct pinctrl_desc  struct pinctrl_dev 结构体。

设备树种的节点会转换成结构体 struct pinctrl_map，该结构体描由 struct pinctrl_maps 结构体以链表形式进行管理。



在 Linux 中，加 devm_ 开头的函数，代表这个函数支持资源管理。

> 一般情况下，我们写一个驱动程序，在程序开头都会申请资源，比如内存、中断号等，万一后面哪一步申请出错，我们要回滚到第一步，去释放已经申请的资源，这样很麻烦。后来 Linux 开发出了很多 devm_ 开头的函数，代表这个函数有支持资源管理的版本，不管哪一步出错，只要错误退出，就会自动释放所申请的资源。



设备树中 pinctrl controller节点的 function 和 group 数据会转换成 pinctrl_map 结构体中的数据。

```c
#include <linux/pinctrl/machine.h>

struct pinctrl_map_mux {
	const char *group;
	const char *function;
};

struct pinctrl_map_configs {
	const char *group_or_pin;
	unsigned long *configs;		// 配置信息 数组
	unsigned num_configs;		// cofigs数组的长度
};

struct pinctrl_map {
	const char *dev_name; 			// 使用这个map的设备的名字
	const char *name;				// 本map的名字
	enum pinctrl_map_type type;		// 转换的类型，如 虚拟化类型、引脚配置类型、引脚组类型
	const char *ctrl_dev_name;		// 所属控制器的名字
	union {
		struct pinctrl_map_mux mux; // 含 字符串 group 和 function
		struct pinctrl_map_configs configs;
	} data;
};
```



## GPIO 子系统

在实现驱动函数 read、write 时，可以通过操作寄存器的方式来控制GPIO，但这种方式太低效且不通用。

BSP工程师会根据芯片实现GPIO子系统，GPIO 子系统封装了常见的GPIO功能，这样编写驱动的时候就可以调用GPIO 子系统的API，不再关心芯片寄存器等细节。

###  GPIO 设备树

```css
/*
 * gpio 控制器 来自linux 文档：
 * Documentation\devicetree\bindings\gpio\fsl-imx-gpio.txt
 */
gpio0: gpio@73f84000 {
	compatible = "fsl,imx51-gpio", "fsl,imx35-gpio";
	reg = <0x73f84000 0x4000>;
	interrupts = <50 51>;
	gpio-controller;
	#gpio-cells = <2>;
	interrupt-controller;
	#interrupt-cells = <2>;
};

/* gpio 使用节点 */
/ {
    led:led@1{
        compatible="led"; /* 可用于匹配驱动 */
        /* 属性名(my_gpios)可以自定义，但要与驱动中of_get_named_gpio参数一致 */
        /* cell的个数由gpio控制器(gpio0)指定，此处对应值为2：引脚 + 默认电平 */
        my_gpios=<&gpio0 RK_PB7 1>
    }
};
```

### gpio 内核代码

GPIO的API分为两套接口:

| 差异项       | 新接口                  | 老接口            |
| ------------ | ----------------------- | ----------------- |
| 接口前缀     | gpiod_                  | gpio_             |
| 引脚句柄     | struct gpio_desc 结构体 | 整数值            |
| 输出控制     | 逻辑电平                | 物理电平          |
| 设备树属性名 | xxx-gpios               | xxx（无格式限制） |
|              |                         |                   |

```c
#include <linux/of_gpio.h>
// 从设备树节点中获取 GPIO号，是用于唯一标识GPIO引脚的标号
// 失败返回错误码,返回值可用 gpio_is_valid 校验
// 即使返回正确的的GPIO号，也不代表成功获取GPIO，需要调用gpio_request获取控制权
int of_get_named_gpio(
    struct device_node *np, // 设备节点
    const char *propname, // 属性名称
    int index // 节点第index个 GPIO，只有一个则输入0
);
// 用于判断返回的 gpio号 是否为合法值
bool gpio_is_valid(int number);

/*************************************************************/
#include <linux/gpio.h>
// 请求这个gpio，如果其他地方请求了这个gpio且没有释放，那么会失败
int gpio_request(unsigned gpio, const char *label);
// 把一个 gpio 设置为输入 
int gpio_direction_input(unsigned gpio);
// 把一个 gpio 设置为输出
int gpio_direction_output(unsigned gpio, int value);
// 获取gpio的 电平状态
int gpio_get_value(unsigned gpio);
// 设置gpio的输出电平
void gpio_set_value(unsigned gpio, int value);
// 释放GPIO
void gpio_free(unsigned gpio);
void gpio_free_array(const struct gpio *array, size_t num);
```

```c
#include <linux/gpio/consumer.h>
/* 新一套 API，使用结构体描述一个GPIO */
// 请求gpio
struct gpio_desc *gpiod_get(
    struct device *dev, // 可来自 platform_device 的 dev成员
    const char *con_id // 为属性名(xxx-gpios)中的xxx
);
// 把一个 gpio 设置为输入 
int gpiod_direction_input(struct gpio_desc *desc);
// 把一个 gpio 设置为输出
int gpiod_direction_output(struct gpio_desc *desc, int value);
// 获取gpio的 电平状态
int gpiod_get_value(struct gpio_desc *desc);
// 设置gpio的输出电平
void gpiod_set_value(struct gpio_desc *desc, int value);
// 释放gpio
void gpiod_put(struct gpio_desc *desc);
```

# input 子系统

麦克风、键盘、鼠标，按键等输入设备可以使用input子系统编写驱动。

input 子系统的设备属于字符设备的一种，其主设备号固定为13。

+ 驱动层 使用input子系统之后就不用在 /class 和 /dev 创建文件，也不用注册file_operation，这些由input子系统完成，创建的设备文件在 /dev/input 目录下。

+ 应用层 读取设备文件的得到数据 为 二进制数据，用于表示事件， 可以用 linux/input.h 的结构体 struct input_event 来解析读取得到的二进制数据。

## input 相关内核源码

```c
#include <linux/input.h>

// 事件定义
#define EV_SYN			0x00 // 同步事件
#define EV_KEY			0x01 // 按键事件
#define EV_REL			0x02 // 相对位移事件（如鼠标移动）
#define EV_ABS			0x03 // 绝对位置事件（如触屏位置）
#define EV_MSC			0x04 // 杂项设备事件
#define EV_SW			0x05 // 开关事件
#define EV_LED			0x11 // LED
#define EV_SND			0x12 // sound声音
#define EV_REP			0x14 // 可重复事件（如按键连按）
#define EV_FF			0x15 // 压力事件（如压力传感器）
#define EV_PWR			0x16 // 电源事件
#define EV_FF_STATUS	0x17 // 压力状态事件
#define EV_MAX			0x1f
#define EV_CNT			(EV_MAX+1)

// input 设备结构体
struct input_dev {
	const char *name; // 设备名称
	const char *phys;
	const char *uniq; // 设备唯一识别码（如果设备有）
	struct input_id id;

	unsigned long propbit[BITS_TO_LONGS(INPUT_PROP_CNT)];
	// 使用位图来表示 支持的事件类型 和 各种事件
	unsigned long evbit[BITS_TO_LONGS(EV_CNT)]; // 设备支持事件类型
	unsigned long keybit[BITS_TO_LONGS(KEY_CNT)];
	unsigned long relbit[BITS_TO_LONGS(REL_CNT)];
	unsigned long absbit[BITS_TO_LONGS(ABS_CNT)];
	unsigned long mscbit[BITS_TO_LONGS(MSC_CNT)];
	unsigned long ledbit[BITS_TO_LONGS(LED_CNT)];
	unsigned long sndbit[BITS_TO_LONGS(SND_CNT)];
	unsigned long ffbit[BITS_TO_LONGS(FF_CNT)];
	unsigned long swbit[BITS_TO_LONGS(SW_CNT)];

	unsigned int hint_events_per_packet;

	unsigned int keycodemax;
	unsigned int keycodesize;
	void *keycode;

	int (*setkeycode)(struct input_dev *dev,
			  const struct input_keymap_entry *ke,
			  unsigned int *old_keycode);
	int (*getkeycode)(struct input_dev *dev,
			  struct input_keymap_entry *ke);

	struct ff_device *ff;

	unsigned int repeat_key;
	struct timer_list timer;

	int rep[REP_CNT];

	struct input_mt *mt;

	struct input_absinfo *absinfo;

	unsigned long key[BITS_TO_LONGS(KEY_CNT)];
	unsigned long led[BITS_TO_LONGS(LED_CNT)];
	unsigned long snd[BITS_TO_LONGS(SND_CNT)];
	unsigned long sw[BITS_TO_LONGS(SW_CNT)];

	int (*open)(struct input_dev *dev);
	void (*close)(struct input_dev *dev);
	int (*flush)(struct input_dev *dev, struct file *file);
	int (*event)(struct input_dev *dev, unsigned int type, unsigned int code, int value);

	struct input_handle __rcu *grab;

	spinlock_t event_lock;
	struct mutex mutex;

	unsigned int users;
	bool going_away;

	struct device dev;

	struct list_head	h_list;
	struct list_head	node;

	unsigned int num_vals;
	unsigned int max_vals;
	struct input_value *vals;

	bool devres_managed;
};

// 用于表示发生的事件的结构体，用于应用层解析事件
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};

// 向内核申请一个 input 设备
struct input_dev __must_check *input_allocate_device(void);
// 向内核注册 input 设备，
int __must_check input_register_device(struct input_dev *);

// 函数用于上报事件， dev 为设备结构体指针
// type 为事件类型，如EV_KEY
// code 为事件码，也就是注册的按键，如 KEY_0 KEY_1
// value 为事件值，比如 0 1 分别表示按下和松开
void input_event(struct input_dev *dev, unsigned int type, unsigned int code, int value);

// 调用上报事件函数之后，一定要使用同步函数，同步本质也是封装调用 input_event
void input_sync(struct input_dev *dev);

// 注销已经注册的 input 设备
void input_unregister_device(struct input_dev *);
// 释放 input 设备 结构体
void input_free_device(struct input_dev *dev);
```

# I2C总线框架

linux中，I2C设备的驱动分为两层：

+ I2C适配器驱动，也叫I2C控制器驱动，由原厂BSP工程师编写。
+ I2C设备驱动，如使用I2C接口的MPU6050等，由驱动工程师编写。



## I2C适配器驱动

```c
#include <linux/i2c.h>

/* 部分表示 I2C 功能的标志位 */
#define I2C_FUNC_I2C			0x00000001
#define I2C_FUNC_10BIT_ADDR		0x00000002
#define I2C_FUNC_PROTOCOL_MANGLING	0x00000004 /* I2C_M_IGNORE_NAK etc. */
#define I2C_FUNC_SMBUS_PEC		0x00000008
#define I2C_FUNC_NOSTART		0x00000010 /* I2C_M_NOSTART */

/*
 * i2c_algorithm 是一类硬件解决方案的接口，这些解决方案可以使用相同的总线算法来解决,例如bit-banging 或 PCF8584，这是最常见的两个。
 * @master_xfer:与i2c适配器进行一组i2c事务通信，通信内容由num个元素的 msgs 数组表示。
 * @smbus_xfer:向给定的I2C适配器发出smbus事务。如果不存在此回调，那么总线层将尝试将SMBus调用转换为I2C传输。
 * @function:函数返回值中用标志位表示支持的功能。
 * @reg_slave:将给定的客户端注册到该适配器的I2C从模式
 * @unreg_slave:从这个适配器的I2C从模式注销给定的客户端
 */
struct i2c_algorithm {
    /*
     * 如果适配器算法不能进行i2c级访问，则将master_xfer设置为NULL。如果适配器算法可以进行SMBus访问，则设置smbus_xfer。如果设置为NULL，则使用普通I2C消息模拟SMBus协议，master_xfer 应该返回成功处理的消息数，如果出错则返回负值。
     * 错误码可参考内核文档 Documentation/i2c/fault-codes。
	*/
	int (*master_xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs, int num);
	int (*smbus_xfer) (struct i2c_adapter *adap,
                       u16 addr,
                       unsigned short flags,
                       char read_write,
                       u8 command,
                       int size,
                       union i2c_smbus_data *data);
	/* 决定适配器支持的功能 */
	u32 (*functionality) (struct i2c_adapter *);
};

/*
 * 表示一个物理I2C总线
 */
struct i2c_adapter {
	struct module *owner;
	unsigned int class;		  /* classes to allow probing for */
	const struct i2c_algorithm *algo; /* 操控硬件的读写算法 */
	void *algo_data;

	/* data fields that are valid for all devices	*/
	struct rt_mutex bus_lock;

	int timeout;			/* in jiffies */
	int retries;
	struct device dev;		/* the adapter device */

	int nr;
	char name[48];
	struct completion dev_released;

	struct mutex userspace_clients_lock;
	struct list_head userspace_clients;

	struct i2c_bus_recovery_info *bus_recovery_info;
	const struct i2c_adapter_quirks *quirks;
};


/*
 * 注册i2c适配器，使用指定的的总线号
 * @adap:待注册的适配器 (其中成员变量 adap->nr 要已经初始化)
 * 当一个I2C适配器的总线号很重要时，使用这个接口声明它。例如：SOC的I2C适配器，或其他内置到系统主板的适配器。
 * 如果请求的总线号(adap->nr)被设置为-1，那么这个函数的行为将与i2c_add_adapter相同，并将动态分配一个总线号。
 * 如果没有预先为这个总线声明设备，那么一定要在动态分配它之前注册适配器。否则，所需的总线ID可能不可用。
 * 成功时返回0值，失败时返回errno值的负数。
 */
int i2c_add_numbered_adapter(struct i2c_adapter *adap);

/*
 * 注册i2c适配器，使用动态分配的总线号
 * 当一个I2C适配器的总线号不重要时，使用这个接口声明它。例如:通过USB链接或PCI插件卡动态添加的I2C适配器。
 * 成功时返回0值，新的总线号被分配并存储在adap->nr中。失败时返回errno值的负数。
 */
int i2c_add_adapter(struct i2c_adapter *adapter);

/*
 * 注销i2c适配器
 */
void i2c_del_adapter(struct i2c_adapter *adap);
```



## I2C设备驱动

```c
// I2C设备的 驱动程序
struct i2c_driver {
	unsigned int class;

    /* 通知驱动有新的总线出现， 此函数已经被弃用，将会被删除 */
	int (*attach_adapter)(struct i2c_adapter *) __deprecated;

	/* 标准驱动模型接口，驱动匹配时调用probe，移除时调用remove */
	int (*probe)(struct i2c_client *, const struct i2c_device_id *);
	int (*remove)(struct i2c_client *);

	/* 设备关机  */
	void (*shutdown)(struct i2c_client *);

	/* 警报回调，例如SMBus警报协议 */
	void (*alert)(struct i2c_client *, unsigned int data);

	/* 类似 ioctl，用来执行一些设备特定的功能 */
	int (*command)(struct i2c_client *client, unsigned int cmd, void *arg);
	
    /* 设备驱动模型的驱动部分，内含 owner name of_id_table 等 */
	struct device_driver driver;
    /* 驱动程序支持的I2C设备列表 */
	const struct i2c_device_id *id_table;

	/* 设备检测回调，用于自动创建设备 */
	int (*detect)(struct i2c_client *, struct i2c_board_info *);
	const unsigned short *address_list;
	struct list_head clients;
};
// 根据成员 struct device_driver driver的地址，获取 struct i2c_driver 的首地址
#define to_i2c_driver(d) container_of(d, struct i2c_driver, driver)

/*
 * 为了自动检测设备，需要同时定义 @detect 和 @address_list。 @class 也应该设置。
 * 在检测成功时， @detect 函数必须至少填充 i2c_board_info结构 的name字段，也可能填充flags字段。
 * 如果缺少 @detect函数，驱动程序仍然可以让 已经被枚举的设备 正常工作。
 */    

/*
* struct i2c_client -表示I2C从机设备
* @flags: 
* 	I2C_CLIENT_TEN 表示设备使用10位芯片地址;
* 	I2C_CLIENT_PEC 表示使用SMBus报文错误检测;
* @addr:连接到I2C总线上使用的地址。
* @name:表示设备的类型，通常是一个通用的芯片名称。
* @adapter:管理承载I2C设备的总线段
* @dev:从机的驱动模型设备节点。
* @irq:表示本设备产生的IRQ(如果有)
* @detected: i2c_driver的成员。客户端列表或i2c-core的userspace_devices列表
* @slave_cb:当适配器使用I2C从机模式时的回调。适配器调用它将从属事件传递给从属驱动程序。
*/
struct i2c_client {
	unsigned short flags;		/* div., see below */
	unsigned short addr;		/* 芯片地址: 7位	*/
	char name[I2C_NAME_SIZE];
	struct i2c_adapter *adapter; /* 适配器 */
	struct device dev;		/* device 结构体*/
	int irq;				/* 表示本设备产生的IRQ	*/
	struct list_head detected;
#if IS_ENABLED(CONFIG_I2C_SLAVE)
	i2c_slave_cb_t slave_cb;	/* callback for slave mode	*/
#endif
};

/*
 * 封装调用 i2c_register_driver
 */
// 注册 I2C总线的 驱动程序 到内核
#define i2c_add_driver(driver) \
	i2c_register_driver(THIS_MODULE, driver)

// 注册 I2C总线的 驱动程序 到内核
int i2c_register_driver(struct module *owner, struct i2c_driver *driver);

// 从内核中注销 I2C总线的 驱动程序
void i2c_del_driver(struct i2c_driver *driver);

/* I2C消息的 flags */
#define I2C_M_TEN			0x0010	/* 10比特芯片地址 */
#define I2C_M_RD			0x0001	/* 从从机芯片读数据 */
#define I2C_M_STOP			0x8000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_NOSTART */
#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */

// 表示I2C总线上的一个传输操作，可能为发送或者接收
struct i2c_msg {
	__u16 addr;		/* 从机地址	*/
	__u16 flags;	/* 标志位，表示读写操作或者属性 */
	__u16 len;		/* 数据buf长度 */
	__u8 *buf;		/* 数据buf指针	*/
};

// I2C 读写接口，进行一组I2C事务
int i2c_transfer(
    struct i2c_adapter *adap,	// I2C适配器，内部含硬件操作方法
    struct i2c_msg *msgs, 		// msg数组，每个msg元素代表一趟消息 
    int num						// msg数组元素个数
);
```

驱动代码中不创建 struct i2c_client 结构体，该结构体由系统解析设备树时创建。 

结构体 i2c_driver 与 platform_driver 类似，也有设备树compatible属性和 probe回调 等函数，只不过一个是虚拟的平台总线，一个是真实的 I2C总线。

### 不使用设备树

使用结构体 i2c_board_info 描述I2C设备信息。

### 使用设备树

I2C设备节点常用属性：

+ compatible：用于与 i2c_driver 匹配的字符串。
+ reg：一般为器件7位地址，查看相应器件手册获得，注意同一总线上地址不可冲突

```css
/* imx6ull-alientek-emmc.dts */
&i2c1 {
	clock-frequency = <100000>;
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_i2c1>;
	status = "okay";

	mag3110@0e {
		compatible = "fsl,mag3110";
		reg = <0x0e>;
		position = <2>;
	};

	fxls8471@1e {
		compatible = "fsl,fxls8471";
		reg = <0x1e>;
		position = <0>;
		interrupt-parent = <&gpio5>;
		interrupts = <0 8>;
	};
};
```

## 驱动编写

## NXP原厂I2C驱动分析

根据设备树compatible属性查找，可找到 "fsl,imx21-i2c" 相关的属性，位于 "drivers\i2c\busses\i2c-imx.c"。

驱动中读取设备树属性，其来源文件除了 imx6ull.dtsi 还有 imx6ull-alientek-emmc.dts。

NXP自定义结构体 struct imx_i2c_struct 封装了 内核结构体 struct i2c_adapter，并且使用了DMA 功能。

NXP驱动代码通过 struct i2c_algorithm 注册了两个回调函数

+ master_xfer：I2C读写操作最终通过此函数操作硬件

+ functionality：返回32位整数，其标志位表示支持的功能



# LCD屏幕驱动

## framebuffer帧缓存

framebuffer在内核中使用 结构体 struct fb_info 表示，LCD屏幕驱动的重点就是：

1. 初始化 struct fb_info 的各个成员变量。
2. 使用 register_framebuffer 函数向内核注册 struct fb_info 结构体。
3. 卸载驱动时调用 unregister_framebuffer 注销结构体。 



## NXP原厂LCD驱动分析

源码文件位于 drivers/video/fbdev/mxsfb.c

NXP定义了结构体 struct mxsfb_info，其包含了内核定义结构体 struct fb_info。 

mxsfb_probe 函数会调用 mxsfb_init_fbinfo 来初始化 fb_info，其中fb_info 中 fbops 成员 表示相关的操作集。mxsfb_probe 函数除了初始化和注册 fb_info 之外， 还初始化 LCDIF 控制器。

设备树节点名称：lcdif。
