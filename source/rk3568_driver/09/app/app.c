#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])  //主函数
{
    int fd;   //定义int类型的文件描述符
    char buf1[32] = {0}; //定义读取缓存区buf1
    char buf2[32] = "nihao"; //定义写入缓存区buf2
    fd = open("/dev/test", O_RDWR);  //打开字符设备驱动
    if (fd < 0)
    {
        perror("open error \n");
        return fd;
    }
    read(fd, buf1, sizeof(buf1));//从/dev/test文件读取数据
    printf("buf1 is %s \r\n", buf1); //打印读取的数据

    write(fd,buf2,sizeof(buf2));//向/dev/test文件写入数据
    close(fd);
    return 0;
}
