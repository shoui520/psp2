#ifndef MARLIN_HCI_FILE_H
#define MARLIN_HCI_FILE_H

/* The original allocation holds an 8 KiB cache plus 64-byte alignment slack. */
typedef struct MarlinFileContext {
	int mode;
	int descriptor;
	int position;
	int cachedBlock;
	int cachedSize;
	unsigned char *buffer;
	unsigned char storage[8192 + 64];
} MarlinFileContext;

int MarlinFileRemove(void *allocator, const char *path);
int MarlinFileOpen(void *allocator, const char *path, int mode,
		   MarlinFileContext **file);
int MarlinFileClose(void *allocator, MarlinFileContext *file);
int MarlinFileRead(void *allocator, MarlinFileContext *file,
		   unsigned int size, unsigned int *readSize, void *data);
int MarlinFileWrite(void *allocator, MarlinFileContext *file,
		    unsigned int size, const void *data);
int MarlinFileGetSize(void *allocator, MarlinFileContext *file,
		      unsigned int *size);

#endif
