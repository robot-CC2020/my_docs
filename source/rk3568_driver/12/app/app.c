
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) //主函数
{
    int fd;
    char buf1[32] = "nihao";  //定义写入缓存区buf1
    fd = open("/dev/test", O_RDWR); //打开/dev/test设备
    if (fd < 0)
    {
        perror("open error \n");
        return fd;
    }
    write(fd,buf1,sizeof(buf1)); //向/dev/test设备写入数据
    close(fd);
    return 0;
}