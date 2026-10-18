/* Headset profile response formatting; original source filename unavailable. */
#include <sysmem/sysclib.h>
#include <stdarg.h>
#include "bt_internal.h"

int BtSendRfcommData(BtConnection *connection, unsigned int dlci,
		     const void *data, int length, int userMode);

int BtSendHspResponse(BtConnection *connection, const char *format, ...)
{
	va_list arguments;
	int length;

	va_start(arguments, format);
	length = vsnprintf((char *)connection->stack->scratch,
		sizeof(connection->stack->scratch), format, arguments);
	BtSendRfcommData(connection, connection->hspHeadsetChannel,
		connection->stack->scratch, length, 0);
	va_end(arguments);
	return 0;
}
