#ifndef PAF_FILE_LOCALFILE_PLATFORM_H
#define PAF_FILE_LOCALFILE_PLATFORM_H

typedef struct LocalFilePsp2Impl LocalFilePsp2Impl;

/* Allocation extent of the Vita backend; its fields belong to that backend. */
enum { LocalFilePsp2ImplSize = 48 };

#define LocalFilePsp2ImplConstruct ScePafMisc_E73FE3E5

LocalFilePsp2Impl *LocalFilePsp2ImplConstruct(LocalFilePsp2Impl *self);

#endif
