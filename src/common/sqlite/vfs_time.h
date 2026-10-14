#ifndef SCE_SQLITE_VFS_TIME_H
#define SCE_SQLITE_VFS_TIME_H

struct sqlite3_vfs;
int psp2Randomness(struct sqlite3_vfs *vfs, int size, char *buffer);
int psp2Sleep(struct sqlite3_vfs *vfs, int microseconds);
int psp2CurrentTime(struct sqlite3_vfs *vfs, double *time);

#endif
