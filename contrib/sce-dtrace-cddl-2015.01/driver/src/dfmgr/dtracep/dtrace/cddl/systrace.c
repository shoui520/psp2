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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (C) 2015 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

/* #pragma ident	"@(#)systrace.c	1.6	06/09/19 SMI" */

#include <sys/dtrace.h>
#include <sys/systrace.h>

#include <dtrace/sce/sce_systrace.h>

//#undef DTRACE_DEBUG_LEVEL
//#define DTRACE_DEBUG_LEVEL	2

dev_info_t *systrace_devi;
dtrace_provider_id_t systrace_id;

static dtrace_pattr_t systrace_attr = {
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
{ DTRACE_STABILITY_EVOLVING, DTRACE_STABILITY_EVOLVING, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_ISA },
};

static dtrace_pops_t systrace_pops = {
	systrace_provide_sce,
	NULL,
	systrace_probe_enable_sce,
	systrace_probe_disable_sce,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	systrace_probe_destroy_sce
};

int
systrace_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	DTRACE_LOG_CALL(1);

	if (ddi_create_minor_node(devi, "systrace", S_IFCHR, 0,
	    DDI_PSEUDO, 0) == DDI_FAILURE ||
	    dtrace_register("syscall", &systrace_attr, DTRACE_PRIV_USER, NULL,
	    &systrace_pops, NULL, &systrace_id) != 0) {
		ddi_remove_minor_node(devi, NULL);
		return (DDI_FAILURE);
	}

	ddi_report_dev(devi);
	systrace_devi = devi;

	return (DDI_SUCCESS);
}

int
systrace_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	DTRACE_LOG_CALL(1);

	if (dtrace_unregister(systrace_id) != 0) {
		return (DDI_FAILURE);
	}

	ddi_remove_minor_node(devi, NULL);

	systrace_detach_sce();

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
systrace_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	(void)devp;
	(void)flag;
	(void)otyp;
	(void)cred_p;

	DTRACE_LOG_CALL(1);

	return (0);
}

int
systrace_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	(void)dev;
	(void)flag;
	(void)otyp;
	(void)cred_p;

	DTRACE_LOG_CALL(1);

	return (0);
}
