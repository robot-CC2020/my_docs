#include <stdio.h>
#include "timerlib.h"
int timer_set(int fd,int arg)
{
	int ret;
	ret = ioctl(fd,TIMER_SET,arg);
	if(ret < 0){
		printf("ioctl error \n");
		return -1;
	}
	return ret;
}
