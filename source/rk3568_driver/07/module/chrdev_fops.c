#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>

static int chrdev_open(struct inode *inode, struct file *file)
{
	printk("This is chrdev_open \n");
	return 0;
}

static ssize_t chrdev_read(struct file *file,char __user *buf, size_t size, loff_t *off)
{
	printk("This is chrdev_read \n");
	return 0;
}

static ssize_t chrdev_write(struct file *file,const char __user *buf,size_t size,loff_t *off)
{
	printk("This is chrdev_write \n");
	return 0;
}
static int chrdev_release(struct inode *inode, struct file *file)
{
	return 0;
}
static dev_t dev_num;//定义dev_t类型变量dev_num来表示设备号
static struct cdev cdev_test;//定义struct cdev 类型结构体变量cdev_test，表示要注册的字符设备
static struct file_operations cdev_fops_test = {
    .owner = THIS_MODULE,//将owner字段指向本模块，可以避免在模块的操作正在被使用时卸载该模块
	.open = chrdev_open,
	.read = chrdev_read,
	.write = chrdev_write,
	.release = chrdev_release,
};//定义file_operations结构体类型的变量cdev_test_ops
static struct class *class_test;//定于struct class *类型结构体变量class_test，表示要创建的类

static int __init chrdev_fops_init(void)//驱动入口函数
{
	int ret;//定义int类型的变量ret，用来对函数返回值进行判断
    int major,minor;//定义int类型的主设备号major和次设备号minor
	ret = alloc_chrdev_region(&dev_num,0,1,"chrdev_name");//自动获取设备号，设备名chrdev_name
    if (ret  < 0){
        printk("alloc_chrdev_region is error \n");
    }
    printk("alloc_chrdev_region is ok \n");
    major = MAJOR(dev_num);//使用MAJOR()函数获取主设备号
    minor = MINOR(dev_num);//使用MINOR()函数获取次设备号
    printk("major is %d\n",major);
	printk("minor is %d\n",minor);
    cdev_init(&cdev_test,&cdev_fops_test);//使用cdev_init()函数初始化cdev_test结构体，并链接到cdev_test_ops结构体
	cdev_test.owner = THIS_MODULE;//将owner字段指向本模块，可以避免在模块的操作正在被使用时卸载该模块
	ret = cdev_add(&cdev_test,dev_num,1); //使用cdev_add()函数进行字符设备的添加
    if (ret < 0){
         printk("cdev_add is error \n");
    }
    printk("cdev_add is ok \n");                                                                                
    class_test  = class_create(THIS_MODULE,"class_test");//使用class_create进行类的创建，类名称为class_test
    device_create(class_test,NULL,dev_num,NULL,"device_test");//使用device_create进行设备的创建，设备名称为device_test
    return 0;
}

static void __exit chrdev_fops_exit(void)//驱动出口函数
{
	device_destroy(class_test,dev_num);//删除创建的设备
    class_destroy(class_test);//删除创建的类
    cdev_del(&cdev_test);//删除添加的字符设备cdev_test
	unregister_chrdev_region(dev_num,1);//释放字符设备所申请的设备号
    printk("module exit \n");
}

module_init(chrdev_fops_init);//注册入口函数
module_exit(chrdev_fops_exit);//注册出口函数
MODULE_LICENSE("GPL v2");//同意GPL开源协议
MODULE_AUTHOR("topeet");//作者信息

