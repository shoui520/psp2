/* Serialized packet decryption. */
#include "adhoc.h"

extern GcAuthAdhocWorkspace gcAuthAdhocWorkspace;
int GcAuthExecuteKirkCommand(void *output, const void *input, int command,
		size_t size, int keyId);

int GcAuthAdhocDecryptPacket(const unsigned char *data, unsigned int size)
{
	GcAuthAdhocCounterStream *input = &gcAuthAdhocWorkspace.counterStream;
	unsigned char *payload = (unsigned char *)input->blocks;
	const unsigned char *end;

	if (size != 0) {
		end = data + size;
		do {
			*payload++ = *data++;
		} while (data != end);
	}
	input->size = size;
	input->mode = 5;
	input->zero[0] = 0;
	input->zero[1] = 0;
	input->keyId = 2;
	if (GcAuthExecuteKirkCommand(&gcAuthAdhocWorkspace, input->blocks,
			7, size, 2) != 0)
		return (int)0x808a010b;
	return 0;
}
