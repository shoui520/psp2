/*
 * SceMarlinHci module entry points.
 */

int MarlinInitialize(void);
void MarlinHandlesInitialize(void);
void MarlinHandlesTerminate(void);
void MarlinTerminate(void);

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

