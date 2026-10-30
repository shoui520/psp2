/* paf::TTY: the process's standard streams, opened once at load. */
#include "tty.h"
#include <iofilemgr/iofilemgr.h>
#include <processmgr/processmgr.h>

namespace paf {

TTY::TTY()
{
	m_stdout = sceKernelGetStdout();
	m_stdin = sceKernelGetStdin();
	m_extra_tty = sceKernelGetExtraTty();
}

TTY::~TTY()
{
	m_stdout = -1;
	m_extra_tty = -1;
	m_stdin = -1;
}

}

static paf::TTY s_tty;

int write_to_stdout(const void *buf, SceSize nbyte)
{
	int result = s_tty.m_stdout;

	if (result != -1)
		result = sceIoWrite(result, buf, nbyte);
	return result;
}

int read_char_from_stdin(void)
{
	signed char buf[1];
	int ch;

	if (s_tty.m_stdin == -1)
		return -1;
	/* the byte is used even when the read reports no data */
	sceIoRead(s_tty.m_stdin, buf, 1);
	ch = buf[0];
	if (ch == '\n')
		ch = '\r';
	return ch;
}
