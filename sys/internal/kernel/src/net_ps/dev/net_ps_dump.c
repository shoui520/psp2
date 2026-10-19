/* SceNetPs development packet capture; original filename is unavailable. */
#include <net_ps/netps.h>
#include "../netps_dump.h"
#include "../netps_resource.h"
#include <net_ps/netps_thread.h>
#include "../net_ps_internal.h"

extern int gNetPsPacketCaptureActive;

int sceNetPsDumpCreate(const char *name, int length, int flags)
{
	SceNetPsPcapHeader header;
	SceNetPsDump *dump;
	size_t copied;
	int dumpId;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (length < 2048 || flags != 0 || name == NULL) {
		result = -22;
	} else {
		dumpId = sceNetPsAllocateDumpId();
		if (dumpId < 0) {
			result = -24;
		} else {
			dump = sceNetPsMalloc(length + sizeof(*dump), 3, 8);
			if (dump != NULL) {
				sceNetPsCopyInString(name, dump->name, 31, &copied);
				sceNetPsSetDump(dumpId, dump);
				dump->length = length;
				dump->flags = flags;
				dump->buffer = (char *)(dump + 1);
				header.magic = 0xa1b2c3d4;
				header.timeZone = 0;
				header.significantFigures = 0;
				result = dumpId;
				header.versionMajor = 2;
				header.versionMinor = 4;
				header.snapshotLength = 0xffff;
				header.linkType = 1;
				sceNetPsWriteDump(dump, &header, sizeof(header));
				++gNetPsPacketCaptureActive;
				sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
				goto done;
			}
			result = -12;
		}
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
done:
	return result;
}
