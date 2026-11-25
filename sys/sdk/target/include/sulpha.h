/* Sony's SDK sulpha.h (Release 03.570.011): Sulpha profiling/inspection library */
#pragma once

#include <stdbool.h>
#include <scetypes.h>

#define SCE_SULPHA_ERROR_ALREADY_INITIALIZED	-2142437375	/* 0x804D0001 */
#define SCE_SULPHA_ERROR_UNINITIALIZED		-2142437374	/* 0x804D0002 */
#define SCE_SULPHA_ERROR_INVALID_VALUE		-2142437373	/* 0x804D0003 */
#define SCE_SULPHA_ERROR_INVALID_POINTER	-2142437372	/* 0x804D0004 */
#define SCE_SULPHA_ERROR_INVALID_HANDLE		-2142437371	/* 0x804D0005 */
#define SCE_SULPHA_ERROR_INVALID_ALIGNMENT	-2142437370	/* 0x804D0006 */
#define SCE_SULPHA_ERROR_OUT_OF_MEMORY		-2142437369	/* 0x804D0007 */
#define SCE_SULPHA_ERROR_UNSUPPORTED		-2142437368	/* 0x804D0008 */
#define SCE_SULPHA_ERROR_CONNECTION		-2142437367	/* 0x804D0009 */
#define SCE_SULPHA_ERROR_NETWORK_OWNERSHIP	-2142437366	/* 0x804D000A */
#define SCE_SULPHA_ERROR_INVALID_AGENT_DEF	-2142437365	/* 0x804D000B */
#define SCE_SULPHA_ERROR_INVALID_AGENT_VERSION	-2142437364	/* 0x804D000C */
#define SCE_SULPHA_ERROR_INVALID_EVENT_ID	-2142437363	/* 0x804D000D */
#define SCE_SULPHA_ERROR_NOT_CONNECTED		-2142437362	/* 0x804D000E */
#define SCE_SULPHA_ERROR_EVENT_ALREADY_SENT	-2142437361	/* 0x804D000F */
#define SCE_SULPHA_ERROR_AGENT_LIMIT_REACHED	-2142437360	/* 0x804D0010 */
#define SCE_SULPHA_ERROR_CAPTURE_STARTED	-2142437359	/* 0x804D0011 */
#define SCE_SULPHA_ERROR_OUT_OF_BOOKMARKS	-2142437358	/* 0x804D0012 */

typedef SceUInt8 SceSulphaUTF8;
typedef void (*SceSulphaCallback)(void *arg);

typedef struct SceSulphaConfig {
	SceSulphaCallback notifyCallback;
	SceUInt32 port;
	SceUInt32 bookmarkCount;
} SceSulphaConfig;

int sceSulphaNetworkInit(void);
int sceSulphaNetworkShutdown(void);

int sceSulphaGetDefaultConfig(SceSulphaConfig *config);
int sceSulphaGetNeededMemory(const SceSulphaConfig *config, SceUInt32 *sizeInBytes);
int sceSulphaInit(const SceSulphaConfig *config, void *buffer, SceUInt32 sizeInBytes);
int sceSulphaShutdown(void);
int sceSulphaUpdate(void);

int sceSulphaFileConnect(const char *filename);
int sceSulphaFileDisconnect(void);

int sceSulphaSetBookmark(const SceSulphaUTF8 *name, SceInt32 id);
