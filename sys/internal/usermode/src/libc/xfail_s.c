/* set_constraint_handler_s, abort_handler_s, ignore_handler_s and _Fail_s functions */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static constraint_handler_t handler = NULL;	/* NULL: report on stderr */

constraint_handler_t set_constraint_handler_s(constraint_handler_t newhandler)
{
	constraint_handler_t oldhandler;

	_Locksyslock(_LOCK_MISC);
	oldhandler = handler;
	handler = newhandler;
	_Unlocksyslock(_LOCK_MISC);
	return oldhandler;
}

void abort_handler_s(const char *__restrict msg, void *__restrict ptr, errno_t error)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	abort();
}

void ignore_handler_s(const char *__restrict msg, void *__restrict ptr, errno_t error)
{
}

/* report a runtime-constraint violation */
int _Fail_s(const char *mesg, size_t siz)
{
	if (mesg == NULL || mesg[strnlen_s(mesg, siz)] != '\0')
		mesg = "constraint handler: bad message";
	if (handler != NULL) {
		(*handler)(mesg, NULL, ERANGE);
	} else {
		fputs(mesg, stderr);
		fputc('\n', stderr);
	}
	return ERANGE;
}
