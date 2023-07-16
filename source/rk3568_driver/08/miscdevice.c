#include <linux/init.h>              //初始化头文件
#include <linux/module.h>            //最基本的文件，支持动态添加和卸载模块。
#include <linux/miscdevice.h>        //注册杂项设备头文件
#include <linux/fs.h>                //注册设备节点的文件结构体

struct file_operations misc_fops = { //文件操作集
    .owner = THIS_MODULE ////将owner字段指向本模块，可以避免在模块的操作正在被使用时卸载该模
};
struct miscdevice misc_dev = {       //杂项设备结构体
    
    .minor = MISC_DYNAMIC_MINOR,     //动态申请的次设备号
    .name = "test",                  //杂项设备名字是hello_misc
    .fops = &misc_fops,              //文件操作集

};
static int __init misc_init(void)           
{ 
    int ret;
    ret = misc_register(&misc_dev); //在初始化函数中注册杂项设备
    if (ret < 0)
    {
        printk("misc registe is error \n"); //打印注册杂项设备失败
    }
    printk("misc registe is succeed \n");//打印注册杂项设备成功
    return 0;
}
static void __exit misc_exit(void)
{ 

    misc_deregister(&misc_dev);     //在卸载函数中注销杂项设备
    printk(" misc goodbye! \n");
}
module_init(misc_init);
module_exit(misc_exit);
MODULE_LICENSE("GPL");
