/*
 * A service exchange: its errors in the core's form, its creation and release, and what it
 * asks of its NEMO message (0x810685FC-). The file's name is ours.
 */
#include "mln_internal.h"

/* an error of the NEMO message or the XML code, as an exchange's */
static inline unsigned int MlnExchangeError(unsigned int ret)
{
	switch (ret & 0xFFFF0000) {
	case 0x22060000:
	case 0x22020000:
	case 0x22030000:
		return MLN_EXCHANGE_ERROR_MESSAGE;
	case 0x10010000:
	case 0x22010000:
	case 0x22090000:
	case 0x220A0000:
	case 0x230F0000:
		return MLN_EXCHANGE_ERROR_FORMAT;
	case 0x23050000:
		switch (ret) {
		case 0x23050001:
			return MLN_EXCHANGE_ERROR_FORMAT;
		case 0x23050002:
			return MLN_EXCHANGE_ERROR_MESSAGE;
		case 0x23050003:
			return 0x23000003;
		case 0x23050004:
			return 0x23000004;
		case 0x23050005:
			return 0x23000005;
		case 0x23050007:
			return 0x23000007;
		case 0x23050008:
			return 0x23000009;
		case 0x23050009:
			return 0x2300000A;
		case 0x2305000A:
			return 0x23000006;
		case 0x2305000B:
			return 5;
		case 0x2305000C:
		case 0x2305000D:
		case 0x2305000E:
		case 0x2305000F:
		case 0x23050010:
		case 0x23050011:
		case 0x23050012:
		case 0x23050013:
		case 0x23050014:
		case 0x23050015:
			return 0x2300000B;
		case 0x23050016:
			return 0x23000008;
		}
		break;
	}
	return ret;
}

unsigned int MlnImpl_810685FC(unsigned int ret)
{
	return MlnExchangeError(ret);
}

/* drop what an exchange's message holds of a request */
unsigned int MlnImpl_81068798(MlnExchange *exchange)
{
	MlnNemoMessage *message = &exchange->message;
	unsigned int i;

	MlnImpl_81042A88(message->credentials);
	message->credentials = NULL;
	MlnXmlNodeFree(MLN_HEAP_GENERAL, message->body);
	message->body = NULL;
	if (message->nodes != NULL) {
		for (i = 0; i < message->nodeCount; i++) {
			MlnXmlNodeFree(MLN_HEAP_GENERAL, message->nodes[i]);
			message->nodes[i] = NULL;
		}
		MlnFree(MLN_HEAP_GENERAL, message->nodes);
		message->nodes = NULL;
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, message->peer);
	message->peer = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, message->nonce);
	message->nonce = NULL;
	return MlnImpl_810799B0(&message->header);
}

/* a new exchange of a protocol */
unsigned int MlnImpl_81068844(int protocol, MlnExchange **exchange)
{
	MlnExchange *new;

	new = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*new));
	if (new == NULL)
		return 2;
	MlnMemset(new, 0, sizeof(*new));
	new->protocol = protocol;
	*exchange = new;
	return 0;
}

/* free an exchange, and its message */
int MlnImpl_81068888(MlnExchange *exchange)
{
	if (exchange != NULL) {
		if (exchange->protocol == MLN_EXCHANGE_REGISTRATION ||
		    exchange->protocol == MLN_EXCHANGE_LICENSE ||
		    exchange->protocol == MLN_EXCHANGE_TRANSFER)
			MlnImpl_8107A19C(&exchange->message);
		MlnFree(MLN_HEAP_GENERAL, exchange);
	}
	return 0;
}

/* the Created time of the message the exchange read */
unsigned int MlnImpl_810688CC(MlnExchange *exchange, int *timestamp)
{
	unsigned int ret;

	if (exchange == NULL)
		return 4;
	ret = MlnImpl_8107A15C(&exchange->message, timestamp);
	if (ret != 0)
		ret = MlnExchangeError(ret);
	return ret;
}

/* give the exchange's message the certificate chains of its two peers */
unsigned int MlnImpl_81068A8C(MlnExchange *exchange, MlnBuffer *first, MlnBuffer *second)
{
	unsigned int ret;

	if (exchange == NULL)
		return 4;
	ret = MlnImpl_81079A7C(&exchange->message, first, second);
	if (ret != 0)
		ret = MlnExchangeError(ret);
	return ret;
}

/* the SOAP fault the exchange's message received */
unsigned int MlnImpl_81068C4C(MlnExchange *exchange, MlnNemoFaults **fault)
{
	if (exchange == NULL)
		return 4;
	if (fault == NULL)
		return 3;
	if (MlnImpl_81079814(&exchange->message, fault) != 0)
		return MLN_EXCHANGE_ERROR_FAULT;
	return 0;
}
