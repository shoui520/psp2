/*
 * The host's commands (Sony's dtracep_command.c), run by a channel's thread
 * with the DTrace mutex held. A reply's command is the request's plus one.
 */
#pragma once

#include "dtracep.h"

#define DTRACEP_CMD_TEST		0x00
#define DTRACEP_CMD_OPEN		0x02
#define DTRACEP_CMD_CLOSE		0x04
#define DTRACEP_CMD_GET_BIG_DATA	0x06
#define DTRACEP_CMD_PUT_BIG_DATA	0x08
#define DTRACEP_CMD_PROVIDER		0x20
#define DTRACEP_CMD_PROBES		0x22
#define DTRACEP_CMD_BUF_SNAP		0x24
#define DTRACEP_CMD_PROBE_MATCH		0x26
#define DTRACEP_CMD_ENABLE		0x28
#define DTRACEP_CMD_AGG_SNAP		0x2a
#define DTRACEP_CMD_EPROBE		0x2c
#define DTRACEP_CMD_PROBE_ARG		0x2e
#define DTRACEP_CMD_CONF		0x30
#define DTRACEP_CMD_STATUS		0x32
#define DTRACEP_CMD_GO			0x34
#define DTRACEP_CMD_STOP		0x36
#define DTRACEP_CMD_AGG_DESC		0x38
#define DTRACEP_CMD_FORMAT		0x3a
#define DTRACEP_CMD_DOF_GET		0x3c
#define DTRACEP_CMD_REPLICATE		0x3e
#define DTRACEP_CMD_ALL_PROBES		0x40
#define DTRACEP_CMD_ALL_PROBE_MATCH	0x42
#define DTRACEP_CMD_ALL_EPROBES		0x44
#define DTRACEP_CMD_GET_MODULE_INFO	0x50
#define DTRACEP_CMD_GET_PROCESS_INFO	0x52

/* the error status a reply carries */
#define DTRACEP_ERROR_INVALID_COMMAND	8

int dtracepTestCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepOpenCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepCloseCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepGetBigDataCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepPutBigDataCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepProviderCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepProbesCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepBufSnapCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepProbeMatchCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepEnableCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepAggSnapCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepEProbeCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepProbeArgCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepConfCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepStatusCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepGoCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepStopCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepAggDescCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepFormatCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepDofGetCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepReplicateCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepAllProbesCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepAllProbeMatchCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepAllEProbesCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepGetModuleInfoCmd(DtracepChannel *ch, DtracepPacket *packet);
int dtracepGetProcessInfoCmd(DtracepChannel *ch, DtracepPacket *packet);

/* reply to a request with an error status */
int dtracepSendErrorReply(DtracepChannel *ch, SceUInt8 command, int error);
