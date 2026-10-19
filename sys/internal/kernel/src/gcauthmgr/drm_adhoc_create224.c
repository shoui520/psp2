/* Adhoc 224-bit initial key exchange. */
#include <threadmgr/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern int gcAuthAdhocInitialized;
extern GcAuthAdhocCertificate gcAuthAdhocCertificate;
extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
GcAuthAdhocGeneratedKeyPair gcAuthAdhocGeneratedKeyPair;
int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);

int GcAuthAdhocCreateContext224(const GcAuthAdhocCreateContext224Request *request)
{
	GcAuthAdhocContext224 *context = request->context;
	unsigned char *packet = request->packet;
	const unsigned char *source;
	const unsigned char *end;
	int result;

	if (sceKernelWaitSema(gcAuthAdhocSemaphore, 1, NULL) != 0)
		return 0x808A010A;
	if (context == NULL || packet == NULL) {
		result = 0x808A0124;
		goto finish;
	}
	result = 0x808A010F;
	if (!gcAuthAdhocInitialized)
		goto finish;
	result = GcAuthAdhocReadCertificate224();
	if (result != 0)
		goto finish;
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocGeneratedKeyPair, NULL,
			20, 0, 0) != 0) {
		result = 0x808A0101;
		goto finish;
	}
	GcAuthAdhocBuildExtendedMessage((unsigned char *)context,
		(const unsigned char *)&gcAuthAdhocGeneratedKeyPair, 84,
		(const unsigned char *)&gcAuthAdhocCertificate, 16,
		NULL, 0, NULL, 0);
	result = GcAuthAdhocEncryptPacket(
		gcAuthAdhocGeneratedKeyPair.key224.publicKey.serialized, 56,
		(const unsigned char *)&gcAuthAdhocCertificate, 128,
		NULL, 0, NULL, 0, NULL, 0, NULL, 0);
	if (result == 0) {
		source = (const unsigned char *)gcAuthAdhocWorkspace.counterStream.blocks;
		end = source + 208;
		do {
			*packet++ = *source++;
		} while (source != end);
	}
finish:
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = 0x808A010A;
	return result;
}
