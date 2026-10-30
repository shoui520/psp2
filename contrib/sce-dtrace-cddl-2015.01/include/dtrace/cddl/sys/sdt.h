/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_SDT_H
#define	_SYS_SDT_H

#include <dtrace/sce/solaris_types.h>

#ifdef KERNEL

#define SDT_PROBEDESC_VERSION		1

struct sdt_probedesc;

typedef union {
	void (*handler0)(const struct sdt_probedesc *desc);
	void (*handler1)(const struct sdt_probedesc *desc, uintptr_t arg0);
	void (*handler2)(const struct sdt_probedesc *desc, uintptr_t arg0, uintptr_t arg1);
	void (*handler3)(const struct sdt_probedesc *desc, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2);
	void (*handler4)(const struct sdt_probedesc *desc, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);
	void (*handler5)(const struct sdt_probedesc *desc, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);
	void (*handler6)(const struct sdt_probedesc *desc, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5);
	void (*handler7)(const struct sdt_probedesc *desc, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, uintptr_t arg6);
	void (*handler8)(const struct sdt_probedesc *desc, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, uintptr_t arg6, uintptr_t arg7);
	void (*handler9)(const struct sdt_probedesc *desc, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, uintptr_t arg6, uintptr_t arg7, uintptr_t arg8);
	void (*handler10)(const struct sdt_probedesc *desc, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, uintptr_t arg6, uintptr_t arg7, uintptr_t arg8, uintptr_t arg9);
} sdpd_handler_t;

typedef struct sdt_probedesc {
	unsigned int *sdpd_id;									/* probe id - filled in when created */
	const char *sdpd_provider;								/* name of provider */
	const char *sdpd_name;									/* name of this probe */
//	const char *sdpd_func;									/* instrumentation point (function) */
//	const char *sdpd_srcfile;								/* instrumentation point (source file) */
//	int sdpd_srcline;										/* instrumentation point (source line) */
	void *sdpd_offset;										/* instrumentation point (address) */
	sdpd_handler_t *sdpd_handler_fn;						/* probe handler_fn function (NULL if disabled) */
	void *sdpd_private;										/* probe private data */
	void (*sdpd_create_fn)(const struct sdt_probedesc *desc);	/* probe create helper function (NULL if unused) */
	void (*sdpd_enable_fn)(const struct sdt_probedesc *desc); 	/* probe enable helper function (NULL if unused) */
	void (*sdpd_disable_fn)(const struct sdt_probedesc *desc);	/* probe disable helper function (NULL if unused) */
	void (*sdpd_destroy_fn)(const struct sdt_probedesc *desc);	/* probe destroy helper function (NULL if unused) */
} sdt_probedesc_t;

extern void dtrace_probe_sdt(const sdt_probedesc_t *probeDesc, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);

typedef struct sdt_probes_info {
	unsigned int 					version;
	unsigned int 					count;
} sdt_probes_info_t;

#if defined(SCE_KERNEL_CONFIG_USE_DTRACE) && !defined(DTRACE_DISABLE_SDT_PROBES)

#define DTRACE_STRINGIFY(s) #s
#define DTRACE_TOSTRING(s) DTRACE_STRINGIFY(s)

#define DTRACE_PROBE_LABEL_STRING(p, n, o) \
    "__dtrace_probe$" DTRACE_TOSTRING(o) DTRACE_STRINGIFY(_##p##___##n)

#define DTRACE_SDT_EMIT_PROBE_DESC_GLOBAL(provider, name, ordinal)		\
{ \
	asm volatile (	\
				".type  " DTRACE_PROBE_LABEL_STRING(provider, name, ordinal) ", %%object"	"\n\t"  \
				".globl " DTRACE_PROBE_LABEL_STRING(provider, name, ordinal) 	"\n\t"	\
				"# "	\
				  : \
				  : \
	); \
}

#define DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn) \
	__label__ pc; \
	pc:; \
	static unsigned int sdtid = 0; \
	static sdpd_handler_t handler; \
	static const sdt_probedesc_t desc asm(DTRACE_PROBE_LABEL_STRING(provider, name, ordinal)) \
		__attribute__((used)) = \
		{ \
		.sdpd_id = &sdtid, \
		.sdpd_provider = DTRACE_STRINGIFY(provider), \
		.sdpd_name = DTRACE_STRINGIFY(name), \
		/* .sdpd_func = __FUNCTION__, */ \
		/* .sdpd_srcfile = __FILE__, */ \
		/* .sdpd_srcline = __LINE__, */ \
		.sdpd_offset = (void *)&&pc, \
		.sdpd_handler_fn = &handler, \
		.sdpd_private = NULL, \
		.sdpd_create_fn = create_fn, \
		.sdpd_enable_fn = enable_fn, \
		.sdpd_disable_fn = disable_fn, \
		.sdpd_destroy_fn = destroy_fn \
		}; \
		DTRACE_SDT_EMIT_PROBE_DESC_GLOBAL(provider, name, ordinal);

/*
 * The following macros are used to create static probes. The argument types
 * should be no greater than uintptr_t in size each. The behavior of larger
 * types is undefined.
 */

// Reference:
// GCC Local Labels: http://gcc.gnu.org/onlinedocs/gcc/Local-Labels.html
// GCC Variable Attributes: http://gcc.gnu.org/onlinedocs/gcc/Variable-Attributes.html#Variable-Attributes

#define DTRACE_PROBE_EX(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler0) { \
		desc.sdpd_handler_fn->handler0(&desc); \
	} \
}
#define DTRACE_PROBE(provider, name, ordinal) DTRACE_PROBE_EX(provider, name, ordinal, NULL, NULL, NULL, NULL)

#define DTRACE_PROBE_EX1(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn, arg0) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler1) { \
		desc.sdpd_handler_fn->handler1(&desc, (uintptr_t)arg0); \
	} \
}
#define DTRACE_PROBE1(provider, name, ordinal, arg0) DTRACE_PROBE_EX1(provider, name, ordinal, NULL, NULL, NULL, NULL, arg0)

#define DTRACE_PROBE_EX2(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn, arg0, arg1) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler2) { \
		desc.sdpd_handler_fn->handler2(&desc, (uintptr_t)arg0, (uintptr_t)arg1); \
	} \
}
#define DTRACE_PROBE2(provider, name, ordinal, arg0, arg1) DTRACE_PROBE_EX2(provider, name, ordinal, NULL, NULL, NULL, NULL, arg0, arg1)

#define DTRACE_PROBE_EX3(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn, arg0, arg1, arg2) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler3) { \
		desc.sdpd_handler_fn->handler3(&desc, (uintptr_t)arg0, (uintptr_t)arg1, (uintptr_t)arg2); \
	} \
}
#define DTRACE_PROBE3(provider, name, ordinal, arg0, arg1, arg2) DTRACE_PROBE_EX3(provider, name, ordinal, NULL, NULL, NULL, NULL, arg0, arg1, arg2)

#define DTRACE_PROBE_EX4(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn, arg0, arg1, arg2, arg3) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler4) { \
		desc.sdpd_handler_fn->handler4(&desc, (uintptr_t)arg0, (uintptr_t)arg1, (uintptr_t)arg2, (uintptr_t)arg3); \
	} \
}
#define DTRACE_PROBE4(provider, name, ordinal, arg0, arg1, arg2, arg3) DTRACE_PROBE_EX4(provider, name, ordinal, NULL, NULL, NULL, NULL, arg0, arg1, arg2, arg3)

#define DTRACE_PROBE_EX5(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn, arg0, arg1, arg2, arg3, arg4) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler5) { \
		desc.sdpd_handler_fn->handler5(&desc, (uintptr_t)arg0, (uintptr_t)arg1, (uintptr_t)arg2, (uintptr_t)arg3, (uintptr_t)arg4); \
	} \
}
#define DTRACE_PROBE5(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4) DTRACE_PROBE_EX5(provider, name, ordinal, NULL, NULL, NULL, NULL, arg0, arg1, arg2, arg3, arg4)

#define DTRACE_PROBE_EX6(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn, arg0, arg1, arg2, arg3, arg4, arg5) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler6) { \
		desc.sdpd_handler_fn->handler6(&desc, (uintptr_t)arg0, (uintptr_t)arg1, (uintptr_t)arg2, (uintptr_t)arg3, (uintptr_t)arg4, (uintptr_t)arg5); \
	} \
}
#define DTRACE_PROBE6(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4, arg5) DTRACE_PROBE_EX6(provider, name, ordinal, NULL, NULL, NULL, NULL, arg0, arg1, arg2, arg3, arg4, arg5)

#define DTRACE_PROBE_EX7(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn, arg0, arg1, arg2, arg3, arg4, arg5, arg6) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler7) { \
		desc.sdpd_handler_fn->handler7(&desc, (uintptr_t)arg0, (uintptr_t)arg1, (uintptr_t)arg2, (uintptr_t)arg3, (uintptr_t)arg4, (uintptr_t)arg5, (uintptr_t)arg6); \
	} \
}
#define DTRACE_PROBE7(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4, arg5, arg6) DTRACE_PROBE_EX7(provider, name, ordinal, NULL, NULL, NULL, NULL, arg0, arg1, arg2, arg3, arg4, arg5, arg6)

#define DTRACE_PROBE_EX8(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler8) { \
		desc.sdpd_handler_fn->handler8(&desc, (uintptr_t)arg0, (uintptr_t)arg1, (uintptr_t)arg2, (uintptr_t)arg3, (uintptr_t)arg4, (uintptr_t)arg5, (uintptr_t)arg6, (uintptr_t)arg7); \
	} \
}
#define DTRACE_PROBE8(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7) DTRACE_PROBE_EX8(provider, name, ordinal, NULL, NULL, NULL, NULL, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7)

#define DTRACE_PROBE_EX9(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler9) { \
		desc.sdpd_handler_fn->handler9(&desc, (uintptr_t)arg0, (uintptr_t)arg1, (uintptr_t)arg2, (uintptr_t)arg3, (uintptr_t)arg4, (uintptr_t)arg5, (uintptr_t)arg6, (uintptr_t)arg7, (uintptr_t)arg8); \
	} \
}
#define DTRACE_PROBE9(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) DTRACE_PROBE_EX9(provider, name, ordinal, NULL, NULL, NULL, NULL, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)

#define DTRACE_PROBE_EX10(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) { \
	DTRACE_SDT_EMIT_PROBE_DESC(provider, name, ordinal, create_fn, enable_fn, disable_fn, destroy_fn); \
	if(desc.sdpd_handler_fn->handler10) { \
		desc.sdpd_handler_fn->handler10(&desc, (uintptr_t)arg0, (uintptr_t)arg1, (uintptr_t)arg2, (uintptr_t)arg3, (uintptr_t)arg4, (uintptr_t)arg5, (uintptr_t)arg6, (uintptr_t)arg7, (uintptr_t)arg8, (uintptr_t)arg9); \
	} \
}
#define DTRACE_PROBE10(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) DTRACE_PROBE_EX10(provider, name, ordinal, NULL, NULL, NULL, NULL, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)

#if 0
#define	DTRACE_IO(name) \
	DTRACE_PROBE(__io_, name)

#define	DTRACE_IO1(name, type1, arg1) \
	DTRACE_PROBE1(__io_, name, arg1)

#define	DTRACE_IO2(name, type1, arg1, type2, arg2) \
	DTRACE_PROBE2(__io_, name, type1, arg1, type2, arg2)

#define	DTRACE_IO3(name, type1, arg1, type2, arg2, type3, arg3) \
	DTRACE_PROBE3(__io_, name, arg1, arg2, arg3)

#define	DTRACE_IO4(name, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
	DTRACE_PROBE4(__io_, name, arg1, arg2, arg3, arg4)

#define	DTRACE_INT5(name, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5) \
	DTRACE_PROBE5(__sdt_, name, arg1, arg2, arg3, arg4, arg5)

#define	DTRACE_TMR3(name, type1, arg1, type2, arg2, type3, arg3) \
	DTRACE_PROBE3(__sdt_, name, arg1, arg2, arg3)

#define	DTRACE_VM(name) \
	DTRACE_PROBE(__vminfo_, name)

#define	DTRACE_VM1(name, type1, arg1) \
	DTRACE_PROBE1(__vminfo_, name, arg1)

#define	DTRACE_VM2(name, type1, arg1, type2, arg2) \
	DTRACE_PROBE2(__vminfo_, name, arg1, arg2)

#define	DTRACE_VM3(name, type1, arg1, type2, arg2, type3, arg3) \
	DTRACE_PROBE3(__vminfo_, name, arg1, arg2, arg3)

#define	DTRACE_VM4(name, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
	DTRACE_PROBE4(__vminfo_, name, arg1, arg2, arg3, arg4)
#endif

#else /* CONFIG_DTRACE */

#define DTRACE_PROBE(provider, name, ordinal) do {} while (0)
#define DTRACE_PROBE1(provider, name, ordinal, arg0) do {} while (0)
#define DTRACE_PROBE2(provider, name, ordinal, arg0, arg1) do {} while (0)
#define DTRACE_PROBE3(provider, name, ordinal, arg0, arg1, arg2) do {} while (0)
#define DTRACE_PROBE4(provider, name, ordinal, arg0, arg1, arg2, arg3) do {} while (0)
#define DTRACE_PROBE5(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4) do {} while (0)
#define DTRACE_PROBE6(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4, arg5) do {} while (0)
#define DTRACE_PROBE7(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4, arg5, arg6) do {} while (0)
#define DTRACE_PROBE8(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7) do {} while (0)
#define DTRACE_PROBE9(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) do {} while (0)
#define DTRACE_PROBE10(provider, name, ordinal, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9) do {} while (0)

#if 0
#define	DTRACE_IO(name, ordinal) do {} while(0)
#define	DTRACE_IO1(name, ordinal, type1, arg1) do {} while(0)
#define	DTRACE_IO2(name, ordinal, type1, arg1, type2, arg2) do {} while(0)
#define	DTRACE_IO3(name, ordinal, type1, arg1, type2, arg2, type3, arg3) do {} while(0)
#define	DTRACE_IO4(name, ordinal, type1, arg1, type2, arg2, type3, arg3, type4, arg4) do {} while(0)
#define	DTRACE_INT5(name, ordinal, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5) do {} while(0)
#define	DTRACE_TMR3(name, ordinal, type1, arg1, type2, arg2, type3, arg3) do {} while(0)

#define DTRACE_VM(name, ordinal) do {} while(0)
#define DTRACE_VM1(name, ordinal, type1, arg1) do {} while(0)
#define DTRACE_VM2(name, ordinal, type1, arg1, type2, arg2) do {} while(0)
#define DTRACE_VM3(name, ordinal, type1, arg1, type2, arg2, type3, arg3) do {} while(0)
#define DTRACE_VM4(name, ordinal, type1, arg1, type2, arg2, type3, arg3, type4, arg4) do {} while(0)
#endif

#endif /* SCE_KERNEL_CONFIG_USE_DTRACE && !DTRACE_DISABLE_SDT_PROBES */

#endif /* KERNEL */

#endif	/* _SYS_SDT_H */
