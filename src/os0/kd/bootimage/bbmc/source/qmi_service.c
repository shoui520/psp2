/* Classification of the protocol's fixed and configured service IDs. */
#include "qmi_backend.h"

/* name unknown: distinguish the control service from supported data services. */
int qmiClassifyService(unsigned int service)
{
	switch (service) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 8:
	case 10:
	case 11:
		return 0;
	case 0:
		return 1;
	}
	if (qmiGetConfiguredService() == service)
		return 0;
	return (int)0x80330002;
}
