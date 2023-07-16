#include <stdio.h>
#include "timerlib.h"
int timer_close(int fd)
{
	int ret;
	ret = ioctl(fd,TIMER_CLOSE);
	if(ret < 0){
		printf("ioctl  close error \n");
		return -1;
	}
	return ret;
}
