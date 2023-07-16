#include <stdio.h>
#include "timerlib.h"
int main(int argc,char *argv[]){
	int fd;
	fd = dev_open();
    timer_set(fd,1000);
	timer_open(fd);
	sleep(3);
	timer_set(fd,3000);
	sleep(7);
	timer_close(fd);
	close(fd);
}
