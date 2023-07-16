#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>

int fd;
char buf1[32] = {0};   


//SIGIO信号的信号处理函数
static void func(int signum)
{
    read(fd,buf1,32);
    printf ("buf is %s\n",buf1);
}
int main(int argc, char *argv[])  
{
    int ret;
    int flags;
       fd = open("/dev/test", O_RDWR);  //打开led驱动
    if (fd < 0)
    {
        perror("open error \n");
        return fd;
    }

    signal(SIGIO,func);  //步骤一：使用signal函数注册SIGIO信号的信号处理函数
     //步骤二：设置能接收这个信号的进程
     //fcntl函数用来操作文件描述符，
     //F_SETOWN 设置当前接收的SIGIO的进程ID
     fcntl(fd,F_SETOWN,getpid()); 

    flags = fcntl(fd,F_GETFD); //获取文件描述符标志
    //步骤三  开启信号驱动IO 使用fcntl函数的F_SETFL命令打开FASYNC标志
    fcntl(fd,F_SETFL,flags| FASYNC);
    while(1);
    
    close(fd);     //关闭文件
    return 0;
}