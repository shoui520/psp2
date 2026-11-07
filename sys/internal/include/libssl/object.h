/* Filename is ours; recovered Sony export link names are retained. */
#ifndef LIBSSL_OBJECT_H
#define LIBSSL_OBJECT_H

#include <scetypes.h>

int SceSslInternal_144FA354(void *certificate);
int SceSslInternal_46358909(void *key);
int SceSslInternal_F030A0DE(void *extension);
int SceSslInternal_4D80D3F7(void *oid, int arg1, const void *data, SceSize len);
int SceSslInternal_3DE4F3DB(void *oid, int arg1, SceSize size, char *buf);
int SceSslInternal_64BDF1BB(void *cert, int item, void *ext);
int SceSslInternal_306E2A22(void *certificate, void *key, void *cryptoContext);
int SceSslInternal_29AAC768(void *lib, int arg1, int format, SceSize size,
			    const void *data, void **end, void **cert);
int SceSslInternal_170D595D(void *certificate, int mode, void **result);
int SceSslInternal_D68D8421(void *cert, void *key);
int SceSslInternal_08ED1B25(void *certificate, int algorithm,
			    SceUInt32 capacity, void *digest,
			    SceUInt32 *digestLength);
int SceSslInternal_2074A6EB(void *cert, void *date);
int SceSslInternal_2464F9D0(void *cert, void *date);

#endif
