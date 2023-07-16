#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int fd1;  //定义设备1的文件描述符
    int fd2;  //定义设备2的文件描述符
    char buf1[32] = "nihao /dev/test1";   //定义写入缓存区buf1
    char buf2[32] = "nihao /dev/test2";   //定义写入缓存区buf2
    fd1 = open("/dev/test1", O_RDWR);  //打开设备1：test1
    if (fd1 < 0)
    {
        perror("open error \n");
        return fd1;
    }
    write(fd1,buf1,sizeof(buf1));  //向设备1写入数据
    close(fd1); //取消文件描述符到文件的映射

    fd2= open("/dev/test2", O_RDWR); //打开设备2：test2
    if (fd2 < 0)
    {
        perror("open error \n");
        return fd2;
    }
    write(fd2,buf2,sizeof(buf2));  //向设备2写入数据
    close(fd2);   //取消文件描述符到文件的映射

    return 0;
}