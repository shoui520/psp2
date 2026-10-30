/*
 * DTrace buffer bookmarks (Sony's dtrace/sce/dtrace_bookmark.c).
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

struct dtrace_buffer;

int dtrace_bookmark_init(void);
int dtrace_bookmark_fini(void);
void dtrace_buffer_flush_bookmarks(struct dtrace_buffer *buf);
