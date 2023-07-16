#include <linux/init.h>
#include <linux/module.h>
#include <linux/timer.h>

static void function_test(struct timer_list *t);//定义function_test定时功能函数
DEFINE_TIMER(timer_test,function_test);//定义一个定时器
static void function_test(struct timer_list *t)
{
	printk("this is function test \n");
	mod_timer(&timer_test,jiffies_64 + msecs_to_jiffies(5000));//使用mod_timer函数将定时时间设置为五秒后
}	
static int __init timer_mod_init(void) //驱动入口函数
{
	timer_test.expires = jiffies_64 + msecs_to_jiffies(5000);//将定时时间设置为五秒后
	add_timer(&timer_test);//添加一个定时器
	return 0;
}

static void __exit timer_mod_exit(void) //驱动出口函数
{
	del_timer(&timer_test);//删除一个定时器
	printk("module exit \n");
}
module_init(timer_mod_init);
module_exit(timer_mod_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("topeet");
