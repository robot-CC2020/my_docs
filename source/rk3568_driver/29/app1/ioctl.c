#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define TIMER_OPEN _IO('L',0)
#define TIMER_CLOSE _IO('L',1)
#define TIMER_SET _IOW('L',2,int)

int main(int argc,char *argv[]){
	int fd;
	fd = open("/dev/test",O_RDWR,0777);
	if(fd < 0){
		printf("file open error \n");
	}
    ioctl(fd,TIMER_SET,1000);
	ioctl(fd,TIMER_OPEN);
	sleep(3);
    ioctl(fd,TIMER_SET,3000);
	sleep(7);
	ioctl(fd,TIMER_CLOSE);
	close(fd);
}
