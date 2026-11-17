/*
 * livespace_db.h: SceLsdb's exports (livespace_db.suprx), as their callers use them. No Sony
 * name is known for them: link names SceLsdb_<NID>.
 */
#ifndef _LIVESPACE_DB_H
#define _LIVESPACE_DB_H

#ifdef __cplusplus
extern "C" {
#endif

/* returns a livespace_db error when its trigger table cannot be used (0x801010F1, ...) */
int SceLsdb_B0E42B67(void);

/* passes value to the new-event daemon (shell gives 60, or -60) and returns its result */
int SceLsdb_2944E643(int value);

/* registers the callback livespace_db calls back into the shell with */
void SceLsdb_AADCDCFC(void *callback);

/* reads a value of a result item (a title ID) into out */
int SceLsdb_EF880EFE(const void *item, void *out);

#ifdef __cplusplus
}
#endif

#endif
