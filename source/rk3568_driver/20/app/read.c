#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[])  
{
    int fd;
    char buf1[32] = {0};   
    char buf2[32] = {0};
    fd = open("/dev/test", O_RDWR);  //打开led驱动
    if (fd < 0)
    {
        perror("open error \n");
        return fd;
    }
    printf("read before \n");
    read(fd,buf1,sizeof(buf1));  //从/dev/test文件读取数据
    printf("buf is %s  \n",buf1);
    printf("read after \n");
    close(fd);     //关闭文件
    return 0;
}