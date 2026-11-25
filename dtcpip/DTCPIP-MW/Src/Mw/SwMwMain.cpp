/*
 * swagner's DTCP-IP middleware main (DTCPIP-MW/Src/Mw/SwMwMain.cpp, from the error
 * strings' path and lines): the activation's and the download's locks, the communication
 * server, its dispatch loop (the manager's dispatcher), then the shutdown in reverse.
 * Each step remembers its own failure and prints its error with its source line; a failed
 * step skips the rest, and the program returns 1 (0 when the loop ran to its end).
 */

#include <stddef.h>
#include <scetypes.h>

extern "C" {
/* the middleware's trace (SwMwTrace.c's [SWMW] print) */
int swMwTrace(const char *format, ...);

/* the activation's lock (DTCPIP-MW/Src/Activate/dtcpip_activate.c) */
int _sceDtcpIpActivateInit(void);
int _sceDtcpIpActivateTerm(void);

/* the download's lock (DTCPIP-MW/Src/Mw/dtcpip_download.c) */
int _sceDtcpIpDownloadInit(void);
int _sceDtcpIpDownloadTerm(void);

/* the communication server (DTCPIP-MW/Src/Comm/vita/SwMwCommServer.cpp) */
struct SwMwCommServer;
int SwMwCommServerCreate(struct SwMwCommServer **server);
int SwMwCommServerRun(struct SwMwCommServer *server, void (*dispatch)(void *, void *, void *),
		      void *argument, int flags);
int SwMwCommServerTerm(struct SwMwCommServer *server);

/* the manager's dispatcher (DTCPIP-MW/Src/Mw/SwMwManagerDispatch.cpp) */
void SwMwManagerDispatch(void *, void *, void *);
}

#define SWMW_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwMain.cpp"

/* one step's failure: its error printed with its source line */
#define SWMW_TRACE_ERROR(result, line)						\
	swMwTrace("[SWMW] Error %d in %s:%d\n", result, SWMW_SOURCE, line)

extern "C" int swagnerMain(void)
{
	int result;
	struct SwMwCommServer *server;

	int activationFailed = 0;
	result = _sceDtcpIpActivateInit();
	if (result != 0) {
		SWMW_TRACE_ERROR(result, 26);
		activationFailed = 1;
	}
	if (activationFailed == 0) {
		int downloadFailed = 0;

		result = _sceDtcpIpDownloadInit();
		if (result != 0) {
			SWMW_TRACE_ERROR(result, 29);
			downloadFailed = 1;
		}
		if (downloadFailed == 0) {
			int createFailed = 0;

			result = SwMwCommServerCreate(&server);
			if (result != 0) {
				SWMW_TRACE_ERROR(result, 32);
				createFailed = 1;
			}
			if (createFailed == 0) {
				int runFailed = 0;

				result = SwMwCommServerRun(server, SwMwManagerDispatch,
							   NULL, 0);
				if (result != 0) {
					SWMW_TRACE_ERROR(result, 35);
					runFailed = 1;
				}
				if (runFailed == 0) {
					int termFailed = 0;

					result = SwMwCommServerTerm(server);
					if (result != 0) {
						SWMW_TRACE_ERROR(result, 38);
						termFailed = 1;
					}
					if (termFailed == 0) {
						int downloadTermFailed = 0;

						result = _sceDtcpIpDownloadTerm();
						if (result != 0) {
							SWMW_TRACE_ERROR(result, 41);
							downloadTermFailed = 1;
						}
						if (downloadTermFailed == 0) {
							int activationTermFailed = 0;

							result = _sceDtcpIpActivateTerm();
							if (result != 0) {
								SWMW_TRACE_ERROR(result, 44);
								activationTermFailed = 1;
							}
							if (activationTermFailed == 0)
								return 0;
						}
					}
				}
			}
		}
	}
	return 1;
}
