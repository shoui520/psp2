/*
 * swagner's communication server (DTCPIP-MW/Src/Comm/vita/SwMwCommServer.cpp, from the
 * error strings' path and lines): the middleware application's side of the SW IPC the
 * library connects to. It owns the control mutex, the two request pipes, the shared
 * memory block (two 16520-byte channel slices) and two channels, and it runs the loop that
 * reads the library's requests and hands them to the manager.
 */

#include <stddef.h>
#include <scetypes.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/threadmgr.h>
#include <kernel/sysmem/memblock.h>
#include <appmgr.h>
#include <libnetctl.h>
#include <net.h>
#include <sysmodule/sysmodule.h>
#include <libkernel/libkernel.h>

#define SWMW_COMM_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Comm\\vita\\SwMwCommServer.cpp"

/* the middleware's trace (SwMwTrace.c's [SWMW] print, flushes every message) */
extern "C" int swMwTrace(const char *format, ...);

/* the server's shared memory: two channel slices of 16520 bytes */
#define SWMW_COMM_SHARED_SIZE		33040
#define SWMW_COMM_CHANNEL_STRIDE		16520
#define SWMW_COMM_CHANNEL_COUNT		2

/* the middleware's identity, from the launch parameter (main.cpp) */
extern char g_swagnerIdentity[16];

/* a request's payload: the SW core's 256-byte block after the request's header */
struct SwMwCommRequest {
	int reserved;			/* 0x00 */
	int value[3];			/* 0x04: the request's header words */
	unsigned char payload[256];	/* 0x10 */
};

/* the network setup the server makes before its pipes: SceNet's 64 KiB work memory */
#define SWMW_NET_MEMORY_SIZE	0x10000
#define SWMW_NET_ERROR_MEMORY	22

extern "C" int SwMwNetInit(void)
{
	SceNetInitParam param;
	int result;

	if (sceSysmoduleLoadModule(SCE_SYSMODULE_NET) < 0)
		return SWMW_NET_ERROR_MEMORY;
	param.memory = malloc(SWMW_NET_MEMORY_SIZE);
	param.size = SWMW_NET_MEMORY_SIZE;
	param.flags = 0;
	if (param.memory == NULL) {
		result = SWMW_NET_ERROR_MEMORY;
	} else {
		result = sceNetInit(&param);
		if (result >= 0) {
			result = sceNetCtlInit();
			if (result >= 0)
				return 0;
		}
	}
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
	return result;
}

extern "C" int SwMwNetTerm(void)
{
	sceNetCtlTerm();
	sceNetTerm();
	return sceSysmoduleUnloadModule(SCE_SYSMODULE_NET);
}

/*
 * hand the SW core a request's header and payload: its three header words, then the
 * 256-byte payload, in the core's own block (0x81000c38; called from the core's dispatch).
 */
extern "C" int SwMwCommServerGetRequest(const struct SwMwCommRequest *request, int *out)
{
	out[0] = request->value[0];
	out[1] = request->value[1];
	out[2] = request->value[2];
	memcpy(out + 3, request->payload, sizeof(request->payload));
	return 0;
}

/* store the 64-byte value a SW core reply carries (after its 12-byte header) in a
 * request's payload (0x81000c78) */
extern "C" int SwMwCommServerSetRequestValue(struct SwMwCommRequest *request, const void *value)
{
	memcpy(request->payload, (const char *)value + 12, 64);
	return 0;
}

/*
 * poll the application manager's events (0x81000ca0): the run loop calls it while a
 * receive times out. A quit request for this application's own identity ends the loop.
 */
extern "C" int SwMwCommServerCheckAppEvent(void)
{
	struct SceAppMgrAppEvent event;
	int count = 0;
	int quit = 0;
	int i;

	sceAppMgrReceiveEventNum(&count);
	for (i = 0; i < count; ++i) {
		sceAppMgrReceiveEvent(&event);
		swMwTrace("sw mw:Event[%d]: EventID = 0x%08x, AppID = 0x%08x\n", i, event.eventId,
			  event.appId);
		if (event.eventId == 0x20000001) {
			if (g_swagnerIdentity[0] != 0) {
				swMwTrace("sw mw:kill name %s\n", g_swagnerIdentity);
				sceAppMgrDestroyAppByName(g_swagnerIdentity);
			}
			quit = 1;
		}
	}
	return quit;
}
