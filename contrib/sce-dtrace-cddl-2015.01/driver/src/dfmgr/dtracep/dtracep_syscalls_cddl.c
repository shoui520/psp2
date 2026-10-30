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
 * Copyright 2013 Sony Computer Entertainment Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Derived from OpenSolaris /usr/src/lib/libdtrace/common/drti.c, onnv b86
 */

#include <kernel/pragma_driver.h>
#include <dtrace/logging.h>
#include <dtrace/sce/solaris_types.h>

#include "dtrace_client.h"
#include "dtrace_client_list.h"
#include "dtracep_syscalls_cddl.h"

//#undef DTRACE_DEBUG_LEVEL
//#define DTRACE_DEBUG_LEVEL	2

SceInt dtrace_add_helper_dof(SceUIntPtr dof)
{
	SceUID processId = dtrace_get_current_process_id();
	SceUID moduleId = SCE_UID_INVALID_UID;
	SceInt token = -1;
    dof_hdr_t dofHdr;

    SceInt res = copyin(dof, &dofHdr, sizeof(dof_hdr_t));
    if(res < 0) {
    	DTRACE_LOG_ERROR("failed to read helper DOF magic at addr:0x%08x in process:0x%08x (0x%08x)", dof, processId, res);
    	return -1;
    }

    int ii;
    const char *dofMagic = DOF_MAG_STRING;
	for(ii = 0; ii < DOF_MAG_STRLEN; ii++) {
		if(dofHdr.dofh_ident[ii] != dofMagic[ii]) {
			DTRACE_LOG_ERROR("failed to add corrupt helper DOF at addr:0x%08x in process:0x%08x", dof, processId);
			return -1;
		}
	}

	moduleId = dtrace_get_current_process_module_at_address(dof);
	if(moduleId <= 0) {
		DTRACE_LOG_ERROR("failed to lookup module containing helper DOF at addr:0x%08x in process:0x%08x", dof, processId);
		return -1;
	}

	token = sceDTraceClientOpen("/dev/dtrace/helper", processId, moduleId, 0);
	if(token < 0) {
		DTRACE_LOG_ERROR("failed to open client for helper DOF in process:0x%08x module:0x%08x (0x%08x)", processId, moduleId, token);
		return -1;
	}

	DTRACE_LOG_DEBUG(1, "opened client for helper DOF with token:%d in process:0x%08x module:0x%08x", token, processId, moduleId);

	DtraceClientCB *client = dtraceFindClientByToken(dtraceClients, token);
	if(!client) {
		DTRACE_LOG_ERROR("failed to find client with token:%d for helper DOF in process:0x%08x module:0x%08x", token, processId, moduleId);
		SCE_PURE_ASSERT1(client != SCE_NULL);
		return -1;
	}

	dof_helper_t dh;
	memset(&dh, 0, sizeof(dof_helper_t));
	dh.dofhp_dof = dof;

	// fill in module name (not filled in by drti.o) containing address of .SUNW_dof section
	dtrace_get_module_name(moduleId, dh.dofhp_mod, sizeof(dh.dofhp_mod));

	int rv = 0;
	res = dtrace_ioctl_helper(dtrace_get_current_process(), DTRACEHIOC_ADDDOF, (SceUIntPtr)&dh, &rv);
	if(res < 0 || rv < 0) {
		DTRACE_LOG_ERROR("failed to add helper DOF from addr:0x%08x in process:0x%08x module:0x%08x (0x%08x)", dof, processId, moduleId, res < 0 ? res : rv);
		sceDTraceClientClose(token);
		return -1;
	}

	client->helperDofGen = rv;

	DTRACE_LOG_DEBUG(1, "added helper DOF with generation:%d from addr:0x%08x in process:0x%08x module:0x%08x", client->helperDofGen, dof, processId, moduleId);

	return token;
}

SceInt dtrace_remove_helper_dof(SceInt token)
{
    SceInt res = SCE_OK;

	DtraceClientCB *client = dtraceFindClientByToken(dtraceClients, token);
	if(!client) {
		DTRACE_LOG_ERROR("failed to find client with token:%d", token);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}

	int helperDofGen = client->helperDofGen;
	SceUID processId = client->processId;
	SceUID moduleId = client->moduleId;

	(void)moduleId;

	proc_t *p = proc_find(processId);
	if(!p) {
		DTRACE_LOG_WARNING("failed to lookup process:0x%08x", processId);
	} else {
		int rv = 0;
		res = dtrace_ioctl_helper(p, DTRACEHIOC_REMOVE, (SceUIntPtr)helperDofGen, &rv);
		if(res < 0 || rv < 0) {
			DTRACE_LOG_WARNING("failed to remove helper DOF with generation:%d in process:0x%08x module:0x%08x (0x%08x)", helperDofGen, processId, moduleId, res < 0 ? res : rv);
		} else {
			client->helperDofGen = -1;
		}

		DTRACE_LOG_DEBUG(1, "removed helper DOF with generation:%d in process:0x%08x module:0x%08x", helperDofGen, processId, moduleId);

		proc_rele(p); // release reference to process from proc_find()
	}

	res = sceDTraceClientClose(token);
	if(res < 0) {
		DTRACE_LOG_ERROR("failed to close client for helper DOF in process:0x%08x module:0x%08x (0x%08x)", processId, moduleId, res);
	}

	DTRACE_LOG_DEBUG(1, "closed client for helper DOF in process:0x%08x module:0x%08x", processId, moduleId);
	return res;
}
