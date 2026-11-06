/*
 * NP Activity's shell methods over SceShellSvc's IPMI connection: a header {context, value}
 * and the strings, each with its NUL. Sony's build keeps them out of the client's file (not
 * inlined); the file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "np_activity_client.h"

#define NP_ACTIVITY_METHOD_POST_STATUS			0x80006
#define NP_ACTIVITY_METHOD_POST_APP_STARTUP_STATUS	0x80007

typedef struct NpActivityIpcHeader {
	int context;
	int value;	/* the status kind, or the application version */
} NpActivityIpcHeader;

static inline SceSize stringSize(const char *string)
{
	const char *s;
	SceSize length = 0;

	for (s = string; *s != '\0'; s++)
		length++;
	return length + 1;
}

int npActivityIpcPostStatus(IPMI::Client *client, int context, int kind, const char *extraText0,
			    const char *extraText1, const char *message, const char *extraText2,
			    const char *actionUri)
{
	NpActivityIpcHeader header;
	IPMI::DataInfo in[6];
	int result = 0;
	int ret;

	header.context = context;
	header.value = kind;
	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = extraText0;
	in[1].dataSize = stringSize(extraText0);
	in[2].pData = extraText1;
	in[2].dataSize = stringSize(extraText1);
	in[3].pData = message;
	in[3].dataSize = stringSize(message);
	in[4].pData = extraText2;
	in[4].dataSize = stringSize(extraText2);
	in[5].pData = actionUri;
	in[5].dataSize = stringSize(actionUri);
	ret = client->invokeSyncMethod(NP_ACTIVITY_METHOD_POST_STATUS, in, 6, &result, NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}

int npActivityIpcPostAppStartupStatus(IPMI::Client *client, int context, const char *message,
				      SceUInt32 appVersion, const char *escapedParam)
{
	NpActivityIpcHeader header;
	IPMI::DataInfo in[3];
	int result = 0;
	int ret;

	header.context = context;
	header.value = appVersion;
	in[0].pData = &header;
	in[0].dataSize = sizeof(header);
	in[1].pData = message;
	in[1].dataSize = stringSize(message);
	in[2].pData = escapedParam;
	in[2].dataSize = stringSize(escapedParam);
	ret = client->invokeSyncMethod(NP_ACTIVITY_METHOD_POST_APP_STARTUP_STATUS, in, 3, &result,
				       NULL, 0);
	if (ret != 0)
		return ret;
	return result;
}
