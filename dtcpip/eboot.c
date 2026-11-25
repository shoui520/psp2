/*
 * swagner (vs0:app/NPXS10080/eboot.bin): the DTCP-IP middleware application's entry. The
 * SDK's start code comes from the project's shared crt0 (3.65 variant); this file holds
 * the application's own main and the module's entry.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>

void _start(SceSize args, char *argp) __attribute__((noreturn));

/* the middleware's own main (the DTCPIP-MW's SwMwMain) */
int swagnerMain(void);

int main(int argc, char *argv[]);

int module_start(SceSize arglen, const void *argp)
{
	_start(arglen, (char *)argp);
	return 0;
}
