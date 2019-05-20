#include <sys/ioctl.h>

#include "proxy/sgx_sysioctl_u.h"

#include "sysioctl_ocall.h"

int ocall_ioctl(int fd, unsigned long request, void* arguments)
{
    increase_ocall_count();
	if (arguments!=NULL)
	{
		return ioctl(fd, request);
	} else {
		return ioctl(fd, request, arguments);
	}
}