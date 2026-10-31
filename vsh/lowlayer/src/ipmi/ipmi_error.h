#ifndef IPMI_ERROR_H
#define IPMI_ERROR_H

/*
 * libipmi's own error codes (facility 0x0002, 0x8002050x-0x8002051x). No Sony header or
 * string names them; the names say what the library returns each one for.
 */
#define IPMI_ERROR_WOULD_BLOCK			-2147351295	/* 0x80020501 */
#define IPMI_ERROR_NOT_FOUND			-2147351291	/* 0x80020505 */
#define IPMI_ERROR_NOT_INITIALIZED		-2147351281	/* 0x8002050F */
#define IPMI_ERROR_INVALID_ARGUMENT		-2147351279	/* 0x80020511 */
#define IPMI_ERROR_TIMEOUT			-2147351270	/* 0x8002051A */
#define IPMI_ERROR_CANCELED			-2147351269	/* 0x8002051B */
#define IPMI_ERROR_NOT_OWNER			-2147351268	/* 0x8002051C */

#endif
