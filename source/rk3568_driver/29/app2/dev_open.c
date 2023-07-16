#include <stdio.h>
#include "timerlib.h"
int dev_open()
{
	fd = open("/dev/test",O_RDWR,0777);
    if(fd < 0){
		printf("file open error \n");
	}
	return fd;
}
