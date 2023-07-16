#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

static dev_t dev_num;  //设备号
static int major = 0;        //主设备号
static int minor = 0;        //次设备号
struct cdev cdev_test;   // cdev

struct class *class;          //类
struct device *device;    //设备

/*打开设备函数*/
static int cdev_test_open(struct inode *inode, struct file *file)
{
    printk("This is cdev_test_open\r\n");
    return 0;
}

/*向设备写入数据函数*/
static ssize_t cdev_test_write(struct file *file, const char __user *buf, size_t size, loff_t *off)
{
    /*本章实验重点******/
    char kbuf[32] = {0};   //定义写入缓存区kbuf
    if (copy_from_user(kbuf, buf, size) != 0) // copy_from_user:用户空间向内核空间传数据
    {
        printk("copy_from_user error\r\n");//打印copy_from_user函数执行失败
        return -1;
    }
    printk("This is cdev_test_write\r\n");

    printk("kbuf is %s\r\n", kbuf);
    return 0;
}

/**从设备读取数据*/
static ssize_t cdev_test_read(struct file *file, char __user *buf, size_t size, loff_t *off)
{
    /*本章实验重点******/
    char kbuf[32] = "This is cdev_test_read!";//定义内核空间数据
    if (copy_to_user(buf, kbuf, strlen(kbuf)) != 0) // copy_to_user:内核空间向用户空间传数据
    {
        printk("copy_to_user error\r\n"); //打印copy_to_user函数执行失败
        return -1;
    }

    printk("This is cdev_test_read\r\n");
    return 0;
}

static int cdev_test_release(struct inode *inode, struct file *file)
{
    printk("This is cdev_test_release\r\n");
    return 0;
}

/*设备操作函数，定义file_operations结构体类型的变量cdev_test_fops*/
struct file_operations cdev_test_fops = {
    .owner = THIS_MODULE, //将owner字段指向本模块，可以避免在模块的操作正在被使用时卸载该模块
    .open = cdev_test_open, //将open字段指向chrdev_open(...)函数
    .read = cdev_test_read,  //将open字段指向chrdev_read(...)函数
    .write = cdev_test_write, //将open字段指向chrdev_write(...)函数
    .release = cdev_test_release, //将open字段指向chrdev_release(...)函数
};

static int __init chr_fops_init(void) //驱动入口函数
{
    /*注册字符设备驱动*/
    int ret;
    /*1 创建设备号*/
    ret = alloc_chrdev_region(&dev_num, 0, 1, "alloc_name"); //动态分配设备号
    if (ret < 0)
    {
        printk("alloc_chrdev_region is error\n");//打印动态分配设备号失败
    }
    printk("alloc_chrdev_region is ok\n");

    major = MAJOR(dev_num); //获取主设备号
    minor = MINOR(dev_num); //获取次设备号

    printk("major is %d \r\n", major); //打印主设备号
    printk("minor is %d \r\n", minor); //打印次设备号
     /*2 初始化cdev*/
    cdev_test.owner = THIS_MODULE;
    cdev_init(&cdev_test, &cdev_test_fops);

    /*3 添加一个cdev,完成字符设备注册到内核*/
    cdev_add(&cdev_test, dev_num, 1);

    /*4 创建类*/
    class = class_create(THIS_MODULE, "test");

    /*5  创建设备*/
    device = device_create(class, NULL, dev_num, NULL, "test");

    return 0;
}

static void __exit chr_fops_exit(void) //驱动出口函数
{
    /*注销字符设备*/
    unregister_chrdev_region(dev_num, 1); //注销设备号
    cdev_del(&cdev_test);                                     //删除cdev
    device_destroy(class, dev_num);               //删除设备
    class_destroy(class);                                        //删除类
}
module_init(chr_fops_init);   //注册入口函数
module_exit(chr_fops_exit);  //注册出口函数
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("topeet");
