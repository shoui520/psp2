/* SSLeay socket BIO with the measured SSL-C changes in bss_sock_sce.patch. */
#include "bio.h"
#include "bio_dispatch.h"
#include "../memory.h"
#include <net/socket.h>
#include <net/errno.h>

#define clear_socket_error() (*sceNetErrnoLoc() = 0)
#define readsocket(fd, data, size) sceNetRecv(fd, data, size, 0)
#define writesocket(fd, data, size) sceNetSend(fd, data, size, 0)
#define shutdown(fd, how) sceNetShutdown(fd, how)
#define closesocket(fd) sceNetSocketClose(fd)
#define strlen sslStringLength

#include "bss_sock_sce.inc"

long BIO_set_fd(BIO *bio, int fd, long closeFlag)
{
	return sslBioCtrlInline(bio, BIO_C_SET_FD, closeFlag, (char *)&fd);
}

int BIO_get_fd(BIO *bio, int *fd)
{
	int local;

	if (fd == NULL)
		fd = &local;
	sslBioCtrlInline(bio, BIO_C_GET_FD, 0, (char *)fd);
	return *fd;
}
