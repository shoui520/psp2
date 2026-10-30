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
 * Copyright (c) 2007 NEC Corporation
 */

/*
 * Copyright (C) 2015 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

/* #ident	"@(#)sdt.c" */

#include <sys/dtrace.h>
#include <sys/sdt_impl.h>

#include <dtrace/sce/dtrace_security.h>

#if defined(__SCE__)
#include <modulemgr/modulecb_kernel.h>
#include <dtrace/cddl/sys/dtrace_impl.h>
#endif
//#undef DTRACE_DEBUG_LEVEL
//#define DTRACE_DEBUG_LEVEL	2

int sdt_verbose = 1;

/*ARGSUSED*/
static void
sdt_provide_module(void *arg, struct modctl *ctl)
{
	(void)arg;

	ASSERT(ctl != NULL);

	if(ctl->sdt_probes == NULL) {
		return;
	}

	if(ctl->sdt_nprobes != 0) { // already provided?
		return;
	}

	if(dtrace_security_check_module_probes(ctl) != 0) {
		// don't create any probes for this module
		return;
	}

	char modName[256] = {0};
	if(dtrace_security_check_module_info(ctl) == 0) {
		strncpy(modName, ModuleGetModuleName(ctl), sizeof(modName) - 1);
	}
	if(modName[0] == '\0') {
		snprintf(modName, sizeof(modName) - 1, "0x%08x", ctl->uid);
	}

	/*
	 * One for all, and all for one:  if we haven't yet registered all of
	 * our providers, we'll refuse to provide anything.
	 */
	sdt_provider_t *prov;
	for (prov = sdt_providers; prov->sdtp_name != NULL; prov++) {
		if (prov->sdtp_id == DTRACE_PROVNONE) {
			DTRACE_LOG_DEBUG(2, "%s - providerName %s not yet registered", __FUNCTION__, prov->sdtp_name);
			return;
		}
	}

	DTRACE_LOG_DEBUG(1, "%s - registering SDT probes in module: %s", __FUNCTION__, modName);

	// count probes (list is NULL-terminated)
	int nprobes = 0;
	for (nprobes = 0; ctl->sdt_probes[nprobes] != NULL; nprobes++) {}

//	boolean_t allowInternal = dtrace_security_check_internal() == 0;
	int primary = 0;
	int p;
	for (p = 0; p < nprobes; p++) { // probe description list is NULL-terminated
		sdt_probedesc_t *sdpd = ctl->sdt_probes[p];
		ASSERT(sdpd != NULL);

		DTRACE_LOG_DEBUG(1, "%s - registering %s module probe - providerName: %s probeName: %s",
				__FUNCTION__, modName, sdpd->sdpd_provider, sdpd->sdpd_name);

		// find provider
		for (prov = sdt_providers; prov->sdtp_prefix != NULL; prov++) {
			const char *prefix = prov->sdtp_prefix;

			if (strncmp(sdpd->sdpd_provider, prefix, strlen(prefix)) == 0) {
				break;
			}
		}

		// convert probe name '__' to '-'
#if defined(__SCE__)
		int probeNameLen = (int)strlen(sdpd->sdpd_name);
		char *probeName = (char *)kmem_alloc((size_t)(probeNameLen + 1), KM_SLEEP);
#else
		int probeNameLen = strlen(sdpd->sdpd_name);
		char *probeName = kmem_alloc(probeNameLen + 1, KM_SLEEP);
#endif
		int i, j;
		for (i = 0, j = 0; sdpd->sdpd_name[j] != '\0'; i++) {
			if (sdpd->sdpd_name[j] == '_' && sdpd->sdpd_name[j + 1] == '_') {
				probeName[i] = '-';
				j += 2;
			} else {
				probeName[i] = sdpd->sdpd_name[j++];
			}
		}
		probeName[i] = '\0';

#if defined(__SCE__)
		sdt_probe_t *sdp = (sdt_probe_t *)kmem_zalloc(sizeof (sdt_probe_t), KM_SLEEP);
#else
		sdt_probe_t *sdp = kmem_zalloc(sizeof (sdt_probe_t), KM_SLEEP);
#endif
#if defined(__SCE__)
		sdp->sdp_loadcnt = 1; // we do not use
#else
		sdp->sdp_loadcnt = ctl->mod_loadcnt;
#endif
		sdp->sdp_primary = primary;
		sdp->sdp_ctl = ctl;
		sdp->sdp_name = probeName;
		sdp->sdp_namelen = probeNameLen;
		sdp->sdp_provider = prov;

		const char *funcName = NULL;
#if 0 // do not show function names for static probes (create a single probe for multiple entry points with the same name)
		if(allowInternal) {
			funcName = sdpd->sdpd_func;
		}
#endif

		/*
		 * We have our provider.  Now create the probe.
		 */
		dtrace_id_t id;
		if ((id = dtrace_probe_lookup(prov->sdtp_id, modName, funcName, probeName)) != DTRACE_IDNONE) {
#if defined(__SCE__)
			sdt_probe_t *old = (sdt_probe_t *)dtrace_probe_arg(prov->sdtp_id, id);
#else
			sdt_probe_t *old = dtrace_probe_arg(prov->sdtp_id, id);
#endif
			ASSERT(old != NULL);

			sdp->sdp_next = old->sdp_next;
			sdp->sdp_id = id;
			old->sdp_next = sdp;

			// save the probe id in the module's static description table
			*sdpd->sdpd_id = id;

		} else {
			sdp->sdp_id = dtrace_probe_create(prov->sdtp_id, modName, funcName, probeName, 1, sdp);

			// save the probe id in the module's static description table
			*sdpd->sdpd_id = sdp->sdp_id;

			ctl->sdt_nprobes++;
		}

		sdp->sdp_probedesc = sdpd; // save the probe description
		sdpd->sdpd_handler_fn->handler0 = NULL; // not enabled by default

		if(sdpd->sdpd_create_fn) { // call create helper function (if present)
			sdpd->sdpd_create_fn(sdpd);
		}
	}
}

/*ARGSUSED*/
static void
sdt_destroy(void *arg, dtrace_id_t id, void *parg)
{
	(void)arg;
	(void)id;

#if defined(__SCE__)
	sdt_probe_t *sdp = (sdt_probe_t *)parg, *old;
#else
	sdt_probe_t *sdp = parg, *old;
#endif
	struct modctl *ctl = sdp->sdp_ctl;

	DTRACE_LOG_DEBUG(2, "%s - probe: %s", __FUNCTION__, sdp->sdp_name);

#if defined(__SCE__)
	if (ctl->sdt_nprobes == 0) { SCE_PANIC(); }
	ctl->sdt_nprobes--;
#else
	if (ctl != NULL && ctl->mod_loadcnt == sdp->sdp_loadcnt) {
		if ((ctl->mod_loadcnt == sdp->sdp_loadcnt &&
		    ctl->mod_loaded) || sdp->sdp_primary) {
			ctl->sdt_nprobes--;
		}
	}
#endif
	while (sdp != NULL) {
		old = sdp;

		if(sdp->sdp_probedesc->sdpd_destroy_fn) { // call destroy helper function (if present)
			sdp->sdp_probedesc->sdpd_destroy_fn(sdp->sdp_probedesc);
		}

#if defined(__SCE__)
		kmem_free(sdp->sdp_name, (size_t)sdp->sdp_namelen);
#else
		kmem_free(sdp->sdp_name, sdp->sdp_namelen);
#endif
		sdp = sdp->sdp_next;
		kmem_free(old, sizeof (sdt_probe_t));
	}
}

/*ARGSUSED*/
static void
sdt_enable(void *arg, dtrace_id_t id, void *parg)
{
	(void)arg;
	(void)id;

#if defined(__SCE__)
	sdt_probe_t *sdp = (sdt_probe_t *)parg;
#else
	sdt_probe_t *sdp = parg;
#endif
#if !defined(__SCE__)
	struct modctl *ctl = sdp->sdp_ctl;
#endif

	DTRACE_LOG_DEBUG(2, "%s - probe: %s", __FUNCTION__, sdp->sdp_name);
#if !defined(__SCE__)
	ctl->mod_nenabled++;
#endif

	/*
	 * If this module has disappeared since we discovered its probes,
	 * refuse to enable it.
	 */
#if !defined(__SCE__)
	if (!sdp->sdp_primary && !ctl->mod_loaded) {
		if (sdt_verbose) {
			cmn_err(CE_NOTE, "sdt is failing for probe %s "
			    "(module %s unloaded)",
			    sdp->sdp_name, ctl->mod_modname);
		}
		goto err;
	}
#endif

	/*
	 * Now check that our modctl has the expected load count.  If it
	 * doesn't, this module must have been unloaded and reloaded -- and
	 * we're not going to touch it.
	 */
#if !defined(__SCE__)
	if (ctl->mod_loadcnt != sdp->sdp_loadcnt) {
		if (sdt_verbose) {
			cmn_err(CE_NOTE, "sdt is failing for probe %s "
			    "(module %s reloaded)",
			    sdp->sdp_name, ctl->mod_modname);
		}
		goto err;
	}
#endif

	while (sdp != NULL) {
		if(sdp->sdp_probedesc->sdpd_enable_fn) { // call enable helper function
			sdp->sdp_probedesc->sdpd_enable_fn(sdp->sdp_probedesc);
		}

#if defined(__SCE__)
		sdp->sdp_probedesc->sdpd_handler_fn->handler0 = (void (*)(const sdt_probedesc_t *))dtrace_probe_sdt_sce;
#else
		sdp->sdp_probedesc->sdpd_handler_fn->handler0 = (void (*)(const sdt_probedesc_t *))dtrace_probe_sdt;
#endif

		sdp = sdp->sdp_next;
	}

#if !defined(__SCE__)
err:
	;
#endif
}

/*ARGSUSED*/
static void
sdt_disable(void *arg, dtrace_id_t id, void *parg)
{
	(void)arg;
	(void)id;

#if defined(__SCE__)
	sdt_probe_t *sdp = (sdt_probe_t *)parg;
#else
	sdt_probe_t *sdp = parg;
#endif
	struct modctl *ctl = sdp->sdp_ctl;

	DTRACE_LOG_DEBUG(2, "%s - probe: %s", __FUNCTION__, sdp->sdp_name);

#if defined(__SCE__)
	(void)ctl;
#else
	ASSERT(ctl->mod_nenabled > 0);
	ctl->mod_nenabled--;
#endif
#if !defined(__SCE__)
	if ((!sdp->sdp_primary && !ctl->mod_loaded) ||
	    (ctl->mod_loadcnt != sdp->sdp_loadcnt))
		goto err;
#endif

	while (sdp != NULL) {
		sdp->sdp_probedesc->sdpd_handler_fn->handler0 = NULL;

		if(sdp->sdp_probedesc->sdpd_disable_fn) { // call disable helper function
			sdp->sdp_probedesc->sdpd_disable_fn(sdp->sdp_probedesc);
		}

		sdp = sdp->sdp_next;
	}
#if !defined(__SCE__)
err:
	;
#endif
}

// Framework-to-Provider API documented in dtrace.h
static dtrace_pops_t sdt_pops = {
	NULL,					// dtps_provide - provide all probes, all modules
	sdt_provide_module,		// dtps_provide_module - provide all probes in specified module
	sdt_enable,				// dtps_enable - enable specified probe
	sdt_disable,			// dtps_disable - disable specified probe
	NULL,					// dtps_suspend - suspend specified probe
	NULL,					// dtps_resume - resume specified probe
	sdt_getargdesc,			// dtps_getargdesc - get the argument description for args[X]
	NULL,					// dtps_getargval - get the value for an argX or args[X] variable
	NULL,					// dtps_usermode - find out if the probe was fired in user mode
	sdt_destroy				// dtps_destroy - destroy all state associated with this probe
};

int
sdt_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	sdt_provider_t *prov;

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	DTRACE_LOG_CALL(2);

	(void)devi;
	(void)cmd;

	for (prov = sdt_providers; prov->sdtp_name != NULL; prov++) {
		if (dtrace_register(prov->sdtp_name, prov->sdtp_attr,
		    DTRACE_PRIV_KERNEL, NULL,
		    &sdt_pops, prov, &prov->sdtp_id) != 0) {
			cmn_err(CE_WARN, "failed to register sdt provider %s",
			    prov->sdtp_name);
		}
	}

	return (DDI_SUCCESS);
}

int
sdt_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	(void)devi;

	sdt_provider_t *prov;

	switch (cmd) {
	case DDI_DETACH:
		break;
	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}

	DTRACE_LOG_CALL(2);

	for (prov = sdt_providers; prov->sdtp_name != NULL; prov++) {
		if (prov->sdtp_id != DTRACE_PROVNONE) {
			if (dtrace_unregister(prov->sdtp_id) != 0)
				return (DDI_FAILURE);
			prov->sdtp_id = DTRACE_PROVNONE;
		}
	}

	return (DDI_SUCCESS);
}

int
sdt_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
	(void)devp;
	(void)flag;
	(void)otyp;
	(void)cred_p;

	return (0);
}

int
sdt_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
	(void)dev;
	(void)flag;
	(void)otyp;
	(void)cred_p;

	return (0);
}
