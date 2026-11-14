/*
 * error.c: SNC's runtime errors (libsnc error.o, EDG's runtime, compiled as C++): the
 * message for each, and __abort_execution, which prints it on stderr and aborts.
 */
#include <stdio.h>
#include <stdlib.h>
#include "error.h"

static const char *error_text(an_error_code code)
{	/* the message for code */
	const char *text;

	switch (code) {
	case ec_abort_header:
		text = "C++ runtime abort";
		break;
	case ec_terminate_called:
		text = "terminate() called by the exception handling mechanism";
		break;
	case ec_terminate_returned:
		text = "returned from a user-defined terminate() routine";
		break;
	case ec_static_object_destroyed_twice:
		text = "internal error: static object marked for destruction more than once";
		break;
	case ec_pure_virtual_called:
		text = "a pure virtual function was called";
		break;
	case ec_bad_dynamic_cast:
		text = "invalid dynamic cast";
		break;
	case ec_bad_typeid:
		text = "invalid typeid operation";
		break;
	case ec_bad_array_delete:
		text = "freeing array not allocated by an array new operation";
		break;
	case ec_terminate_recursive:
		text = "terminate() called itself recursively";
		break;
	case ec_negative_vla_size:
		text = "negative size for variable-length array";
		break;
	case ec_vla_allocation_failed:
		text = "VLA allocation failed";
		break;
	case ec_deleted_virtual_called:
		text = "a deleted virtual function was called";
		break;
	default:
		fprintf(stderr, "Assertion failed in file \"%s\", line %d\n", "..\\error.c", 69);
		abort();
	}
	return text;
}

static void display_abort_message(an_error_code code)
{	/* report code on stderr */
	fprintf(stderr, "%s: %s\n", error_text(ec_abort_header), error_text(code));
}

extern "C" void __abort_execution(an_error_code code)
{	/* stop the program for a run-time error */
	display_abort_message(code);
	abort();
}
