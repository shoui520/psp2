#include "tty.h"
#include "stdc++/atexit.h"
#include <iofilemgr/iofilemgr.h>
#include <processmgr/processmgr.h>

static TTY tty;

int ScePafStdc_913B81F5(const void *buf, SceSize nbyte)
{
	int result = tty.fd_stdout;

	if (result != -1)
		result = sceIoWrite(result, buf, nbyte);
	return result;
}

int ScePafStdc_7FA92616(void)
{
	signed char buf[1];
	int ch;

	if (tty.fd_stdin == -1)
		return -1;
	/* The original consumes the byte even when the read reports no data. */
	sceIoRead(tty.fd_stdin, buf, 1);
	ch = buf[0];
	if (ch == '\n')
		ch = '\r';
	return ch;
}

TTY *ScePafStdc_112FF129(TTY *self)
{
	self->fd_stdout = sceKernelGetStdout();
	self->fd_stdin = sceKernelGetStdin();
	self->fd_extratty = sceKernelGetExtraTty();
	return self;
}

TTY *ScePafStdc_DF1F6AB5(TTY *self)
{
	self->fd_stdout = sceKernelGetStdout();
	self->fd_stdin = sceKernelGetStdin();
	self->fd_extratty = sceKernelGetExtraTty();
	return self;
}

TTY *ScePafStdc_CAFCD794(TTY *self)
{
	self->fd_stdout = -1;
	self->fd_extratty = -1;
	self->fd_stdin = -1;
	return self;
}

TTY *ScePafStdc_A6AA1C1B(TTY *self)
{
	self->fd_stdout = -1;
	self->fd_extratty = -1;
	self->fd_stdin = -1;
	return self;
}

int TTYInitialize(void)
{
	extern void *__dso_handle;

	ScePafStdc_112FF129(&tty);
	return ScePafStdc_C95732BB(&tty,
			(void (*)(void *))ScePafStdc_CAFCD794, &__dso_handle);
}
