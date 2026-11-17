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

#ifdef __cplusplus
}
#endif

#endif
