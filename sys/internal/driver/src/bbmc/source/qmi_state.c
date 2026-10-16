/* Active protocol-driver instance. */
#include "qmi_backend.h"

static unsigned int g_qmiConfiguredService = 0xe0;
QmiBackendControl *g_qmiBackendControl;
QmiBackendControl g_qmiBackendStorage;
BbcmdProtocolClient g_qmiSecondaryControlClient;
BbcmdProtocolClient g_qmiPrimaryControlClient;

/* name unknown: return the configured service accepted alongside fixed IDs. */
unsigned int qmiGetConfiguredService(void)
{
	return g_qmiConfiguredService;
}

/* name unknown: replace the configured protocol service identifier. */
void qmiSetConfiguredService(unsigned int service)
{
	g_qmiConfiguredService = service;
}

/* name unknown: return the published protocol-driver control object. */
QmiBackendControl *qmiGetBackendControl(void)
{
	return g_qmiBackendControl;
}
