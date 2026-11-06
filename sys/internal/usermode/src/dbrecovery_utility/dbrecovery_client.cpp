/*
 * The shell's database recovery method, called over SceShellSvc's IPMI connection. Sony's build
 * keeps it out of the module's file (it is not inlined into its one caller); the file's name is
 * ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "../../../../../vsh/lowlayer/src/ipmi/client.h"
#include "dbrecovery_client.h"

/* the shell's method that recovers a database */
#define DBRECOVERY_METHOD_REPORT	0x150000

struct DbRecoveryReportArgs {
	SceUInt32 flags;
	SceInt32 kind;
};

int DbRecoveryReportCorruption(IPMI::Client *client, const char *path, SceUInt32 flags,
			       SceInt32 kind)
{
	int result = 0;
	DbRecoveryReportArgs args;
	IPMI::DataInfo in[2];
	SceSize length;
	int ret;

	args.flags = flags;
	args.kind = kind;
	in[0].pData = &args;
	in[0].dataSize = sizeof(args);
	in[1].pData = path;
	for (length = 0; path[length] != '\0'; length++)
		;
	in[1].dataSize = length + 1;
	ret = client->invokeSyncMethod(DBRECOVERY_METHOD_REPORT, in, 2, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
