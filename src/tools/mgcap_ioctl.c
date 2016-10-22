#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define IOCTL_DEBUG_ENABLE    10
#define IOCTL_DEBUG_DISABLE   11

int main(void) {
	int fd;

	fd = open("/dev/mgcap/lo", O_RDWR);
	if(fd < 0) {
		perror("open");
		return 1;
	}

	ioctl(fd, IOCTL_DEBUG_ENABLE);

	close(fd);
}

