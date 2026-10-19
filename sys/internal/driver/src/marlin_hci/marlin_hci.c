#include "handles.h"
#include "key.h"
/*
 * SceMarlinHci module entry points.
 */

int module_start(void)
{
	int result = MarlinInitialize();

	if (result == 0)
		MarlinHandlesInitialize();
	return result;
}

int module_stop(void)
{
	MarlinHandlesTerminate();
	MarlinTerminate();
	return 0;
}

