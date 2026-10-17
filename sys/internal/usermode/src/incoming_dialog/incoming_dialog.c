/* Incoming dialog ShellSvc requests; original request operation IDs retained. */
#include <stdint.h>
#include <stddef.h>

#include <vsh/lowlayer/src/shell_svc/shellsvc.h>

static int sendDialogRequest(uint32_t operation,
			     const struct ShellSvcBuffer *inputs, uint32_t inputCount,
			     const struct ShellSvcBuffer *outputs, uint32_t outputCount)
{
	struct ShellSvcInterface *interface = SceShellSvc_B31E7F1C();
	ShellSvcRequest request = (ShellSvcRequest)interface->methods[5];
	int output = 0;
	int result;

	result = request(interface, operation, inputs, inputCount, &output,
			 outputs, outputCount);
	return result ? result : output;
}

int sceIncomingDialogInitialize(uint32_t flags)
{
	if (flags)
		return (int)0x80106202U;
	return sendDialogRequest(0x170000, NULL, 0, NULL, 0);
}

int sceIncomingDialogFinish(void)
{
	return sendDialogRequest(0x170001, NULL, 0, NULL, 0);
}

int sceIncomingDialogOpen(void *parameters)
{
	const char *text = "";
	struct ShellSvcBuffer inputs[2];
	uint32_t textSize = 0;

	if (!parameters)
		return (int)0x80106202U;
	if (*(uint32_t *)parameters >= 0x03000000U)
		text = (char *)parameters + 0x25a;
	while (text[textSize])
		textSize++;
	textSize++;
	inputs[0].address = parameters;
	inputs[0].size = 0x25c;
	inputs[1].address = (void *)text;
	inputs[1].size = textSize;
	return sendDialogRequest(0x170002, inputs, 2, NULL, 0);
}

int sceIncomingDialogClose(void)
{
	return sendDialogRequest(0x170003, NULL, 0, NULL, 0);
}

uint32_t sceIncomingDialogGetStatus(void)
{
	uint32_t status = 0;
	struct ShellSvcBuffer output = { &status, sizeof(status) };

	sendDialogRequest(0x170004, NULL, 0, &output, 1);
	return status;
}

int sceIncomingDialogSwitchToDialog(void)
{
	return sendDialogRequest(0x170005, NULL, 0, NULL, 0);
}

int module_start(uint32_t argumentSize, void *arguments)
{
	return 0;
}

int module_stop(uint32_t argumentSize, void *arguments)
{
	return 0;
}

int module_exit(uint32_t argumentSize, void *arguments)
{
	return 0;
}
