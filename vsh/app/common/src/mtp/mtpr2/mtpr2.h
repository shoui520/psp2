/*
 * mtpr2.h: MTPR2, the download of a file the PC serves over the MTP connection
 * (mtpr2.cc). MTPR2Args, MTPR2FileAttributes, MTPR2StartDownloadStep1,
 * MTPR2StartDownloadStep2, MTPR2EndDownload and MTPR2Read are Sony's names (its
 * messages); every other name is ours (PLACEHOLDER).
 */
#ifndef _MTPR2_H
#define _MTPR2_H

#include <stddef.h>
#include <scetypes.h>

#define MTPR2_URL_MAX			1024	/* PLACEHOLDER names */

#define MTPR2_ERROR_NOT_STARTED		((int)0x80105F01)	/* PLACEHOLDER names */
#define MTPR2_ERROR_INVALID_ARGUMENT	((int)0x80105F02)
#define MTPR2_ERROR_TIMEOUT		((int)0x80105F03)
#define MTPR2_ERROR_BUSY		((int)0x80105F04)
#define MTPR2_ERROR_CANCELED		((int)0x80105F05)
#define MTPR2_ERROR_NO_MEMORY		12

/* MTPR2_OPTION_PROTOCOL's value: the responder the download uses */
#define MTPR2_PROTOCOL_HOSTFS		"fakehost"	/* the host's file system */
#define MTPR2_PROTOCOL_PROTO2		"proto2"
#define MTPR2_OPTION_PROTOCOL		1		/* PLACEHOLDER name */

/* what a download is started with */
struct MTPR2Args {
	const char *url;
	int reserved;
	SceInt64 offset;			/* where it starts: only 0 */
	SceUInt32 ringBufSize;			/* the ring buffer's size */
	SceUInt32 *timeout;			/* how long a read waits, NULL forever */
	SceUInt32 optionFlags;			/* PLACEHOLDER: not read */
	SceUInt32 optionType;			/* MTPR2_OPTION_PROTOCOL */
	SceUInt32 optionValue;			/* a const char **: the protocol's name */
	int reserved2;
};

/* the file downloaded */
struct MTPR2FileAttributes {
	SceUInt32 size;
	SceUInt32 time;				/* its date, an RTC tick */
	SceUInt32 kind;				/* the MTP object's kind: 1, 2, or 3 any other */
};

int MTPR2StartDownloadStep1(MTPR2Args *args, unsigned int *fileHandle);
int MTPR2StartDownloadStep2(unsigned int fileHandle, MTPR2FileAttributes *attr);
int MTPR2EndDownload(unsigned int fileHandle, unsigned int *timeout);
int MTPR2Read(unsigned int fileHandle, void *buf, unsigned int nbyte, long long offset,
	      unsigned int *read);
int MTPR2AbortDownload(void);			/* PLACEHOLDER name */

#endif
