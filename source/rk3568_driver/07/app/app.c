#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc,char *argv[])
{
    int fd;//定义int类型的文件描述符
    char buf[32];//定义读取缓冲区buf
    fd=open(argv[1],O_RDWR,0666);//调用open函数，打开输入的第一个参数文件，权限为可读可写
    if(fd<0){
        printf("open is error\n");
        return -1;
    }
    printf("open is ok\n");
	/*如果第二个参数为read，条件成立，调用read函数，对文件进行读取*/                                                                                                                                  
    if(!strcmp(argv[2], "read")){
        read(fd,buf,32);
        }
	/*如果第二个参数为write，条件成立，调用write函数，对文件进行写入*/  
    else if(!strcmp(argv[2], "write")){
        write(fd,"hello\n",6);
    }
    close(fd);//调用close函数，对取消文件描述符到文件的映射
    return 0;
}

