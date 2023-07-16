#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>

int main(int argc, char *argv[])  
{
    int fd;//要监视的文件描述符
    char buf1[32] = {0};   
    char buf2[32] = {0};
    struct pollfd  fds[1];
    int ret;
    fd = open("/dev/test", O_RDWR);  //打开/dev/test设备，阻塞式访问
    if (fd < 0)
    {
        perror("open error \n");
        return fd;
    }

    fds[0] .fd =fd;
    fds[0].events = POLLIN;
    printf("read before \n");
    while (1)
    {
        ret = poll(fds,1,3000);
    if(!ret){
        printf("time out !!\n");

    }else if(fds[0].revents == POLLIN)
    {
        read(fd,buf1,sizeof(buf1));  //从/dev/test文件读取数据
         printf("buf is %s \n",buf1);
         sleep(1);
    }
         
    }
    
    printf("read after\n");
    close(fd);     //关闭文件
    return 0;
}