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

![image-20230630010633688](E:\学习资料\嵌入式Linux\git备份\学习笔记\linux 驱动.assets\image-20230630010633688.png)



### mmap

应用程序调用mmap，最终会调用到驱动程序的 mmap。

![image-20230630014646124](E:\学习资料\嵌入式Linux\git备份\学习笔记\linux 驱动.assets\image-20230630014646124.png)





### cache 与 buffer

![image-20230630015450655](E:\学习资料\嵌入式Linux\git备份\学习笔记\linux 驱动.assets\image-20230630015450655.png)



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

![image-20230627223514668](E:\学习资料\嵌入式Linux\git备份\学习笔记\linux 驱动.assets\image-20230627223514668.png)

共享中断：多个外设共用同一个中断源。

非共享中断：一个中断源只有一个外设触发。

在linux内核中，每个中断源都有一个整数与之对应。

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





# 驱动基础

## 最简单驱动

```c
/* 必须包含的两个头文件 */
#include <linux/module.h>
#include <linux/init.h>

// 加载函数
static int helloworld_init(void)
{
    return 0;
}
// 卸载函数
static int helloworld_exit(void)
{
    return 0;
}
/* 必须使用宏指定 加载函数、卸载函数、GPL声明 */
module_init(helloworld_init);
module_exit(helloworld_exit);
MODULE_LICENSE("GPL");
// 可选的 作者、版本等信息
MODULE_AUTHOR("pdg");
MODULVERSION("v1.0");
```

以上代码展示了一个内核模块最精简的框架，他不关联任何的硬件，也不创建设备文件。

驱动编译方式：

1. 把驱动编译进内核，内核启动时自动加载
2. 把驱动编译成模块，使用 insmod命令 动态加载



## 字符设备

```c
#include <linux/cdev.h>
struct cdev {
	struct kobject kobj;
	struct module *owner;
	const struct file_operations *ops; // 文件操作方法集
	struct list_head list;
	dev_t dev; // 设备号 12位主设备号 + 20位次设备号
	unsigned int count;
};

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

// 注销分配得到的设备号
int unregister_chrdev_region(
    dev_t dev,  // 要释放的设备号，含主、次设备号
    unsigned count, // 设备号的数量
);

// 初始化 cdev结构体
void cdev_init(
    struct cdev *, // 需要初始化的结构体
    const struct file_operations * // 对应的文件操作
);
// 把字符设备添加到系统中
int cdev_add(
    struct cdev *, // 已经初始化的字符设备
    dev_t, // 对应的设备号
    unsigned // 设备的数量，它们的次设备号是连续的
);
// 把字符设备删除
void cdev_del(struct cdev *);
```

在模块初始化时 注册字符设备：

1. 创建 struct cdev结构体
2. 获取设备号和操作方法
3. 调用cdev_init和cdev_add添加字符设备

在模块卸载时 注销字符设备：

1. 注销分配的设备号
2. 调用cdev_del删除字符设备

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

compat_ioctl 函数与 unlocked_ioctl 函数功能一样，区别在于在 64 位系统上，32 位的应用程序调用将会使用此函数。在 32 位的系统上运行 32 位的应用程序调用的是unlocked_ioctl。

mmap 函数用于将将设备的内存映射到进程空间中(也就是用户空间)，一般帧缓冲设备会使用此函数，比如 LCD 驱动的显存，将帧缓冲(LCD 显存)映射到用户空间中以后应用程序就可以直接操作显存了，这样就不用在用户空间和内核空间之间来回复制。

open 函数用于打开设备文件。

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

杂项设备是特殊的字符设备。



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



# 运行调试

## 内核模块

+ insmod

  insmod xxx.ko，加载内核模块

  insmod xxx.ko a=1 array=1,2,3 str=hello 带参数地加载内核模块

+ rmmod

  rmmod xxx.ko，移除已经加载的内核模块 

+ modprob

  modprob xxx.ko 加载内核模块同时依赖的模块也一同加载

+ lsmod

  查看已经载入的模块，也可以 cat /proc/modules

+ modinfo

  查看模块信息

## 设备文件/文件夹

```shell
ls /sys/class/ #查看 class_create 创建的文件夹
ls /dev/ #查看 device_create 创建的文件(设备节点)，该文件可以使用 open close操作
ls -al /dev/xxx #可以看出设备类型，主设备号、次设备号


ls /sys/bus/platform/devices #查看加载的平台总线设备文件夹
ls /sys/bus/platform/drivers #查看加载的平台总线驱动文件夹
cat /proc/devices #查看所有系统已经使用设备号
cat /proc/(pid)/maps #查看进程使用的虚拟地址
```



## 设备树信息

目录 /proc/device-tree，所有设备树节点都以文件夹形式存在。

可在此查看所有节点、节点属性及其取值。



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
DECLARE_WAIT_QUEUE_HEAD(wq);

// 如果条件为假，那么线程会进入不可中断的休眠
wait_event(wq, condition);
// 如果条件为假，那么线程会进入可被中断的休眠
wait_event_interruptible(wq, condition);

// 唤醒队列头所有休眠线程
wake_up(wait_queue_head_t *wq);
// 唤醒队列头可中断的休眠线程
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
// 应用层调用 select 或者 poll 会调用驱动函数 poll
unsigned int (*poll) (struct file *, struct poll_table_struct *);

// 针对文件filp,把等待队列 wait_address 添加到 p 中
void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
```

驱动程序编写：

1. 驱动函数 poll 中，对可能引起设备文件状态变化的等待队列调用 poll_wait（是个非阻塞函数），将对应的等待队列头添加到 poll_table。
2. 驱动函数 poll 完成上述事情之后，返回是否能进行非阻塞读写的掩码。

## 信号驱动IO

信号驱动IO的方式不需要应用程序去观察IO的状态，而是通过SIGIO信号来通知应用程序。

应用程序编写：

1. 使用 signal 注册信号处理函数。
2. 设置能够接受这个信号的进程。
3. 开启信号驱动IO，可以使用fcntl 的F_SETFL命令设置FASYNC标志（会调用驱动fasync函数）。

驱动程序编写：

1. 定义 fasync 函数对应的原型
2. 在函数中调用 fasync_helper 来操作 fasync_struct 结构体

## 异步IO

可以使用用户空间的glibc库实现，不依赖于内核。



## linux 内核定时器

内核定时器精度不高，不能作为高精度定时器使用。内核定时器不是周期运行的，超时后会关闭，如果需要周期运行，需要在定时处理中重新打开定时器。





# 中断框架

## 基础知识

| GIC 中断源分类     | 中断号    | 描述                                            |
| ------------------ | --------- | ----------------------------------------------- |
| SGI 软件通用中断   | 0 - 15    | 用于core之间相互通信                            |
| PPI 私有外设中断   | 16 - 31   | 每个core私有的中断，如调度使用的tick中断        |
| SPI 共享外设中断   | 32 - 1020 | 由外设触发，如按键、串口等中断，可分配给不同CPU |
| LPI 基于消息的中断 |           | 不支持 GIC-V1 GIC-V2                            |

每个CPU仅有四个中断接口：FIQ、IRQ、virtual FIQ、virtual IRQ



硬件中断与linux中断框架：

![image-20230619012516471](E:\学习资料\嵌入式Linux\git备份\学习笔记\linux 驱动.assets\image-20230619012516471.png)



## 中断相关驱动API

```c
#include <linux/interrput.h>

// 中断返回值 枚举
enum irqreturn {
	IRQ_NONE		= (0 << 0),		// 中断与本设备无关
	IRQ_HANDLED		= (1 << 0),		// 本设备的中断发生了
	IRQ_WAKE_THREAD		= (1 << 1),	// 请求唤醒处理线程
};
typedef enum irqreturn irqreturn_t;

// 中断处理函数原型（如果任务较长，则在此函数内创建 tasklet中断下半部分）
typedef irqreturn_t (*irq_handler_t)(int, void *);

// 设备向内核注册中断处理函数 API
int request_irq(
    unsigned int irq,  		// 软件中断号
    irq_handler_t handler, 	// 注册的中断处理函数
    unsigned long flags, 	// 中断标志，如 共享中断、电平触发、边缘触发等
    const char *name, 		// 自定义的中断名字，可在 /proc/irq 中看到
    void *dev 				// 传给中断处理函数(handler)的 参数，如果flags设置了共享中断，则该参数不可为NULL，用来区分设备；
    						// 如果flags不为共享中断，该参数可以为 NULL
);

// 释放中断资源 API
void free_irq(
    unsigned int, 		// 软件中断号
    void *				// 如果为共享中断，该参数用来区分设备
);
```



```c
#include <linux/gpio.h>

// 根据原理图确定引脚名，再查阅SOC手册计算 gpio数值
// 输入 gpio数值 获取软件中断号
int gpio_to_irq(unsigned int gpio);

```



## 中断内核源码

```c
// 中断行为
struct irqaction {
	irq_handler_t		handler;	// 存放注册的 中断服务函数
	void				*dev_id; 	// 存放注册的 指针参数，用于共享中断卸载时，找到需要卸载的设备
	void __percpu		*percpu_dev_id;
	struct irqaction	*next;
	irq_handler_t		thread_fn;
	struct task_struct	*thread;
	unsigned int		irq;
	unsigned int		flags;		// 存放注册的 flags，含 是否共享、触发方式等
	unsigned long		thread_flags;
	unsigned long		thread_mask;
	const char		*name;			// 存放注册的 name指针
	struct proc_dir_entry	*dir;
} ____cacheline_internodealigned_in_smp;

// 中断描述
struct irq_desc {
	struct irq_data		irq_data;
	unsigned int __percpu	*kstat_irqs;
	irq_flow_handler_t	handle_irq;
	struct irqaction	*action;	/* IRQ action 链表，每个成员表示不同的外设
										如只有多个成员代表为本中断为共享中断 */
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

// 相关API
```

每个外设的外部中断，都是用 struct irq_desc 结构体描述的，所以也叫做中断描述符。 

linux管理中断方法可分为动态和静态。

如果定义了 CONFIG_SPACE_IRQ则使用静态方法，在ARM64上这个宏是默认打开的。

1. 静态方法

   使用静态全局变量数组 struct irq_desc irq_desc[NR_IRQS];

   其下标为中断号，可通过 中断号 获取 中断描述符结构体。

   中断描述符 含有成员 action， 里面的中断服务函数（中断上半部分）。

2. 动态方法

   使用动态 radix tree



注册的 中断处理函数(irq_handler_t) 仅处理紧急事件，如果其他有较多的任务，可创建中断下半部分来进行处理。此时的 中断处理函数(irq_handler_t) 又叫中断上半部分。

##  tasklet

内核中一般使用tasklet实现中断下半部分，有以下限制：

tasklet 绑定的函数同一时间只能在一个cpu上运行。

tasklet 绑定的函数不可以使用任何引起休眠的函数，否则会引起内核异常。

tasklet 绑定的函数其实是运行在 TASKLET_SOFTIRQ 软中断 上下文中。

```c
/* Tasklets --- multithreaded analogue of BHs.

   Main feature differing them of generic softirqs: tasklet
   is running only on one CPU simultaneously.

   Main feature differing them of BHs: different tasklets
   may be run simultaneously on different CPUs.

   Properties:
   * If tasklet_schedule() is called, then tasklet is guaranteed
     to be executed on some cpu at least once after this.
   * If the tasklet is already scheduled, but its execution is still not
     started, it will be executed only once.
   * If this tasklet is already running on another CPU (or schedule is called
     from tasklet itself), it is rescheduled for later.
   * Tasklet is strictly serialized wrt itself, but not
     wrt another tasklets. If client needs some intertask synchronization,
     he makes it with spinlocks.
 */

struct tasklet_struct
{
	struct tasklet_struct *next; 	// 链表下一个节点
	unsigned long state;·			// 调度状态
	atomic_t count;					// 0:使能 非0:未使能
	void (*func)(unsigned long);	// 绑定中断下文的函数
	unsigned long data;				// func 的参数
};

// 把某个tasklet加入调度，如果调度选中时为使能状态，则会执行相应函数
static inline void tasklet_schedule(struct tasklet_struct *t)
// 把某个已经使能的 tasklet 移出调度，如果目标正在执行会等待其执行完成
void tasklet_kill(struct tasklet_struct *t);
```

+ 模块初始化 **modue_init**()
  + 取得中断号 并 注册中断上半部分 **request_irq**()
    + 在中断上半部分中，把 tasklet 加入调度 **tasklet_schedule**()
  + 可动态初始化 tasklet，默认为使能状态。 **tasklet_init**()
+ 模块退出 **module_exit**()
  + 释放中断 **free_irq**()
  + 可以使能tasklet，防止下次不能成功运行 **tasklet_enable**()
  + 把 tasklet 移出调度 **tasklet_kill**()



## 软中断

软中断也是实现中断下半部分的方法之一，但是软中断资源有限，中断号不多，一般用于网络设备驱动、块设备驱动当中。

内核开发者希望驱动工程师使用tasklet而不是软中断，所以 open_softirq()、 raise_softirq() 正常情况下不导出到符号表。

```c
/* PLEASE, avoid to allocate new softirqs, if you need not _really_ high
   frequency threaded job scheduling. For almost all the purposes
   tasklets are more than enough. F.e. all serial device BHs et
   al. should be converted to tasklets, not to softirqs.
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
	RCU_SOFTIRQ,    /* Preferable RCU should always be the last softirq */

	NR_SOFTIRQS
};

// 软中断 API
// 软中断初始化
void softirq_init(void);
// 创建一个软中断
void open_softirq(int nr, void (*action)(struct softirq_action *));
// 使能一个软中断
void raise_softirq(unsigned int nr);
// 关闭一个软中断
void raise_softirq_irqoff(unsigned int nr);
```

## 工作队列

工作队列是 实现中断下半部分的机制之一，与 tasklet 的区别是工作队列 处理过程中可以进行休眠，可以执行比tasklet 更耗时的工作。

工作队列分为共享工作队列和自定义工作队列，如果共享工作队列不满足要求，可以使用自定义工作队列。

共享工作队列：

+ 内核已经创建共享工作队列，该队列内有各种不同的驱动的工作
+ 驱动只需要往 linux全局队列 添加自定义的工作

自定义工作队列：

+ 需要自己创建工作队列
+ 队列的维护需要消耗资源，故不优先使用。

```c
#include <linux/workqueue.h>
// 工作队列中的工作
struct work_struct {
	atomic_long_t data;
	struct list_head entry;	// 
	work_func_t func;		// 回调函数，具体的工作
};
// 工作队列回调原型
typedef void (*work_func_t)(struct work_struct *work);

// API
// 把一个工作添加到 linux 的 共享工作队列 中进行调度
bool schedule_work(struct work_struct *work);
// 取消一个已经调度的工作队列，如果正在执行，会等待执行完成
bool cancel_work_sync(struct work_struct *work);

// 创建工作队列，输入自定义的名字，返回工作队列指针 宏定义
struct workqueue_struct *create_workqueue(const char *name);
// 把一个工作添加到 特定的工作队列 中进行调度
bool queue_work(struct workqueue_struct *wq, struct work_struct *work);
```



自定义工作队列使用：

+ 模块初始化 **modue_init**()
  + 取得中断号 并 注册中断上半部分 **request_irq**()
    + 在中断上半部分中，把 tasklet 加入调度 **tasklet_schedule**()
  + 可动态初始化 tasklet，默认为使能状态。 **tasklet_init**()
+ 模块退出 **module_exit**()
  + 释放中断 **free_irq**()
  + 可以使能tasklet，防止下次不能成功运行 **tasklet_enable**()
  + 把 tasklet 移出调度 **tasklet_kill**()

工作队列可以使用命令  ps -aux 查看



### 延时工作队列

### 工作队列传参

### 并发管理工作队列

根据工作队列的flag参数，其任务可以分配给 CPU上运行的不同线程来执行：

+ CPU内 高优先级线程
+ CPU内 正常优先级线程
+ 不与 CPU 绑定的线程

## 中断线程化

中断线程化也是实现中断上半部分和下半部分的方式之一。

下半部分的任务会交给一个中断线程来处理，这个内核线程只用于这个中断。

当发生中断的时候，中断上半部分会唤醒这个内核线程，然后由这个内核线程来执行下半部分的函数。



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
struct platform_driver {
	int (*probe)(struct platform_device *); // 与设备匹配之后会执行，必须要实现
	int (*remove)(struct platform_device *); // 移除设备时会执行
	void (*shutdown)(struct platform_device *); // 关闭设备时调用
	int (*suspend)(struct platform_device *, pm_message_t state); // 进入睡眠时调用
	int (*resume)(struct platform_device *); // 从睡眠模式恢复时调用
	struct device_driver driver;	// 设备共同属性，里面也有名字，函数指针，匹配优先级比id_table低
	const struct platform_device_id *id_table; // 优先匹配id_table名字
	bool prevent_deferred_probe;
};

// 构成 id_table结构体数组 的结构体定义
struct platform_device_id {
	char name[PLATFORM_NAME_SIZE]; // 用于匹配的名字
	kernel_ulong_t driver_data;
};

struct device_driver {
	const char		*name; // 驱动的名字，用于与设备名字匹配
	struct bus_type	*bus; // 标识本驱动的设备属于哪一种总线

	struct module	*owner; // 模块 拥有者
	const char		*mod_name;	/* used for built-in modules */

	bool suppress_bind_attrs;	/* disables bind/unbind via sysfs */

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



## driver中的probe函数

```c
// 匹配成功会调用的probe函数
int (*probe)(struct platform_device *);

// 获取device的资源信息，返回的结构体可访问 start end 等字段
struct resource *platform_get_resource(
    struct platform_device *,	// 平台总线设备
    unsigned int,	// 匹配资源里面的 flags字段，表示资源类型
    unsigned int	// 
);

int platform_get_irq(
    struct platform_device *,
    unsigned int
);

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

### 设备树属性

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

### 描述 GPIO

```bash
/ {
    led:led@1{  # 别名:节点名
        compatible="led";  # 用于匹配驱动
        gpios=<&gpio0 RK_PB7 1> # cell的个数由gpio控制器(gpio0)指定，此处为2（引脚+默认电平）
    }
};
```

### pinctrl 语法

pinctrl 可以分为 client 和sevice两部分。

其中client具备固定格式，而service因平台（瑞芯微、恩智浦……）的不同而不同。

```shell
# 客户端例子1
{
	pinctrl-names = "default"; # 该属性表示设备状态，此处表示状态0为"default"
	pinctrl-0 = <&pinctr_i2c2>; # 该属性表示状态0对应的引脚在 节点pinctr_i2c2 中配置
}

# 客户端例子2
{
	pinctrl-names = "default", "wake up"; # 有两个状态
	pinctrl-0 = <&pinctrl_hog_1 &pinctrl_hog_2>; # 状态0("default") 由两个节点配置
	pinctrl-1 = <&pinctrl_hog_3>; # 状态1("wake up") 由节点pinctrl_hog_3配置
}
```

```shell
# 服务端例子 恩智浦
&iomuxc{
	pinctr_i2c2:i2c2grp{
		fsl,pins = <
			# 把 uart5 的两个引脚,复用成 i2c 的引脚，后面的数值表示电气属性，可查手册
			MX6UL_PAD_UART5_TX_DATA_I2C2_SCL 0X4001b8b0
			MX6UL_PAD_UART5_RX_DATA_I2C2_SDA 0X4001b8b0
		>;
	}
}

# 服务端例子 瑞芯微
&iomuxc{
	uart7m1_xfer:uart7m1-xfer{
		rockchip.pins = 
			# 把gpio3的 c5和c4引脚,设置成复用4功能，电气属性为上拉
			<3 RK_PC5 4 &pcfg_pull_up>,
			<3 RK_PC4 4 &pcfg_pull_up>;
	}
}

```







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

内核会把dtb转换成 device_node ，部分 device_node 会转换成 platform_device（平台总线模型的device部分）。 

相关内核函数： of_platform_default_populate_init

转换成 platform_device 的节点，要满足以下条件：

1. 节点含有有 compatible 属性。
2. 节点的 compatible 属性不能包含"arm,primecell" 任意一个值。（会转换成amba设备）
3. 父节点为根节点，或者父节点的 compatible 属性包含"simple-bus", "simple-mfd", "isa"其中之一。





## 设备树相关内核API

Linux 内核给我们提供了一系列的函数来获取设备树中的节点或者属性信息，这一系列的函数都有一个统一的前缀“of_”，所以在很多资料里面也被叫做 OF 函数。这些 OF 函数原型都定义在 include/linux/of.h 文件中。

```c
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

```



# 内核子系统

**Linux 内核针对 PIN 的配置推出了 pinctrl 子系统，对 GPIO 的配置推出了 gpio 子系统。**

pinctrl 子系统管理 200 个 IO 口的上拉下拉电阻，电流驱动能力，是硬件底层的存在。如果 pinctrl 将某个 pin  脚初始化成了普通 GPIO 而不是 IIC 或者 SPI，那么接下来我们就可以使用 gpio 子系统的 API 去操作 IO 口输出高低电平。

传统的配置 pin 的方式就是直接操作相应的寄存器，但是这种配置 方式比较繁琐、而且容易出问题(比如 pin 功能冲突)。pinctrl 子系统就是为了解决这个问题而引入的，pinctrl 子系统主要工作内容如下：

1. 获取设备树中 pin 信息。

2. 根据获取到的 pin 信息来设置 pin 的复用功能

3. 根据获取到的 pin 信息来设置 pin 的电气特性，比如上/下拉、速度、驱动能力等。

对于我们使用者来讲，只需要在设备树里面设置好某个 pin 的相关属性即可，其他的初始化工作均由 pinctrl 子系统来完成，pinctrl 子系统源码目录为 drivers/pinctrl。



注意：pinctrl 子系统也是一个标准的 platform 驱动，当设备和驱动匹配的时候，probe 函数会执行，只是 pinctrl 子系统采用的 arch_initcall 去声明，而不是 module_init（device_initcall），所以在系统起来的时候它会先加载。



## gpio API

1. of_find_compatible_node : 函数在设备树中根据 device_type 和 compatible 这两个属性查找指定的节点，此处是为了获取在设备树中设置的 GPIO 的节点句柄。如果其他地方有获得句柄，那么可以直接使用这个句柄。

2. of_get_named_gpio : 获取所设置的 gpio number。

3. gpio_request  : 请求这个 gpio 。如果其他地方请求了这个 gpio，还没有释放，那么我们会请求不到。

4. gpio_direction_input、gpio_direction_output : 请求到这个 gpio 以后，我们就可以对它进行操作，比如获取到它的值，设置它的值。

5. gpio_free : 使用完以后，释放这个 gpio。



## pinctrl API

在 Linux 中，加 devm_ 开头的函数，代表这个函数支持资源管理。一般情况下，我们写一个驱动程序，在程序开头都会申请资源，比如内存、中断号等，万一后面哪一步申请出错，我们要回滚到第一步，去释放已经申请的资源，这样很麻烦。后来 Linux 开发出了很多 devm_ 开头的函数，代表这个函数有支持资源管理的版本，不管哪一步出错，只要错误退出，就会自动释放所申请的资源。

1. devm_pinctrl_get：用于获取设备树中自己用 pinctrl 建立的节点的句柄；

2. pinctrl_lookup_state：用于选择其中一个 pinctrl 的状态，同一个 pinctrl 可以有很多状态。比如 GPIO50 ，**一开始初始化的时候是 I2C ，设备待机时候，我希望切换到普通 GPIO 模式，并且配置为下拉输入，省电**。这时候如果 pinctrl 节点有描述，我们就可以在代码中切换 pin 的功能，从 I2C 功能切换成普通 GPIO 功能；

3. pinctrl_select_stat：用于真正设置，在上一步获取到某个状态以后，这一步真正设置为这个状态。

对于 pinctrl 子系统的设备树配置，是遵守 **service 和 client 结构**。

client 端各个平台基本都是一样的，server 端每个平台都不一样，使用的字符串的配置也不一样。



# 热插拔

热插拔机制有devfs、udev、mdev，其中 mdev 是udev的简化版本，常用于嵌入式设备中。

mdev是基于uevent_helper机制，内核产生的uevent会调用uevent_helper所指的用户程序mdev来执行热插拔动作。



