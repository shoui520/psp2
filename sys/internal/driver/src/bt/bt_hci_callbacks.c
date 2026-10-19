/* HCI retry timers; original source filename unavailable. */
#include "bt_internal.h"

void BtRoleSwitchTimedOut(void *argument)
{
	BtConnection *connection = argument;

	connection->authenticationTimer = 0;
	BtSendHciCommand(connection->stack, 0x080b, "61",
			 connection->deviceAddress, 0);
}

void BtRemoteNameTimedOut(void *argument)
{
	BtStack *stack = argument;

	stack->state = 35;
	BtSendHciCommand(stack, 0x041a, "6", stack->inquiryAddress);
	stack->nameRequestTimer = 0;
}

void BtReadAddressRetry(void *argument)
{
	BtStack *stack = argument;

	BtSendHciCommand(stack, 0x1009, gBtControllerEmptyFormat);
}
