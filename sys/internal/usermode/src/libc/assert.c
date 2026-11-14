/* _Assert and _SCE_Assert functions */
#include <assert.h>
#include <kernel/processmgr.h>
#include <stdlib.h>

#include "libc_internal.h"
#include "xstdio.h"

/* print the assertion's message and the function it failed in */
static __attribute__((noinline)) void putmsg(const char *mesg, const char *fun)
{
	fputs(mesg, stderr);
	if (fun != NULL) {
		fputs(" in function ", stderr);
		fputs(fun, stderr);
	}
	fputs("\n", stderr);
	fflush(NULL);
}

void _SCE_Assert(const char *mesg, const char *fun)
{
	sceKernelCallAbortHandler(SCE_LIBC_ASSERT_CODE, NULL);
	putmsg(mesg, fun);
	__breakpoint(0x40);
}

void _Assert(const char *mesg, const char *fun)
{
	sceKernelCallAbortHandler(SCE_LIBC_ASSERT_CODE, NULL);
	putmsg(mesg, fun);
	abort();
}
