#include <sys/select.h>

#include "sysselect_ocall.h"

int ocall_select (int __nfds,  fd_set *__readfds,
    fd_set *__writefds,
    fd_set *__exceptfds,
    struct timeval *__timeout)
{
    increase_ocall_count();
	return select(__nfds, __readfds, __writefds, __exceptfds, __timeout);
}