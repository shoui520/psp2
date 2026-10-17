/*
 * SceDeci4pTtyp: the development host's consoles. Eight DECI4P channels
 * (protocols 0x80030000 + n, "TTYP0" .. "TTYP7") back the devices tty0: ..
 * tty7: ("deci4p_ttyp_dev_fs", mounted at /tty).
 *
 * Input from the host is kept in a ring buffer per channel; output is sent in
 * packets of up to 4 KiB taken from three shared resources, queued per
 * channel until SceDeci4pDfMgr lets the channel send.
 */

#pragma once

#include <psp2kern/types.h>
#include <deci4p_dfmgr/deci4p.h>
#include <kernel/error.h>

#define TTYP_CHANNELS		8
#define TTYP_RESOURCES		3
#define TTYP_DATA_MAX		0x1000
#define TTYP_IN_SIZE		0x400



/* an output packet */
typedef struct TtypResource {
	TtypHeader hdr;
	char data[TTYP_DATA_MAX];
	SceUInt32 state;			/* 1: reserved, 2: queued */
	struct TtypResource *next;
	int handle;				/* the channel's DfMgr handle */
	SceUInt32 reserved[2];
} TtypResource;

/* input ring buffer */
typedef struct TtypInput {
	SceUInt32 flags;			/* TTYP_IN_* */
	unsigned char *wp;
	unsigned char *rp;
	unsigned char data[TTYP_IN_SIZE];
} TtypInput;

#define TTYP_IN_RAW		0x1		/* no ^S / ^Q / ^C, no line editing */
#define TTYP_IN_STOPPED		0x2		/* ^S, or the host went away */
#define TTYP_IN_INTR		0x4		/* ^C ends the current read */

typedef struct TtypChannel {
	int handle;				/* sceDfMgrOpen */
	int openCount;
	SceUID evf;				/* 1: input, 2: state, 4: closed */
	SceUID readMutex;
	SceUID writeMutex;
	TtypInput *in;
	TtypHeader rhdr;			/* packet being received */
	SceSize rdata;				/* its data bytes in rbuf */
	SceSize rlen;				/* its bytes received */
	unsigned char rbuf[8];
	SceUInt32 flags;			/* TTYP_CH_* */
	SceUInt32 reserved0[2];
	TtypResource head;			/* queue sentinel: head.next is the first */
	TtypResource *tail;
	TtypResource *sending;
	SceUInt32 reserved1[14];
} TtypChannel;

#define TTYP_CH_ENABLED		0x1		/* the host listens */
#define TTYP_CH_INTR		0x2		/* ^C received */

_Static_assert(sizeof(TtypResource) == 0x1040, "TtypResource");
_Static_assert(sizeof(TtypInput) == 0x40c, "TtypInput");
_Static_assert(sizeof(TtypChannel) == 0x10e0, "TtypChannel");

/* ttyp.c */
int ttypLock(void);
void ttypUnlock(void);
SceUID ttypGetHeap(void);
int TtypOpen(int unit);
int TtypClose(int unit);
int TtypRead(int unit, void *buf, SceSize size, SceUInt32 flags);
int TtypWrite(int unit, const void *buf, SceSize size, const SceUID *pid);

/* ttyp_fs.c */
int TtypFsInitVfs(void);
