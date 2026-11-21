/*
 * mtp_client_lib.h: the MTP client the shell links (a header of ours: no record names one),
 * the connection MTPR2's protocol 2 downloads over. initializeClient and finalizeClient are
 * Sony's names (its messages); every other name is ours (PLACEHOLDER).
 */
#ifndef _MTP_CLIENT_LIB_H
#define _MTP_CLIENT_LIB_H

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <std/map>

#define MTP_CLIENT_ERROR_NOT_CONNECTED	((int)0x80540001)	/* PLACEHOLDER names */
#define MTP_CLIENT_ERROR_BUSY		((int)0x80540002)
#define MTP_CLIENT_ERROR_NO_MEMORY	((int)0x80540007)

/* the connection's status */
struct MtpClientStatus {
	int state;			/* 3 and 4: connected */
	int reserved[20];		/* PLACEHOLDER: not read */
	int kind;			/* the object's kind: 1, 2 */
	int reserved2[4];		/* PLACEHOLDER: not read */
};

class MtpClient {
public:
	MtpClient()
		: sender(NULL), receiver(NULL), status(0), statusArg(0), statusError(0),
		  requests(), observerStarted(0), observerEvent(0), observerArg(0),
		  observerThread(-1), observerStop(0), session(-1)
	{
	}

	~MtpClient();

	int finalizeClient(void);
	int Bgdl_81137ADE(void);				/* finalize, if initialized */
	int initializeClient(void);
	int Bgdl_81138068(int handle, int mode);		/* close the object */
	int Bgdl_811380BC(const char *url, unsigned int size, int flags, SceInt64 offset,
			  int *handle);				/* open it */
	int Bgdl_81138184(int handle);				/* end the transfer */
	int Bgdl_81138266(int handle, void *buf, unsigned int size, unsigned int *received,
			  SceUInt64 *remaining);		/* receive */
	int Bgdl_81138432(MtpClientStatus *status);		/* the status */
	int Bgdl_8113844A(int handle);				/* abort the transfer */
	int Bgdl_81138498(int *mode);
	int Bgdl_811384AE(int arg);
	int Bgdl_811384C4(void);

	void *sender;
	void *receiver;
	SceKernelLwMutexWork lock;
	int status;
	int statusArg;
	int statusError;
	int reserved[2];		/* PLACEHOLDER: not set */
	paf::map<int, int> requests;
	int observerStarted;
	int observerEvent;
	int observerArg;
	SceUID observerThread;		/* SceMtpStatusObserver */
	int observerStop;
	int session;
};

#endif
