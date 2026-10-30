/*
 * Solaris credentials (Sony's dtrace/sce/solaris_cred.c): there is one,
 * the kernel's, and it holds every privilege.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

typedef struct cred {
	uid_t cr_uid;
	uid_t cr_ruid;
	uid_t cr_suid;
	gid_t cr_gid;
	gid_t cr_rgid;
	gid_t cr_sgid;
} cred_t;

#define PRIV_ALL		(-1)
#define PRIV_DTRACE_KERNEL	1
#define PRIV_DTRACE_PROC	2
#define PRIV_DTRACE_USER	3
#define PRIV_PROC_OWNER		4
#define PRIV_PROC_ZONE		5

int priv_policy_choice(const cred_t *cr, int priv, boolean_t allzone);
int priv_policy_only(const cred_t *cr, int priv, boolean_t allzone);
cred_t *CRED(void);
uid_t crgetuid(const cred_t *cr);

#define PRIV_POLICY_CHOICE(cr, priv, allzone)	priv_policy_choice((cr), (priv), (allzone))
#define PRIV_POLICY_ONLY(cr, priv, allzone)	priv_policy_only((cr), (priv), (allzone))

#define crhold(cr)		((void)0)
#define crfree(cr)		((void)0)
#define crgetzoneid(cr)		((zoneid_t)0)
#define getzoneid()		((zoneid_t)0)
