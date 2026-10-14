/*
 * Background application launch through the ShellSvc request interface.
 * Interface and operation names retain NIDs and numeric IDs where unresolved.
 */
#include <stdint.h>
#include <stddef.h>

struct ShellRequestInterface {
	void **methods;
};

typedef int (*ShellRequestMethod)(struct ShellRequestInterface *, uint32_t,
				 uint32_t, uint32_t, void *, void *);

extern int SceShellSvc_A0B067AC(void *request, uint32_t type);
extern int SceShellSvc_8D4F9F2E(uint32_t type, uint32_t flags);
extern struct ShellRequestInterface *SceShellSvc_B31E7F1C(void);
extern int sceClibPrintf(const char *format, ...);

static void assertShellResult(int result)
{
	if (!result)
		return;
	sceClibPrintf("[SceBgAppUtil] ASSERTION FAILURE (0x%x)\n", result);
	__asm__ volatile("bkpt #0");
}

static int sendRequest180000(struct ShellRequestInterface *interface,
			     uint32_t *token, void *request)
{
	ShellRequestMethod method = (ShellRequestMethod)interface->methods[7];

	return method(interface, 0x180000, 0, 0, token, request);
}

static int sendRequest180001(struct ShellRequestInterface *interface,
			     uint32_t token)
{
	ShellRequestMethod method = (ShellRequestMethod)interface->methods[9];
	int output = 0;
	int result;

	result = method(interface, 0x180001, token, (uint32_t)(uintptr_t)&output,
			NULL, NULL);
	return result ? result : output;
}

int sceBgAppUtilStartBgApp(uint32_t flags)
{
	uint32_t request[2];
	uint32_t token;
	int result;

	if (flags)
		return (int)0x80106501U;
	assertShellResult(SceShellSvc_A0B067AC(request, 5));
	result = sendRequest180000(SceShellSvc_B31E7F1C(), &token, request);
	if (result)
		return result;
	assertShellResult(SceShellSvc_8D4F9F2E(5, 0));
	return sendRequest180001(SceShellSvc_B31E7F1C(), token);
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
