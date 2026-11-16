/*
 * SceNpImpsUtil: the raw message client (impsutility/raw_message_client.cpp). It opens an IPMI
 * client to the NP service, registers a message type with it, and sends and receives raw messages.
 */
#ifndef NPIMPSUTIL_RAW_MESSAGE_CLIENT_H
#define NPIMPSUTIL_RAW_MESSAGE_CLIENT_H

#include "../npbasic/npbasic_internal.h"

/* the client's state: its message type, the largest message, and the IpmiWrapper client */
typedef struct NpImpsRawMsgClient {
	int type;
	SceSize maxSize;
	void *clientMemory;
	IpmiWrapper::Client *client;
} NpImpsRawMsgClient;

typedef int (*NpImpsRawMsgCallback)(void *);

extern int npImpsRawMsgClientCreate(int type, SceSize maxSize, int queueCount);
extern int npImpsRawMsgClientDestroy(void) __attribute__((noinline));
extern NpImpsRawMsgClient *npImpsRawMsgClientGet(void) __attribute__((noinline));
extern int npImpsRawMsgClientReceive(NpImpsRawMsgClient *client, void *buffer, SceSize size, void *received)
	__attribute__((noinline));
extern int npImpsRawMsgClientSend(NpImpsRawMsgClient *client, void *message, SceSize size)
	__attribute__((noinline));

extern int npImpsRawMsgStart(int type, SceSize maxSize, int queueCount, NpImpsRawMsgCallback callback);
extern int npImpsRawMsgStop(void);
extern int npImpsRawMsgSend(void *message, SceSize size);
extern int npImpsRawMsgReceive(void *buffer, SceSize size, int *received);

#endif
