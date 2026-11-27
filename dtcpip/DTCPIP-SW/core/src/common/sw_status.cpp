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

class SwMoveMessageImpl {
public:
	SwMoveMessageImpl()
		: m_label(0)
	{
		memset(m_key, 0, sizeof(m_key));
		memset(m_first, 0, sizeof(m_first));
		memset(m_second, 0, sizeof(m_second));
		memset(m_reserved, 0, sizeof(m_reserved));
	}
	virtual ~SwMoveMessageImpl() {}

	uint8_t m_label;
	uint8_t m_key[8];
	uint8_t m_first[10];
	uint8_t m_second[10];
	uint8_t m_reserved[3];
};

extern "C" {

int SwMwMoveMessageInit(void *message)
{
	new (message) SwMoveMessageImpl();
	return 0;
}

int SwMwMoveMessageDone(void *message)
{
	reinterpret_cast<SwMoveMessageImpl *>(message)->~SwMoveMessageImpl();
	return 0;
}

} /* extern "C" */
