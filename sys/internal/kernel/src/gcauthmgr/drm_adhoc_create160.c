/* Adhoc 160-bit initial key exchange. */
#include <threadmgr/threadmgr.h>
#include "adhoc.h"

extern SceUID gcAuthAdhocSemaphore;
extern int gcAuthAdhocInitialized;
extern GcAuthAdhocCertificate gcAuthAdhocCertificate;
extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
extern GcAuthAdhocGeneratedKeyPair gcAuthAdhocGeneratedKeyPair;
int GcAuthAdhocReadCertificate160(void);
int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);
void GcAuthAdhocBuildMessage(unsigned char *output,
		const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize);
int GcAuthAdhocEncryptPacket(const unsigned char *first, unsigned int firstSize,
		const unsigned char *second, unsigned int secondSize,
		const unsigned char *third, unsigned int thirdSize,
		const unsigned char *fourth, unsigned int fourthSize,
		const unsigned char *fifth, unsigned int fifthSize,
		const unsigned char *sixth, unsigned int sixthSize);

int GcAuthAdhocCreateContext160(const GcAuthAdhocCreateContext160Request *request)
{
	GcAuthAdhocContext160 *context = request->context;
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
	result = GcAuthAdhocReadCertificate160();
	if (result != 0)
		goto finish;
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocGeneratedKeyPair, NULL,
			12, 0, 0) != 0) {
		result = 0x808A0101;
		goto finish;
	}
	GcAuthAdhocBuildMessage((unsigned char *)context,
		(const unsigned char *)&gcAuthAdhocGeneratedKeyPair, 60,
		(const unsigned char *)&gcAuthAdhocCertificate, 16,
		NULL, 0, NULL, 0);
	result = GcAuthAdhocEncryptPacket(
		gcAuthAdhocGeneratedKeyPair.key160.publicKey.serialized, 40,
		(const unsigned char *)&gcAuthAdhocCertificate, 96,
		NULL, 0, NULL, 0, NULL, 0, NULL, 0);
	if (result == 0) {
		source = (const unsigned char *)gcAuthAdhocWorkspace.counterStream.blocks;
		end = source + 160;
		do {
			*packet++ = *source++;
		} while (source != end);
	}
finish:
	if (sceKernelSignalSema(gcAuthAdhocSemaphore, 1) != 0)
		result = 0x808A010A;
	return result;
}
