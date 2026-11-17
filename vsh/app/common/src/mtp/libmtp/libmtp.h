/*
 * libmtp.h: SceLibMtp's exports the shell uses (link names SceLibMtp_<NID>: no Sony name is
 * known for them). Names ours.
 */
#ifndef _LIBMTP_LIBMTP_H
#define _LIBMTP_LIBMTP_H

#ifdef __cplusplus
extern "C" {
#endif

/* the host's functions libmtp calls, six of them (their meaning is the shell's) */
typedef struct SceLibMtpHostFuncs {
	int (*func[6])(int arg);
} SceLibMtpHostFuncs;

/* registers the host's functions */
int SceLibMtp_67DB4C13(const SceLibMtpHostFuncs *funcs);

#ifdef __cplusplus
}
#endif

#endif
