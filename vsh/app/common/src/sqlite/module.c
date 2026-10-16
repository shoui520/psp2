/* SceSqliteVsh module lifecycle. */

extern int sqlite3_shutdown(void);

int module_start(unsigned int arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

int module_stop(unsigned int arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	sqlite3_shutdown();
	return 0;
}
