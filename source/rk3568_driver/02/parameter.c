#include <linux/init.h>
#include <linux/module.h>
static int number;//定义int类型变量number
static char *name;//定义char类型变量name
static int para[8];//定义int类型的数组
static char str1[10];//定义char类型字符串str1
static int n_para;//定义int类型的用来记录module_param_array函数传递数组元素个数的变量n_para
module_param(number, int, S_IRUGO);//传递int类型的参数number，S_IRUGO表示权限为可读
module_param(name, charp, S_IRUGO);//传递char类型变量name
module_param_array(para , int , &n_para , S_IRUGO);//传递int类型的数组变量para
module_param_string(str, str1 ,sizeof(str1), S_IRUGO);//传递字符串类型的变量str1
static int __init parameter_init(void)//驱动入口函数
{
    static int i;
    printk(KERN_EMERG "%d\n",number);
    printk(KERN_EMERG "%s\n",name);                                                                                                                                                          
    for(i = 0; i < n_para; i++)
    {
        printk(KERN_EMERG "para[%d] : %d \n", i, para[i]);
    }
    printk(KERN_EMERG "%s\n",str1);
    return 0;
}
static void __exit parameter_exit(void)//驱动出口函数
{
    printk(KERN_EMERG "parameter_exit\n");
}
module_init(parameter_init);//注册入口函数
module_exit(parameter_exit);//注册出口函数
MODULE_LICENSE("GPL v2");//同意GPL开源协议
MODULE_AUTHOR("topeet"); //作者信息
