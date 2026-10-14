/* Adhoc session seed retrieval. */
#include "adhoc.h"

extern unsigned char gcAuthAdhocRandom[32];

int GcAuthAdhocGetSeeds160(const GcAuthAdhocGetSeeds160Request *request)
{
	const GcAuthAdhocContext160 *context = request->context;
	unsigned char *random = request->random;
	unsigned char *contextRandom = request->contextRandom;
	unsigned char *seed = request->seed;
	unsigned int index;

	if (random == NULL || context == NULL)
		return 0x808A0124;
	if (seed == NULL || contextRandom == NULL)
		return 0x808A0124;
	for (index = 0; index != 16; index++)
		random[index] = gcAuthAdhocRandom[index];
	for (index = 0; index != 16; index++)
		contextRandom[index] = context->ready.data.bytes[index];
	for (index = 0; index != 16; index++)
		seed[index] = context->ready.seed[index];
	return 0;
}

int GcAuthAdhocGetSeeds224(const GcAuthAdhocGetSeeds224Request *request)
{
	const GcAuthAdhocContext224 *context = request->context;
	unsigned char *random = request->random;
	unsigned char *contextRandom = request->contextRandom;
	unsigned char *seed = request->seed;
	unsigned int index;

	if (random == NULL || context == NULL)
		return 0x808A0124;
	if (seed == NULL || contextRandom == NULL)
		return 0x808A0124;
	for (index = 0; index != 16; index++)
		random[index] = gcAuthAdhocRandom[index];
	for (index = 0; index != 16; index++)
		contextRandom[index] = context->ready.data.bytes[index];
	for (index = 0; index != 16; index++)
		seed[index] = context->ready.seed[index];
	return 0;
}
