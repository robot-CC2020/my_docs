#include <linux/init.h>
#include <linux/module.h>
extern int num;//导入int类型变量num
extern int add(int a, int b);//导入函数add
static int __init hello_init(void)//驱动入口函数
{
    static int sum;
    printk("num = %d\n", num);//打印num值
    sum = add(3, 4);//使用add函数进行3+4的运算                                                                                                                                                                          
    printk("sum = %d\n", sum);//打印add函数的运算值
    return 0;
}

static void __exit hello_exit(void)//驱动出口函数
{
    printk("Goodbye hello module\n");
}

module_init(hello_init);//注册入口函数
module_exit(hello_exit);//注册出口函数

MODULE_LICENSE("GPL");//同意GPL开源协议
MODULE_AUTHOR("topeet");//作者信息
