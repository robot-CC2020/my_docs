#include <stdio.h>
#include "timerlib.h"
int timer_open(int fd)
{
	int ret;
	ret = ioctl(fd,TIMER_OPEN);
	if(ret < 0){
		printf("ioctl open error \n");
		return -1;
	}
	return ret;
}
