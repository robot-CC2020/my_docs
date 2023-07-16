#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/errno.h>
static atomic_t v = ATOMIC_INIT(1) ;
static int open_test(struct inode *inode,struct file *file)
{
	if(atomic_read(&v) != 1){
		return -EBUSY;
	}
	atomic_set(&v,0);
	//printk("\nthis is open_test \n");
	return 0;
}

static ssize_t read_test(struct file *file,char __user *ubuf,size_t len,loff_t *off)
{
	int ret;
	char kbuf[10] = "topeet";
	printk("\nthis is read_test \n");
	ret = copy_to_user(ubuf,kbuf,strlen(kbuf));
	if (ret != 0){
		printk("copy_to_user is error \n");
	}
	printk("copy_to_user is ok \n");
	return 0;
}
char kbuf[10] = {0};
static ssize_t write_test(struct file *file,const char __user *ubuf,size_t len,loff_t *off)
{
	int ret;
	ret = copy_from_user(kbuf,ubuf,len);
	if (ret != 0){
		printk("copy_from_user is error\n");
	}
	if(strcmp(kbuf,"topeet") == 0 ){
		ssleep(4);
	}
	else if(strcmp(kbuf,"itop") == 0 ){
		ssleep(2);
	}
	printk("copy_from_user buf is %s \n",kbuf);
	return 0;
}
static int release_test(struct inode *inode,struct file *file)
{
	//printk("\nthis is release_test \n");
	atomic_set(&v,1);
	return 0;
}


struct chrdev_test {
       dev_t dev_num;
       int major,minor;
       struct cdev cdev_test;
       struct class *class_test;
};
struct chrdev_test dev1;
struct file_operations fops_test = {
      .owner = THIS_MODULE,
      .open = open_test,
      .read = read_test,
      .write = write_test,
      .release = release_test,
};
 

static int __init atomic_init(void)
{
	if(alloc_chrdev_region(&dev1.dev_num,0,1,"chrdev_name") < 0 ){
		printk("alloc_chrdev_region is error \n");
	}
	printk("alloc_chrdev_region is ok \n");
	dev1.major = MAJOR(dev1.dev_num);
	dev1.minor = MINOR(dev1.dev_num);
	printk("major is %d,minor is %d\n",dev1.major,dev1.minor);
	cdev_init(&dev1.cdev_test,&fops_test);
	cdev_add(&dev1.cdev_test,dev1.dev_num,1);
	dev1.class_test = class_create(THIS_MODULE,"class_test");
	device_create(dev1.class_test,0,dev1.dev_num,0,"device_test");
	return 0;
}

static void __exit atomic_exit(void)
{
	device_destroy(dev1.class_test,dev1.dev_num);
	class_destroy(dev1.class_test);
	cdev_del(&dev1.cdev_test);
	unregister_chrdev_region(dev1.dev_num,1);
	printk("module exit \n");
}
module_init(atomic_init);
module_exit(atomic_exit)
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("topeet");
