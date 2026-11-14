/*
 * error.h: the run-time errors that stop a C++ program (libsnc error.o), and the call that
 * reports one on stderr and aborts.
 */
#ifndef _LIBSNC_ERROR_H
#define _LIBSNC_ERROR_H

typedef enum an_error_code
	{	/* the messages of error_text */
	ec_none,
	ec_abort_header,			/* "C++ runtime abort" */
	ec_terminate_called,			/* by the exception handling */
	ec_terminate_returned,			/* from a user-defined terminate() */
	ec_static_object_destroyed_twice,	/* an internal error */
	ec_assertion_failed,			/* no message of its own */
	ec_pure_virtual_called,
	ec_bad_dynamic_cast,
	ec_bad_typeid,
	ec_bad_array_delete,			/* not allocated by an array new */
	ec_terminate_recursive,
	ec_negative_vla_size,
	ec_vla_allocation_failed,
	ec_deleted_virtual_called
	} an_error_code;

#ifdef __cplusplus
extern "C" {
#endif
void __abort_execution(an_error_code code);
#ifdef __cplusplus
}
#endif
#endif /* _LIBSNC_ERROR_H */
