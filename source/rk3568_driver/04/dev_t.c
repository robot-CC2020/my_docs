#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
static int major;//定义静态加载方式时的主设备号参数major
static int minor;//定义静态加载方式时的次设备号参数minor
module_param(major,int,S_IRUGO);//通过驱动模块传参的方式传递主设备号参数major
module_param(minor,int,S_IRUGO);//通过驱动模块传参的方式传递次设备号参数minor
static dev_t dev_num;//定义dev_t类型(32位大小)的变量dev_num

static int __init dev_t_init(void)//驱动入口函数
{
	int ret;//定义int类型的变量ret，用来判断函数返回值
	/*以主设备号进行条件判断，即如果通过驱动传入了major参数则条件成立，进入以下分支*/
	if(major){
    	dev_num = MKDEV(major,minor);//通过MKDEV函数将驱动传参的主设备号和次设备号转换成dev_t类型的设备号
    	printk("major is %d\n",major);
    	printk("minor is %d\n",minor);
    	ret = register_chrdev_region(dev_num,1,"chrdev_name");//通过静态方式进行设备号注册
        if(ret < 0){
            printk("register_chrdev_region is error\n");
        }
        printk("register_chrdev_region is ok\n");
    }
	/*如果没有通过驱动传入major参数，则条件成立，进入以下分支*/
    else{
        ret = alloc_chrdev_region(&dev_num,0,1,"chrdev_num");//通过动态方式进行设备号注册
        if(ret < 0){
            printk("alloc_chrdev_region is error\n");
        }                                                                                                                                              
        printk("alloc_chrdev_region is ok\n");
        major=MAJOR(dev_num);//通过MAJOR()函数进行主设备号获取
        minor=MINOR(dev_num);//通过MINOR()函数进行次设备号获取
        printk("major is %d\n",major);
        printk("minor is %d\n",minor);
    }
    return 0;
}

static void __exit dev_t_exit(void)//驱动出口函数
{
    unregister_chrdev_region(dev_num,1);//注销字符驱动
    printk("module exit \n");
}

module_init(dev_t_init);//注册入口函数
module_exit(dev_t_exit);//注册出口函数
MODULE_LICENSE("GPL v2");//同意GPL开源协议
MODULE_AUTHOR("topeet");  //作者信息
