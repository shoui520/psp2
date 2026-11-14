/*
 * _Strerror function: the message for an error code.
 */
#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "libc_internal.h"

/* SCE: the names of the codes from 1000 on, in std::errc's spelling */
static const char *const errnames[] = {
	"argument_list_too_long",
	"permission_denied",
	"address_in_use",	/* EADDRINUSE */
	"address_not_available",	/* EADDRNOTAVAIL */
	"address_family_not_supported",	/* EAFNOSUPPORT */
	"resource_unavailable_try_again",
	"connection_already_in_progress",	/* EALREADY */
	"bad_file_descriptor",
	"bad_message",
	"device_or_resource_busy",
	"operation_canceled",
	"no_child_process",
	"connection_aborted",	/* ECONNABORTED */
	"connection_refused",	/* ECONNREFUSED */
	"connection_reset",	/* ECONNRESET */
	"resource_deadlock_would_occur",
	"destination_address_required",	/* EDESTADDRREQ */
	"argument_out_of_domain",
	"file_exists",
	"bad_address",
	"file_too_large",
	"host_unreachable",	/* EHOSTUNREACH */
	"identifier_removed",	/* EIDRM */
	"illegal_byte_sequence",
	"operation_in_progress",
	"interrupted",
	"invalid_argument",
	"io_error",
	"already_connected",	/* EISCONN */
	"is_a_directory",
	"too_many_synbolic_link_levels",	/* ELOOP */
	"too_many_files_open",
	"too_many_links",
	"message_size",
	"filename_too_long",
	"network_down",	/* ENETDOWN */
	"network_reset",	/* ENETRESET */
	"network_unreachable",	/* ENETUNREACH */
	"too_many_files_open_in_system",
	"no_buffer_space",	/* ENOBUFS */
	"no_message_available",	/* ENODATA */
	"no_such_device",
	"no_such_file_or_directory",
	"executable_format_error",
	"no_lock_available",
	"no_link",	/* ENOLINK */
	"not_enough_memory",
	"no_message",	/* ENOMSG */
	"no_protocol_option",	/* ENOPROTOOPT */
	"no_space_on_device",
	"no_stream_resources",	/* ENOSR */
	"not_a_stream",	/* ENOSTR */
	"function_not_supported",
	"not_connected",	/* ENOTCONN */
	"not_a_directory",
	"directory_not_empty",
	"state_not_recoverable",	/* ENOTRECOVERABLE */
	"not_a_socket",	/* ENOTSOCK */
	"not_supported",
	"inappropriate_io_control_operation",
	"no_such_device_or_address",
	"operation_not_supported",	/* EOPNOTSUPP */
	"other",	/* EOTHER */
	"value_too_large",	/* EOVERFLOW */
	"owner_dead",	/* EOWNERDEAD */
	"operation_not_permitted",
	"broken_pipe",
	"protocol_error",	/* EPROTO */
	"protocol_not_supported",	/* EPROTONOSUPPORT */
	"wrong_protocol_type",	/* EPROTOTYPE */
	"result_out_of_range",
	"read_only_file_system",
	"invalid_seek",
	"no_such_process",
	"stream_timeout",	/* ETIME */
	"timed_out",
	"text_file_busy",	/* ETXTBSY */
	"operation_would_block",	/* EWOULDBLOCK */
	"cross_device_link",
};

/* SCE: buf is ignored (perror and strerror pass the code alone) */
char *_Strerror(int errcode, char *buf)
{
	char *s;

	switch (errcode) {
	case 0:
		s = (char *)"no error";
		break;
	case EPERM:
		s = (char *)"operation not permitted";
		break;
	case ENOENT:
		s = (char *)"no such file or directory";
		break;
	case ESRCH:
		s = (char *)"no such process";
		break;
	case EINTR:
		s = (char *)"interrupted function call";
		break;
	case EIO:
		s = (char *)"input/output error";
		break;
	case ENXIO:
		s = (char *)"no such device or address";
		break;
	case E2BIG:
		s = (char *)"argument list too long";
		break;
	case ENOEXEC:
		s = (char *)"exec format error";
		break;
	case EBADF:
		s = (char *)"bad file descriptor";
		break;
	case ECHILD:
		s = (char *)"no child processes";
		break;
	case EAGAIN:
		s = (char *)"resource temporarily unavailable";
		break;
	case ENOMEM:
		s = (char *)"not enough space";
		break;
	case EACCES:
		s = (char *)"permission denied";
		break;
	case EFAULT:
		s = (char *)"bad address";
		break;
	case EBUSY:
		s = (char *)"resource busy";
		break;
	case EEXIST:
		s = (char *)"file exists";
		break;
	case EXDEV:
		s = (char *)"improper link";
		break;
	case ENODEV:
		s = (char *)"no such device";
		break;
	case ENOTDIR:
		s = (char *)"not a directory";
		break;
	case EISDIR:
		s = (char *)"is a directory";
		break;
	case EINVAL:
		s = (char *)"invalid argument";
		break;
	case ENFILE:
		s = (char *)"too many files open in system";
		break;
	case EMFILE:
		s = (char *)"too many open files";
		break;
	case ENOTTY:
		s = (char *)"bad I/O control operation";
		break;
	case EFBIG:
		s = (char *)"file too large";
		break;
	case ENOSPC:
		s = (char *)"no space left on device";
		break;
	case ESPIPE:
		s = (char *)"invalid seek";
		break;
	case EROFS:
		s = (char *)"read-only file system";
		break;
	case EMLINK:
		s = (char *)"too many links";
		break;
	case EPIPE:
		s = (char *)"broken pipe";
		break;
	case EDOM:
		s = (char *)"domain error";
		break;
	case ERANGE:
		s = (char *)"range error";
		break;
	case EDEADLK:
		s = (char *)"resource deadlock avoided";
		break;
	case ENOLCK:
		s = (char *)"no locks available";
		break;
	case EBADMSG:
		s = (char *)"bad message";
		break;
	case ENOSYS:
		s = (char *)"function not implemented";
		break;
	case ENOTEMPTY:
		s = (char *)"directory not empty";
		break;
	case ENAMETOOLONG:
		s = (char *)"filename too long";
		break;
	case ETIMEDOUT:
		s = (char *)"operation timed out";
		break;
	case EINPROGRESS:
		s = (char *)"operation in progress";
		break;
	case EMSGSIZE:
		s = (char *)"bad message buffer length";
		break;
	case ENOTSUP:
		s = (char *)"not supported";
		break;
	case EILSEQ:
		s = (char *)"multibyte encoding error";
		break;
	case ECANCELED:
		s = (char *)"operation canceled";
		break;
	case EFPOS:
		s = (char *)"file positioning error";
		break;
	default:
		if (1000 <= errcode && (unsigned int)errcode < 1000 + sizeof(errnames) / sizeof(errnames[0])) {
			s = (char *)errnames[errcode - 1000];
		} else {	/* an unknown code: "error 0x" and its hex digits */
			const char digits[] = "0123456789abcdef";
			unsigned int n = errcode;
			char *p;

			s = _Getstrerrorbuf();
			strncpy(s, "error 0x00000000", sizeof("error 0x00000000"));
			for (p = s + 15; n != 0; n >>= 4)
				*p-- = digits[n & 0xf];
		}
		break;
	}
	return s;
}
