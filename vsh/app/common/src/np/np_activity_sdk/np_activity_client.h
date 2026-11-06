/*
 * The status posts this library sends to the shell (np_activity_client.cpp,
 * np_activity_ipc_client.cpp). The names are ours.
 */
#ifndef NP_ACTIVITY_SDK_NP_ACTIVITY_CLIENT_H
#define NP_ACTIVITY_SDK_NP_ACTIVITY_CLIENT_H

#include <scetypes.h>
#include "../../../../../lowlayer/src/ipmi/client.h"

/* the client's errors; the library reports every other 0x801010xx one as internal */
#define NP_ACTIVITY_CLIENT_ERROR_INVALID_ARGUMENT	-2146430975	/* 0x80101001 */
#define NP_ACTIVITY_CLIENT_ERROR_INVALID_STATUS_MESSAGE	-2146430905	/* 0x80101047 */
#define NP_ACTIVITY_CLIENT_ERROR_BASE			-2146430976	/* 0x80101000 */

/*
 * The context is 0 from this library. A status carries its message, an action URI and a third
 * text, "" when there is none; an application startup status carries the application version
 * and its escaped parameter.
 */
int npActivityPostStatus(int context, const char *message, const char *actionUri,
			 const char *extraText);
int npActivityPostAppStartupStatus(int context, const char *message, SceUInt32 appVersion,
				   const char *escapedParam);

/* np_activity_utf8.cpp */
int countUtf8Characters(const char *string, SceSize size);
int countLinefeeds(const char *string, SceSize size);

/* the shell's methods */
int npActivityIpcPostStatus(IPMI::Client *client, int context, int kind, const char *extraText0,
			    const char *extraText1, const char *message, const char *extraText2,
			    const char *actionUri);
int npActivityIpcPostAppStartupStatus(IPMI::Client *client, int context, const char *message,
				      SceUInt32 appVersion, const char *escapedParam);

#endif
