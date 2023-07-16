#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#define TIMER_OPEN _IO('L',0)
#define TIMER_CLOSE _IO('L',1)
#define TIMER_SET _IOW('L',2,int)

struct device_test{

    dev_t dev_num;  //设备号
    int major ;  //主设备号
    int minor ;  //次设备号
    struct cdev cdev_test; // cdev
    struct class *class;   //类
    struct device *device; //设备
	int counter; 
};
static struct device_test dev1;
static void fnction_test(struct timer_list *t);//定义function_test定时功能函数
DEFINE_TIMER(timer_test,fnction_test);//定义一个定时器
void fnction_test(struct timer_list *t)
{
    printk("this is fnction_test\n");
    mod_timer(&timer_test,jiffies_64 + msecs_to_jiffies(dev1.counter));//使用mod_timer函数重新设置定时时间
}
static int cdev_test_open(struct inode *inode, struct file *file)
{
    file->private_data=&dev1;//设置私有数据
    return 0;
}

static int cdev_test_release(struct inode *inode, struct file *file)
{
    file->private_data=&dev1;//设置私有数据

    return 0;
}

static long cdev_test_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct device_test *test_dev = (struct device_test *)file->private_data;//设置私有数据
	switch(cmd){
        case TIMER_OPEN:
			add_timer(&timer_test);//添加一个定时器
            break;
        case TIMER_CLOSE:
			del_timer(&timer_test);//删除一个定时器
            break;
        case TIMER_SET:
			test_dev->counter = arg;
			timer_test.expires = jiffies_64 + msecs_to_jiffies(test_dev->counter);//设置定时时间
            break;

	default:
			break;
	}
	return 0;
}
/*设备操作函数*/
struct file_operations cdev_test_fops = {
    .owner = THIS_MODULE, //将owner字段指向本模块，可以避免在模块的操作正在被使用时卸载该模块
	.open = cdev_test_open,
	.release = cdev_test_release,
	.unlocked_ioctl = cdev_test_ioctl,
};
static int __init timer_dev_init(void) //驱动入口函数
{
    /*注册字符设备驱动*/
    int ret;
    /*1 创建设备号*/
    ret = alloc_chrdev_region(&dev1.dev_num, 0, 1, "alloc_name"); //动态分配设备号
    if (ret < 0)
    {
       goto err_chrdev;
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
   ret =  cdev_add(&dev1.cdev_test, dev1.dev_num, 1);
    if(ret<0)
    {
        goto  err_chr_add;
    }
    /*4 创建类*/
 dev1. class = class_create(THIS_MODULE, "test");
if(IS_ERR(dev1.class))
{
        ret=PTR_ERR(dev1.class);
        goto err_class_create;
    }
    /*5  创建设备*/
  	dev1.device = device_create(dev1.class, NULL, dev1.dev_num, NULL, "test");
    if(IS_ERR(dev1.device))
    {
        ret=PTR_ERR(dev1.device);
        goto err_device_create;
    }

return 0;

err_device_create:
        class_destroy(dev1.class);                 //删除类

err_class_create:
       cdev_del(&dev1.cdev_test);                 //删除cdev

err_chr_add:
        unregister_chrdev_region(dev1.dev_num, 1); //注销设备号

err_chrdev:
        return ret;
}

static void __exit timer_dev_exit(void) //驱动出口函数
{
    /*注销字符设备*/
    unregister_chrdev_region(dev1.dev_num, 1); //注销设备号
    cdev_del(&dev1.cdev_test);                 //删除cdev
    device_destroy(dev1.class, dev1.dev_num);       //删除设备
    class_destroy(dev1.class);                 //删除类
}
module_init(timer_dev_init);
module_exit(timer_dev_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("topeet");
