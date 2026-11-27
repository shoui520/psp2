/*
 * rsa_snc_compat.h: SNC C mode compatibility shim for C11 _Static_assert
 * used in marlin_hci/rsa.h when building shared RSA sources with SNC.
 */
#ifndef SHELL_RSA_SNC_COMPAT_H
#define SHELL_RSA_SNC_COMPAT_H

#ifndef _Static_assert
#define _Static_assert(cond, msg)
#endif

#endif /* SHELL_RSA_SNC_COMPAT_H */
