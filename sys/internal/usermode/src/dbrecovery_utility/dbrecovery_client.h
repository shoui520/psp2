/* the shell's database recovery method (dbrecovery_client.cpp) */
#ifndef DBRECOVERY_UTILITY_DBRECOVERY_CLIENT_H
#define DBRECOVERY_UTILITY_DBRECOVERY_CLIENT_H

#include <scetypes.h>

namespace IPMI {
class Client;
}

/*
 * reports the database at path; returns the shell's result (the shell's 0.995.070 build names the
 * call: "DbRecoveryReportCorruption(%s) returned %#x")
 */
int DbRecoveryReportCorruption(IPMI::Client *client, const char *path, SceUInt32 flags,
			       SceInt32 kind);

#endif
