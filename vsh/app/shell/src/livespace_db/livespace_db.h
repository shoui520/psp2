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

/*
 * A title query: a handle on the application database for FindTitles-style lookups.
 * SceLsdb_A6D8772C constructs it (no handle), SceLsdb_E90B81CD opens it (an error, or 0),
 * SceLsdb_76B75713 runs it: conditions (a key hash followed by its values) select the titles,
 * up to count, into titleIds (a vector of paf::string). SceLsdb_E2101B03 closes and destroys
 * it.
 */
typedef struct LsdbTitleQuery {
	void *impl;
} LsdbTitleQuery;

LsdbTitleQuery *SceLsdb_A6D8772C(LsdbTitleQuery *query);
int SceLsdb_E90B81CD(LsdbTitleQuery *query);
int SceLsdb_76B75713(LsdbTitleQuery *query, const void *conditions, void *titleIds, int count);
LsdbTitleQuery *SceLsdb_E2101B03(LsdbTitleQuery *query);

/* reads a value of a result item (a title ID) into out */
int SceLsdb_EF880EFE(const void *item, void *out);

#ifdef __cplusplus
}
#endif

#endif
