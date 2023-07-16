#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

struct device_test{
   
    dev_t dev_num;              //设备号
     int major ;                         //主设备号
    int minor ;                          //次设备号
    struct cdev cdev_test;  // cdev
    struct class *class;         //类
    struct device *device;   //设备
    char kbuf[32];                  //缓存区buf
};

struct  device_test dev1;  //定义一个device_test结构体变量


/*打开设备函数*/
static int cdev_test_open(struct inode *inode, struct file *file)
{
    
    file->private_data=&dev1;  //设置私有数据
    printk("This is cdev_test_open\r\n");

    return 0;
}

/*向设备写入数据函数*/
static ssize_t cdev_test_write(struct file *file, const char __user *buf, size_t size, loff_t *off)
{
     struct device_test *test_dev=(struct device_test *)file->private_data; //在write函数中读取private_data

    if (copy_from_user(test_dev->kbuf, buf, size) != 0) // copy_from_user:用户空间向内核空间传数据
    {
        printk("copy_from_user error\r\n");
        return -1;
    }
    printk("This is cdev_test_write\r\n");

    printk("kbuf is %s\r\n", test_dev->kbuf); //打印kbuf的值
    return 0;
}

/**从设备读取数据*/
static ssize_t cdev_test_read(struct file *file, char __user *buf, size_t size, loff_t *off)
{
    
    struct device_test *test_dev=(struct device_test *)file->private_data;
    
    if (copy_to_user(buf, test_dev->kbuf, strlen( test_dev->kbuf)) != 0) // copy_to_user:内核空间向用户空间传数据
    {
        printk("copy_to_user error\r\n");
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

/*设备操作函数*/
struct file_operations cdev_test_fops = {
    .owner = THIS_MODULE,         //将owner字段指向本模块，可以避免在模块的操作正在被使用时卸载该模块
    .open = cdev_test_open,         //将open字段指向chrdev_open(...)函数
    .read = cdev_test_read,            //将open字段指向chrdev_read(...)函数
    .write = cdev_test_write,         //将open字段指向chrdev_write(...)函数
    .release = cdev_test_release,//将open字段指向chrdev_release(...)函数
};

static int __init chr_fops_init(void) //驱动入口函数
{
    /*注册字符设备驱动*/
    int ret;
    /*1 创建设备号*/
    ret = alloc_chrdev_region(&dev1.dev_num, 0, 1, "alloc_name"); //动态分配设备号
    if (ret < 0)
    {
        printk("alloc_chrdev_region is error\n");
    }
    printk("alloc_chrdev_region is ok\n");

    dev1.major = MAJOR(dev1.dev_num); //获取主设备号
   dev1.minor = MINOR(dev1.dev_num); //获取次设备号

    printk("major is %d \r\n", dev1.major); //打印主设备号
    printk("minor is %d \r\n", dev1.minor); //打印次设备号
     /*2 初始化cdev*/
    dev1.cdev_test.owner = THIS_MODULE;
    cdev_init(&dev1.cdev_test, &cdev_test_fops);

    /*3 添加一个cdev,完成字符设备注册到内核*/
    cdev_add(&dev1.cdev_test, dev1.dev_num, 1);

    /*4 创建类*/
  dev1. class = class_create(THIS_MODULE, "test");

    /*创建设备*/
  dev1.device = device_create(dev1.class, NULL, dev1.dev_num, NULL, "test");

    return 0;
}

static void __exit chr_fops_exit(void) //驱动出口函数
{
    /*注销字符设备*/
    unregister_chrdev_region(dev1.dev_num, 1); //注销设备号
    cdev_del(&dev1.cdev_test);                 //删除cdev
    device_destroy(dev1.class, dev1.dev_num);       //删除设备
    class_destroy(dev1.class);                 //删除类
}
module_init(chr_fops_init);
module_exit(chr_fops_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("topeet");
