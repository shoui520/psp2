/*
 * Solaris credentials (Sony's dtrace/sce/solaris_cred.c): one credential for
 * every caller, which holds every privilege.
 */
#include <dtrace/sce/solaris_cred.h>

static cred_t s_cred;

int priv_policy_only(const cred_t *cr, int priv, boolean_t allzone)
{
	return 1;
}

int priv_policy_choice(const cred_t *cr, int priv, boolean_t allzone)
{
	return 1;
}

cred_t *CRED(void)
{
	return &s_cred;
}

uid_t crgetuid(const cred_t *cr)
{
	return cr->cr_uid;
}
