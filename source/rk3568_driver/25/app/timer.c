#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc,char *argv[]){
	int fd;//定义int类型的文件描述符fd
	int count;//定义int类型记录秒数的变量count
	fd = open("/dev/test",O_RDWR);//使用open()函数以可读可写的方式打开设备文件
	while(1)
	{
		read(fd,&count,sizeof(count));//使用read函数读取内核传递来的秒数
		printf("num is %d\n",count);
		sleep(1);
	}
	return 0;
}
