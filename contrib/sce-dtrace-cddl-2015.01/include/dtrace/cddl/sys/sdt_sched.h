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

// this file does not exist in Solaris DTrace

#ifndef _SYS_SDT_SCHED_H
#define	_SYS_SDT_SCHED_H

#include <dtrace/sce/solaris_types.h>
#include <dtrace/cddl/sys/sdt.h>

#ifdef KERNEL

#if defined(SCE_KERNEL_CONFIG_USE_DTRACE) && !defined(DTRACE_DISABLE_SDT_PROBES)

#define	DTRACE_SCHED(name, ordinal) \
	DTRACE_PROBE(__sched_, name, ordinal)

#define	DTRACE_SCHED1(name, ordinal, type0, arg0) \
	DTRACE_PROBE1(__sched_, name, ordinal, arg0)

#define	DTRACE_SCHED2(name, ordinal, type0, arg0, type1, arg1) \
	DTRACE_PROBE2(__sched_, name, ordinal, arg0, arg1)

#define	DTRACE_SCHED3(name, ordinal, type0, arg0, type1, arg1, type2, arg2) \
	DTRACE_PROBE3(__sched_, name, ordinal, arg0, arg1, arg2)

#define	DTRACE_SCHED4(name, ordinal, type0, arg0, type1, arg1, type2, arg2, type3, arg3) \
	DTRACE_PROBE4(__sched_, name, ordinal, arg0, arg1, arg2, arg3)

#define	DTRACE_PROC(name, ordinal) \
	DTRACE_PROBE(__proc_, name, ordinal)

#define	DTRACE_PROC1(name, ordinal, type0, arg0) \
	DTRACE_PROBE1(__proc_, name, ordinal, arg0)

#define	DTRACE_PROC2(name, ordinal, type0, arg0, type1, arg1) \
	DTRACE_PROBE2(__proc_, name, ordinal, arg0, arg1)

#define	DTRACE_PROC3(name, ordinal, type0, arg0, type1, arg1, type2, arg2) \
	DTRACE_PROBE3(__proc_, name, ordinal, arg0, arg1, arg2)

#define	DTRACE_PROC4(name, ordinal, type0, arg0, type1, arg1, type2, arg2, type3, arg3) \
	DTRACE_PROBE4(__proc_, name, ordinal, arg0, arg1, arg2, arg3)

#else /* CONFIG_DTRACE */

#define	DTRACE_SCHED(name, ordinal) do {} while (0)
#define	DTRACE_SCHED1(name, ordinal, type1, arg1) do {} while (0)
#define	DTRACE_SCHED2(name, ordinal, type1, arg1, type2, arg2) do {} while (0)
#define	DTRACE_SCHED3(name, ordinal, type1, arg1, type2, arg2, type3, arg3) do {} while (0)
#define	DTRACE_SCHED4(name, ordinal, type1, arg1, type2, arg2, type3, arg3, type4, arg4) do {} while (0)

#define DTRACE_PROC(name, ordinal) do {} while(0)
#define DTRACE_PROC1(name, ordinal, type1, arg1) do {} while(0)
#define	DTRACE_PROC2(name, ordinal, type1, arg1, type2, arg2) do {} while (0)
#define	DTRACE_PROC3(name, ordinal, type1, arg1, type2, arg2, type3, arg3) do {} while (0)
#define	DTRACE_PROC4(name, ordinal, type1, arg1, type2, arg2, type3, arg3, type4, arg4) do {} while(0)

#endif /* SCE_KERNEL_CONFIG_USE_DTRACE && !DTRACE_DISABLE_SDT_PROBES */

#endif /* KERNEL */

#endif	/* _SYS_SDT_SCHED_H */
