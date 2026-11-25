/*
 * swagner's DTCP-IP middleware main (DTCPIP-MW/Src/Mw/SwMwMain.cpp, from the error
 * strings' path and lines): the activation's and the download's locks, the communication
 * server, its dispatch loop (the manager's dispatcher), then the shutdown in reverse.
 * Any failed step prints its error and makes the program return 1; the dispatch loop's
 * return is the normal exit.
 */

#include <stddef.h>
#include <scetypes.h>

extern "C" {
/* the middleware's trace (the [SWMW] messages) */
int swMwTrace(const char *format, ...);

/* the activation's lock (DTCPIP-MW/Src/Activate/dtcpip_activate.c) */
int _sceDtcpIpActivateInit(void);
int _sceDtcpIpActivateTerm(void);

/* the download's lock (DTCPIP-MW/Src/Mw/dtcpip_download.c) */
int _sceDtcpIpDownloadInit(void);
int _sceDtcpIpDownloadTerm(void);

/* the communication server (DTCPIP-MW/Src/Comm/vita/SwMwCommServer.cpp) */
int SwMwCommServerCreate(int *server);
int SwMwCommServerRun(int server, int (*dispatch)(int, void *), int argument, int flags);
int SwMwCommServerTerm(int server);

/* the manager's dispatcher (DTCPIP-MW/Src/Mw/SwMwManagerDispatch.cpp) */
int SwMwManagerDispatch(int, void *);
}

#define SWMW_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwMain.cpp"

/* one checked step: its error printed with its line, and the failure remembered */
#define SWMW_STEP(call, line)						\
	do {								\
		int swmwResult = (call);				\
		int swmwFailed = 0;					\
									\
		if (swmwResult != 0) {					\
			swMwTrace("[SWMW] Error %d in %s:%d\n", swmwResult,	\
				  SWMW_SOURCE, line);			\
			swmwFailed = 1;					\
		}							\
		if (swmwFailed != 0)					\
			goto failed;					\
	} while (0)

extern "C" int swagnerMain(void)
{
	int server;

	SWMW_STEP(_sceDtcpIpActivateInit(), 26);
	SWMW_STEP(_sceDtcpIpDownloadInit(), 29);
	SWMW_STEP(SwMwCommServerCreate(&server), 32);
	SWMW_STEP(SwMwCommServerRun(server, SwMwManagerDispatch, 0, 0), 35);
	SWMW_STEP(SwMwCommServerTerm(server), 38);
	SWMW_STEP(_sceDtcpIpDownloadTerm(), 41);
	SWMW_STEP(_sceDtcpIpActivateTerm(), 44);
	return 0;
failed:
	return 1;
}
