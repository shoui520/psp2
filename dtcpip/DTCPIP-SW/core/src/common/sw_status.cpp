/*
 * DTCPIP-SW status code mapping, move message helpers, and global constants.
 */

#include "../osal/sw_osal_cpp.h"

#include <string.h>

extern "C" {

extern const unsigned char g_swMwCertificateTemplate[4] = { 0, 0, 0, 0 };

int SwMwStatus(int status)
{
	if ((status >= 0 && status <= 13) ||
	    (status >= 100 && status <= 103) ||
	    (status >= 200 && status <= 215) ||
	    (status >= 1000 && status <= 1048) ||
	    (status >= 1500 && status <= 1560) ||
	    (status >= 1600 && status <= 1652) ||
	    (status >= 10000 && status <= 10015) ||
	    (status >= 11000 && status <= 11022)) {
		return status;
	}
	return 12001;
}

} /* extern "C" */

struct SwMoveMessageRaw {
	void *vtable;
	uint8_t label;
	uint8_t key[8];
	uint8_t first[10];
	uint8_t second[10];
	uint8_t reserved[3];
};

static void swMwMoveMessageDelete(void *message);

extern "C" int SwMwMoveMessageDone(void *message);

static void *g_swMoveMessageVtable[2] = {
	(void *)SwMwMoveMessageDone,
	(void *)swMwMoveMessageDelete,
};

static void swMwMoveMessageDelete(void *message)
{
	SwMwMoveMessageDone(message);
	::operator delete(message);
}

extern "C" {

int SwMwMoveMessageInit(void *message)
{
	SwMoveMessageRaw *m = static_cast<SwMoveMessageRaw *>(message);
	m->vtable = g_swMoveMessageVtable;
	m->label = 0;
	memset(m->key, 0, sizeof(m->key));
	memset(m->first, 0, sizeof(m->first));
	memset(m->second, 0, sizeof(m->second));
	memset(m->reserved, 0, sizeof(m->reserved));
	return 0;
}

int SwMwMoveMessageDone(void *message)
{
	SwMoveMessageRaw *m = static_cast<SwMoveMessageRaw *>(message);
	m->vtable = g_swMoveMessageVtable;
	return 0;
}

} /* extern "C" */
