/* Transport queue initialization; original source filename unavailable. */
#include "bt_internal.h"

int BtSendHciCommand(BtStack *stack, unsigned int opcode,
		     const char *format, ...);

int BtInitializeQueues(BtStack *stack)
{
	unsigned int controllerType;

	BtRingReset(&stack->commandQueue);
	stack->commandQueue.data = stack->commandData;
	stack->commandQueue.capacity = sizeof(stack->commandData);
	stack->commandLength = 0;
	stack->commandBusy = 0;
	BtRingReset(&stack->aclQueue);
	stack->aclQueue.data = stack->aclData;
	stack->aclQueue.capacity = sizeof(stack->aclData);
	controllerType = stack->controllerType;
	stack->aclLength = 0;
	stack->localAclHandle = 0xffff;
	if (controllerType == 1) {
		stack->state = 1;
		return BtSendHciCommand(stack, 0xfc5b, "1", 241);
	}
	stack->state = 2;
	return BtSendHciCommand(stack, 0x1009, gBtQueueInitializationFormat);
}
