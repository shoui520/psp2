/* Shared ES3 bus-error records and decoder table entries. */
#ifndef SCE_BUSERROR_TYPES_H
#define SCE_BUSERROR_TYPES_H

#include <psp2kern/types.h>

/* the record in the sysroot shared memory (+0x6000) */
typedef struct BusErrorRecord {
	SceSize hdrSize;			/* blocks follow at hdrSize */
	SceSize size;				/* header and blocks */
	SceSize ocpSize;
	SceSize siSize;
	SceSize grabSize;
	SceSize regbusSize;
	SceSize vipSize;			/* present when hdrSize > 27 */
} BusErrorRecord;

typedef struct BusErrorOcpError {
	SceUInt32 address;
	SceUInt32 attribute;
} BusErrorOcpError;

#define BUSERROR_OCP_TARGETS	18

typedef struct BusErrorOcpInfo {
	SceSize size;				/* up to target[] */
	SceUInt32 status[2][3];			/* BET0, BET1, BEBT */
	SceUInt32 count;
	struct {
		BusErrorOcpError error[2];	/* first, second error */
	} target[BUSERROR_OCP_TARGETS];
} BusErrorOcpInfo;

typedef struct BusErrorSiInfo {
	SceSize size;				/* up to the buses */
	SceUInt32 count;
} BusErrorSiInfo;

/* one bus, followed by its initiator and then its target agents */
typedef struct BusErrorSiBus {
	SceSize size;
	SceUInt32 initiatorCount;
	SceUInt32 targetCount;
	SceUInt32 address;
	SceUInt64 flagStatus[5];
} BusErrorSiBus;

typedef struct BusErrorSiAgent {
	SceSize size;
	SceUInt32 address;
	SceUInt64 status;
	SceUInt64 errorLog;
	SceUInt64 errorAddress;
} BusErrorSiAgent;

typedef struct BusErrorGrabInfo {
	SceSize size;
	SceUInt32 status;
	SceUInt32 write[4];
	SceUInt32 read[4];
	SceUInt32 secureStatus;
	SceUInt32 secureWrite[4];
	SceUInt32 secureRead[4];
	SceUInt32 auxiliaryRegisters[2];
} BusErrorGrabInfo;

typedef struct BusErrorRegbusInfo {
	SceSize size;
	SceUInt32 status;
	SceUInt32 error[4];
	SceUInt32 secureStatus;
	SceUInt32 secureError[4];
} BusErrorRegbusInfo;

typedef struct BusErrorVipInfo {
	SceSize size;
	SceUInt32 bap[3];
	SceUInt32 vdpd[3];
	SceUInt32 vdpm[3];
	SceUInt32 venezia[3];
} BusErrorVipInfo;

/* tables */
typedef struct BusErrorOcpTarget {
	const char *name;
	SceUInt32 mask;				/* in the status word */
	int (*isOn)(void);
	struct {
		const SceUInt32 *address;
		const SceUInt32 *attribute;	/* NULL: none */
	} error[2];
} BusErrorOcpTarget;

typedef struct BusErrorPort {
	const char *name;			/* NULL ends a list */
	SceUInt32 address;			/* SsxAgentRegs */
} BusErrorPort;

typedef struct BusErrorSystemBus {
	const char *name;
	SceUInt32 mask;				/* in BEBT */
	SceUInt32 address;			/* SsxBusRegs */
	const BusErrorPort *initiators;
	const BusErrorPort *targets;
} BusErrorSystemBus;

#endif
