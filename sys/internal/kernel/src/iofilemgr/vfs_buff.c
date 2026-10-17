/*
 * SceIofilemgr: the per-vnode buffer cache (Sony's vfs_buff.c): an I/O buffer
 * in front of the file system and a set-associative data cache of lines.
 */

#include "iofilemgr_internal.h"

void *vfs_buf_allocate_memory(SceUID id, SceSize size, int memType)
{
	switch (memType & 7) {
	case VFS_BUFF_MEM_HEAP:
		return SceSysmemForDriver_7B4CB60A(id, size);
	case VFS_BUFF_MEM_DEFAULT:
	case VFS_BUFF_MEM_HOST:
		return SceSysrootForKernel_D351EBC8(id, size, NULL);
	case VFS_BUFF_MEM_CDIALOG:
		return sceKernelAllocRemoteCDialogHeap(id, size, NULL);
	default:
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 96, "unexpected flag value = %d\n", memType);
		return NULL;
	}
}

int vfs_buf_free_memory(SceUID id, void *ptr, int memType)
{
	switch (memType & 7) {
	case VFS_BUFF_MEM_HEAP:
		return sceKernelFreeHeapMemory(id, ptr);
	case VFS_BUFF_MEM_DEFAULT:
	case VFS_BUFF_MEM_HOST:
		return SceSysrootForKernel_7968F091(id, ptr);
	case VFS_BUFF_MEM_CDIALOG:
		return sceKernelFreeRemoteCDialogHeap(id, ptr);
	default:
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 130, "unexpected flag value = %d\n", memType);
		return SCE_ERROR_ERRNO_EINVAL;
	}
}

int vfsBuffDataRead(struct BuffCache *bc, SceVfsVnode *vp, SceOff pos, SceSize size, void *buf)
{
	SceSize lineSize = bc->lineSize;
	SceOff lineOffset = pos & ~(SceOff)(lineSize - 1);
	int set = (pos / lineSize) % bc->setNum;
	struct BuffLine **head = &bc->lineList[set];
	int wayNum = bc->wayNum;
	SceSize inLine = pos % lineSize;
	struct BuffLine *line, *prev, *next;
	int i;

	line = *head;
	if (inLine + size > lineSize)
		return SCE_ERROR_ERRNO_EINVAL;
	if (wayNum == 0)
		return SCE_ERROR_ERRNO_ENOENT;
	if (line->offset != lineOffset) {
		line = *head;
		i = 0;
		do {
			i++;
			line = line->next;
			if (i >= wayNum)
				return SCE_ERROR_ERRNO_ENOENT;
		} while (line->offset != lineOffset);
	}
	if (line->state >= VFS_BUFF_LINE_INVALID) {
		if (line->state != VFS_BUFF_LINE_FREE && line->state != VFS_BUFF_LINE_INVALID)
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 640,
					     "found cache entry is not valid ... %p state=0x%x off=0x%llx\n",
					     line, line->state, line->offset);
		return SCE_ERROR_ERRNO_ENOENT;
	}
	/* move the line to the front of its set */
	if (*head == line) {
		*head = line->next;
	} else {
		next = *head;
		do {
			prev = next;
			next = prev->next;
		} while (next != line && next != NULL);
		prev->next = line->next;
	}
	line->next = NULL;
	memcpy(buf, (char *)line->data + inLine, size);
	if (*head != NULL) {
		line->next = *head;
		*head = line;
		return 0;
	}
	*head = line;
	return 0;
}

int vfsBuffDataWrite(struct BuffCache *bc, SceVfsVnode *vp, SceOff pos, SceSize size,
		     const void *buf)
{
	SceSize lineSize = bc->lineSize;
	SceOff lineOffset = pos & ~(SceOff)(lineSize - 1);
	int set = (lineOffset / lineSize) % bc->setNum;
	struct BuffLine **head = &bc->lineList[set];
	int wayNum = bc->wayNum;
	SceSize inLine = pos % lineSize;
	struct BuffLine *line, *prev, *next;
	int i;

	line = *head;
	if (inLine + size > lineSize)
		return SCE_ERROR_ERRNO_EINVAL;
	for (i = 0; i < wayNum; i++) {
		if (line->offset == lineOffset)
			break;
		line = line->next;
	}
	if (i == wayNum)
		return SCE_ERROR_ERRNO_ENOENT;
	if (line->state >= VFS_BUFF_LINE_INVALID) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 707, "found cache entry is not valid ... %p state=0x%x\n",
				     line, line->state);
		return SCE_ERROR_ERRNO_ENOENT;
	}
	if (*head == line) {
		*head = line->next;
	} else {
		next = *head;
		do {
			prev = next;
			next = prev->next;
		} while (next != line && next != NULL);
		prev->next = line->next;
	}
	line->next = NULL;
	memcpy((char *)line->data + inLine, buf, size);
	line->state |= VFS_BUFF_LINE_DIRTY;
	if (*head != NULL) {
		line->next = *head;
		*head = line;
		return 0;
	}
	*head = line;
	return 0;
}

/* name unknown: does [pos, pos + size) lie in one cache line */
int vfs_buf_in_one_line(struct BuffCache *bc, SceOff pos, SceSize size)
{
	SceOff mask = ~(SceOff)(bc->lineSize - 1);

	if (size > bc->lineSize)
		return 0;
	return (pos & mask) == ((pos + size - 1) & mask);
}

/* name unknown: does the I/O buffer's cached part hold all of [pos, pos + size) */
int vfs_buf_io_cached_contains(struct BuffCache *bc, SceOff pos, SceSize size)
{
	return bc->ioBuffState < VFS_BUFF_LINE_INVALID && pos >= bc->ioBuffOffset &&
	       bc->ioBuffOffset + bc->ioBuffCachedSize >= pos + size;
}

/* name unknown: does either end of [pos, pos + size] fall in the I/O buffer's cached part */
int vfs_buf_io_cached_overlaps(struct BuffCache *bc, SceOff pos, SceSize size)
{
	if (bc->ioBuffState >= VFS_BUFF_LINE_INVALID)
		return 0;
	if (pos >= bc->ioBuffOffset && bc->ioBuffOffset + bc->ioBuffCachedSize >= pos)
		return 1;
	return pos + size >= bc->ioBuffOffset &&
	       bc->ioBuffOffset + bc->ioBuffCachedSize >= pos + size;
}

/* name unknown: does the I/O buffer hold all of [pos, pos + size) */
int vfs_buf_io_buff_contains(struct BuffCache *bc, SceOff pos, SceSize size)
{
	return bc->ioBuffState < VFS_BUFF_LINE_INVALID && pos >= bc->ioBuffOffset &&
	       bc->ioBuffOffset + bc->ioBuffSize >= pos + size;
}

/*
 * Copy into the I/O buffer. grow 0: inside what is cached already; otherwise
 * the cached part grows by grow bytes (-1: to the whole buffer) and the
 * vnode's size follows it.
 */
int vfsBuffIoWrite(struct BuffCache *bc, SceOff pos, SceSize size, const void *buf,
		   SceVfsVnode *vp, SceSize grow)
{
	SceOff end;

	if (bc->ioBuffState >= VFS_BUFF_LINE_INVALID)
		return SCE_ERROR_ERRNO_EACCES;
	if (grow == 0) {
		if (pos < bc->ioBuffOffset ||
		    bc->ioBuffOffset + bc->ioBuffCachedSize < pos + size)
			return SCE_ERROR_ERRNO_EINVAL;
	} else {
		if (bc->ioBuffCachedSize == 0) {
			bc->ioBuffState = VFS_BUFF_LINE_INVALID;
			return SCE_ERROR_ERRNO_ENOSPC;
		}
		if (pos < bc->ioBuffOffset || bc->ioBuffOffset + bc->ioBuffSize < pos + size) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1249,
					     "bc->ioBuffOffset=0x%llx, pos=0x%llx, size=0x%x, bc->ioBuffSize=0x%x\n",
					     bc->ioBuffOffset, pos, size, bc->ioBuffSize);
			return SCE_ERROR_ERRNO_EINVAL;
		}
		if (grow == (SceSize)-1) {
			bc->ioBuffCachedSize = bc->ioBuffSize;
			SCE_DBG_ASSERT(bc->ioBuffCachedSize <= bc->ioBuffSize, 1255);
		} else {
			bc->ioBuffCachedSize += grow;
			if (bc->ioBuffCachedSize > bc->ioBuffSize) {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 1263,
						     "bc->ioBuffCachedSize=0x%x iooffset=0x%llx extend_size=0x%x\n",
						     bc->ioBuffCachedSize, bc->ioBuffOffset, grow);
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 1265,
						     "vp->size=0x%llx pos=0x%llx size=0x%x extend_size=0x%x\n",
						     vp->core.size, pos, size, grow);
				SCE_DBG_ASSERT(0, 1266);
			}
		}
		end = bc->ioBuffOffset + bc->ioBuffCachedSize;
		if ((SceOff)vp->core.size < end)
			vp->core.size = end;
	}
	memcpy((char *)bc->ioBuff + (SceSize)(pos - bc->ioBuffOffset), buf, size);
	bc->ioBuffState |= VFS_BUFF_LINE_DIRTY;
	return 0;
}

/* write a dirty line back: through the I/O buffer when it holds the line */
int vfs_bop_flush_data_cache(struct BuffCache *bc, struct BuffLine *line, SceVfsVnode *vp,
			     SceVfsFile *file)
{
	SceOff offset = line->offset;
	const struct VfsBuffIoOps *ioOps = bc->ioOps;
	SceSize lineSize = bc->lineSize;
	SceVopTable *ops;
	SceUInt32 flags;
	SceOff position;
	int ret;

	if (bc->ioBuffState < VFS_BUFF_LINE_INVALID && offset >= bc->ioBuffOffset &&
	    bc->ioBuffOffset + bc->ioBuffCachedSize >= offset + lineSize) {
		ret = vfsBuffIoWrite(bc, offset, lineSize, line->data, vp, 0);
		if (ret >= 0)
			return 0;
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1092, "retval = %d 0x%x\n", ret, ret);
	}

	if (ioOps != NULL && ioOps->write != NULL) {
		VfsBuffIoWriteArgs args;

		args.offset = offset;
		args.data = line->data;
		args.size = lineSize;
		args.vp = vp;
		ret = ioOps->write(&args);
	} else {
		ops = vp->core.ops;
		if (ops->vop_write != NULL && file != NULL) {
			SceVopWriteArgs args;

			args.vp = vp;
			args.file = file;
			args.buf = line->data;
			args.nbyte = lineSize;
			flags = file->flags;
			position = file->position;
			file->flags = flags | 3;
			file->position = offset;
			ret = ops->vop_write(&args);
			file->position = position;
			file->flags = (file->flags & ~3) | (flags & 3);
			if (ret == args.nbyte) {
				ret = 0;
			} else {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1142,
						     "wbyte(0x%x) != args.nbyte(0x%x)\n", ret,
						     args.nbyte);
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1144, "off=0x%llx file->fpos=0x%llx\n",
						     offset, position);
			}
		} else {
			ret = SCE_ERROR_ERRNO_EUNSUP;
		}
	}
	return ret;
}

/* apply a VFS_BUFF_OP_* request to the cached line holding pos */
int vfs_buf_op_subr(struct BuffCache *bc, SceOff pos, SceVfsVnode *vp, SceVfsFile *file,
		    int mode)
{
	SceSize lineSize = bc->lineSize;
	SceOff lineOffset = pos & ~(SceOff)(lineSize - 1);
	int set = (pos / lineSize) % bc->setNum;
	struct BuffLine *line;
	int ret;

	for (line = bc->lineList[set]; line != NULL; line = line->next) {
		if (line->offset != lineOffset ||
		    line->state >= VFS_BUFF_LINE_INVALID)
			continue;
		switch (mode) {
		case VFS_BUFF_OP_LOOKUP:
			return 1;
		case VFS_BUFF_OP_FLUSH:
			if (line->state & VFS_BUFF_LINE_DIRTY) {
				ret = vfs_bop_flush_data_cache(bc, line, vp, file);
				if (ret < 0) {
					SCE_DBG_PRINTF_LEVEL(2, 0xf, 784,
							     "vfs_bop_flush_data_cache() failed %d 0x%x offset=%lld 0x%llx dataSize=%d 0x%x\n",
							     ret, ret, line->offset, line->offset,
							     bc->lineSize, bc->lineSize);
					return ret;
				}
				line->state &= ~VFS_BUFF_LINE_DIRTY;
			}
			line->state |= VFS_BUFF_LINE_FLUSHED;
			return 0;
		case VFS_BUFF_OP_HOLD:
			line->state |= VFS_BUFF_LINE_HELD;
			return 0;
		case VFS_BUFF_OP_RELEASE:
			line->state &= ~VFS_BUFF_LINE_HELD;
			return 0;
		case VFS_BUFF_OP_IS_HELD:
			if (line->state & VFS_BUFF_LINE_HELD)
				return 1;
			break;
		case VFS_BUFF_OP_INVALIDATE:
			line->state = VFS_BUFF_LINE_INVALID;
			break;
		default:
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 816, "invalide mode is specified 0x%x\n", mode);
			return SCE_ERROR_ERRNO_EINVAL;
		}
	}
	return 0;
}

/* name unknown: is the line holding pos cached */
int vfs_buf_data_cached(struct BuffCache *bc, SceOff pos)
{
	return vfs_buf_op_subr(bc, pos, NULL, NULL, VFS_BUFF_OP_LOOKUP);
}

/* write back the line holding pos if it is dirty */
int vfsBuffDataFlush(struct BuffCache *bc, SceOff pos, SceSize size, SceVfsVnode *vp,
		     SceVfsFile *file)
{
	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 730);
	return vfs_buf_op_subr(bc, pos, vp, file, VFS_BUFF_OP_FLUSH);
}

/* apply a VFS_BUFF_OP_* request to every cached line in [start, end] */
int vfs_buf_area_op_subr(struct BuffCache *bc, SceOff start, SceOff end, SceVfsVnode *vp,
			 SceVfsFile *file, int mode)
{
	int setNum = bc->setNum;
	SceOff lineSize = bc->lineSize;
	int startSet, endSet, lastSet, wrap, set, ret;
	struct BuffLine *line;

	if (start >= end) {
		if (start != end)
			return SCE_ERROR_ERRNO_EINVAL;
		return 0;
	}

	startSet = (start / lineSize) % setNum;
	endSet = (end / lineSize) % setNum;
	start &= -lineSize;
	end &= -lineSize;
	lastSet = setNum;
	if ((int)((end - start) / lineSize + 1) < setNum) {
		if (endSet < startSet) {
			wrap = 1;
		} else {
			wrap = 0;
			lastSet = endSet + 1;
		}
	} else {
		if (startSet == 0)
			endSet = -1;
		wrap = startSet != 0;
	}

	for (set = startSet; set < lastSet; set++) {
		for (line = bc->lineList[set]; line != NULL; line = line->next) {
			if (line->offset < start || end < line->offset ||
			    line->state >= VFS_BUFF_LINE_INVALID)
				continue;
			switch (mode) {
			case VFS_BUFF_OP_FLUSH:
				if (line->state & VFS_BUFF_LINE_DIRTY) {
					ret = vfs_bop_flush_data_cache(bc, line, vp, file);
					if (ret < 0) {
						SCE_DBG_PRINTF_LEVEL(2, 0xf, 895,
								     "vfs_bop_flush_data_cache() failed %d 0x%x offset=%lld 0x%llx dataSize=%d 0x%x\n",
								     ret, ret, line->offset, line->offset,
								     bc->lineSize, bc->lineSize);
						SCE_DBG_PRINTF_LEVEL(1, 0xf, 896, "\n");
						return ret;
					}
					line->state &= ~VFS_BUFF_LINE_DIRTY;
				}
				line->state |= VFS_BUFF_LINE_FLUSHED;
				break;
			case VFS_BUFF_OP_INVALIDATE:
				line->state = VFS_BUFF_LINE_INVALID;
				break;
			case VFS_BUFF_OP_LOOKUP:
				return 1;
			default:
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 911, "invalide mode is specified 0x%x\n",
						     mode);
				return SCE_ERROR_ERRNO_EINVAL;
			}
		}
	}

	if (!wrap || endSet < 0)
		return 0;

	/* the range wrapped around to the first sets */
	for (set = 0; set <= endSet; set++) {
		for (line = bc->lineList[set]; line != NULL; line = line->next) {
			if (line->offset < start || end < line->offset ||
			    line->state >= VFS_BUFF_LINE_INVALID)
				continue;
			switch (mode) {
			case VFS_BUFF_OP_FLUSH:
				if (line->state & VFS_BUFF_LINE_DIRTY) {
					ret = vfs_bop_flush_data_cache(bc, line, vp, file);
					if (ret < 0) {
						SCE_DBG_PRINTF_LEVEL(2, 0xf, 939,
								     "vfs_bop_flush_data_cache() failed %d 0x%x offset=%lld 0x%llx dataSize=%d 0x%x\n",
								     ret, ret, line->offset, line->offset,
								     bc->lineSize, bc->lineSize);
						SCE_DBG_PRINTF_LEVEL(1, 0xf, 940, "\n");
						return ret;
					}
					line->state &= ~VFS_BUFF_LINE_DIRTY;
				}
				line->state |= VFS_BUFF_LINE_FLUSHED;
				break;
			case VFS_BUFF_OP_INVALIDATE:
				line->state = VFS_BUFF_LINE_INVALID;
				break;
			case VFS_BUFF_OP_LOOKUP:
				return 1;
			default:
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 954, "invalide mode is specified 0x%x\n",
						     mode);
				return SCE_ERROR_ERRNO_EINVAL;
			}
		}
	}
	return 0;
}

/* name unknown: is any line in [start, end] cached */
int vfs_buf_area_cached(struct BuffCache *bc, SceOff start, SceOff end)
{
	return vfs_buf_area_op_subr(bc, start, end, NULL, NULL, VFS_BUFF_OP_LOOKUP);
}

/* write back the dirty lines in [start, end] */
int vfsBuffDataFlushArea(struct BuffCache *bc, SceOff start, SceOff end, SceSize size,
			 SceVfsVnode *vp, SceVfsFile *file)
{
	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 740);
	return vfs_buf_area_op_subr(bc, start, end, vp, file, VFS_BUFF_OP_FLUSH);
}

/* name unknown: drop the lines in [start, end] */
int vfs_buf_area_invalidate(struct BuffCache *bc, SceOff start, SceOff end, SceVfsVnode *vp)
{
	return vfs_buf_area_op_subr(bc, start, end, vp, NULL, VFS_BUFF_OP_INVALIDATE);
}

/* put a line of data at pos into the cache, replacing an unused or the oldest line */
int vfsBuffDataEnter(struct BuffCache *bc, SceOff pos, SceSize size, const void *data, int state,
		     SceVfsVnode *vp, SceVfsFile *file)
{
	SceSize lineSize = bc->lineSize;
	int set = (pos / lineSize) % bc->setNum;
	struct BuffLine **head = &bc->lineList[set];
	int wayNum = bc->wayNum;
	struct BuffLine *line = *head, *prev, *next;
	int ret, i;

	if (pos % lineSize != 0 || size != lineSize)
		return SCE_ERROR_ERRNO_EINVAL;

	for (;;) {
		if (wayNum > 0) {
			line = *head;
			if (line->state < VFS_BUFF_LINE_INVALID) {
				if (line->offset == pos)
					return SCE_ERROR_ERRNO_EEXIST;
				i = 0;
				for (;;) {
					next = line->next;
					if (++i == wayNum)
						break;
					line = next;
					if (line->state >= VFS_BUFF_LINE_INVALID)
						break;
					if (line->offset == pos)
						return SCE_ERROR_ERRNO_EEXIST;
				}
			}
		}
		if (!(line->state & VFS_BUFF_LINE_HELD))
			break;
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 515, "locked offset is %lld 0x%llx\n", line->offset,
				     line->offset);
		if (--wayNum == 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 518,
					     "all buff cache entry is locked, could not replace\n");
			return SCE_ERROR_ERRNO_EBUSY;
		}
	}

	if (line->state & VFS_BUFF_LINE_DIRTY) {
		ret = vfs_bop_flush_data_cache(bc, line, vp, file);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 531,
					     "failed %d 0x%x offset=%lld 0x%llx dataSize=%d 0x%x\n", ret,
					     ret, line->offset, line->offset, bc->lineSize, bc->lineSize);
			return ret;
		}
	}
	if (*head == NULL) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 540, "unexpected error occurs ... %d 0x%x\n",
				     SCE_ERROR_ERRNO_ENOENT, SCE_ERROR_ERRNO_ENOENT);
		return SCE_ERROR_ERRNO_ENOENT;
	}

	if (*head == line) {
		*head = line->next;
	} else {
		next = *head;
		do {
			prev = next;
			next = prev->next;
		} while (next != line && next != NULL);
		prev->next = line->next;
	}
	line->next = NULL;
	memcpy(line->data, data, size);
	line->offset = pos;
	line->state = state;
	if (*head != NULL)
		line->next = *head;
	*head = line;
	return 0;
}

/* apply a request to every cached line */
static int vfs_buf_all_op_subr(struct BuffCache *bc, SceVfsVnode *vp, SceVfsFile *file,
			       int mode)
{
	struct BuffLine *line;
	int set, way, ret;

	for (set = 0; set < bc->setNum; set++) {
		line = bc->lineList[set];
		for (way = 0; way < bc->wayNum; way++) {
			if (line->state < VFS_BUFF_LINE_INVALID) {
				switch (mode) {
				case VFS_BUFF_OP_FLUSH:
					if (line->state & VFS_BUFF_LINE_DIRTY) {
						ret = vfs_bop_flush_data_cache(bc, line, vp, file);
						if (ret < 0) {
							SCE_DBG_PRINTF_LEVEL(2, 0xf, 999,
									     "vfs_bop_flush_data_cache() failed %d 0x%x offset=%lld 0x%llx dataSize=%d 0x%x\n",
									     ret, ret, line->offset,
									     line->offset, bc->lineSize,
									     bc->lineSize);
							return ret;
						}
						line->state &= ~VFS_BUFF_LINE_DIRTY;
					}
					line->state |= VFS_BUFF_LINE_FLUSHED;
					break;
				case VFS_BUFF_OP_INVALIDATE:
					line->state = VFS_BUFF_LINE_INVALID;
					break;
				default:
					return SCE_ERROR_ERRNO_EINVAL;
				}
			}
			line = line->next;
		}
	}
	return 0;
}

/* write back every dirty line */
int vfsBuffDataFlushAll(struct BuffCache *bc, SceVfsVnode *vp, SceVfsFile *file)
{
	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 750);
	return vfs_buf_all_op_subr(bc, vp, file, VFS_BUFF_OP_FLUSH);
}

/* free the data cache: its lines and the line headers (the lowest one starts their block) */
int vfs_buf_free_data_buff(struct BuffCache *bc, SceVfsVnode *vp, SceVfsFile *file)
{
	struct BuffLine *line = bc->lineList[0];
	struct BuffLine *lines = line;
	SceUID pid = bc->pid;
	int set, way, ret;

	if (bc->setNum != 0) {
		for (set = 0;;) {
			for (way = 0; way < bc->wayNum; way++) {
				if (line <= lines)
					lines = line;
				line = line->next;
			}
			if (++set >= bc->setNum)
				break;
			line = bc->lineList[set];
		}
	}

	ret = vfs_buf_free_memory(bc->pid, bc->dataBuffMemory, bc->memType);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 358,
				     "vfs_buf_free_memory failed ... %d 0x%x free address is %p\n", ret,
				     ret, bc->dataBuffMemory);
	ret = vfs_buf_free_memory(pid, lines, bc->memType);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 365,
				     "vfs_buf_free_memory failed ... %d 0x%x free address is %p\n", ret,
				     ret, lines);
	return 0;
}

/* allocate a buffer cache: setNum sets of wayNum lines, and an I/O buffer */
int vfsBuffAllocate(SceSize dataCacheSize, int wayNum, SceSize lineSize,
		    SceSize ioCacheSize, SceUID pid, SceUInt16 memType,
		    const struct VfsBuffIoOps *ioOps, struct BuffCache **bcp)
{
	struct BuffCache *bc;
	struct BuffLine *lines, *line;
	SceUInt32 lineNum;
	int setNum, set, way;
	char *dataMemory, *data, *lineData, *ioBuff;

	if (dataCacheSize == 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 153, "\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (wayNum != 1 && wayNum != 2 && wayNum != 4) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 158, "\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (dataCacheSize % (lineSize * wayNum) != 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 164, "\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (ioCacheSize & 0x1ff) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 170, "unexpected IO buffer size = 0x%x", ioCacheSize);
		return SCE_ERROR_ERRNO_EINVAL;
	}

	lineNum = dataCacheSize / lineSize;
	setNum = (SceUInt8)((SceUInt8)lineNum / (SceUInt32)wayNum);
	bc = vfs_buf_allocate_memory(pid, sizeof(*bc) + setNum * sizeof(bc->lineList[0]), memType);
	if (bc == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	bc->wayNum = wayNum;
	bc->memType = memType;
	bc->lineSize = lineSize;
	bc->setNum = setNum;
	bc->pid = pid;
	bc->hostBuff = NULL;
	bc->ioOps = ioOps;
	bc->hostMemBlock = 0;

	lines = vfs_buf_allocate_memory(pid, lineNum * sizeof(*lines), memType);
	if (lines == NULL)
		goto free_bc;
	dataMemory = vfs_buf_allocate_memory(pid, dataCacheSize + 0x200, memType);
	bc->dataBuffMemory = dataMemory;
	if (dataMemory == NULL) {
		vfs_buf_free_memory(pid, bc, memType);
		vfs_buf_free_memory(pid, lines, memType);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	data = (char *)(((SceUIntPtr)dataMemory + 0x1ff) & ~0x1ff);
	memset(data, 0, dataCacheSize);
	memset(lines, 0, lineNum * sizeof(*lines));

	for (set = 0; set < setNum; set++) {
		bc->lineList[set] = NULL;
		if (wayNum == 0)
			continue;
		line = lines;
		lineData = data;
		for (way = 0; way < wayNum; way++) {
			if (bc->lineList[set] != NULL)
				line->next = bc->lineList[set];
			bc->lineList[set] = line;
			line->data = lineData;
			line->offset = -1;
			line->state = VFS_BUFF_LINE_FREE;
			lineData += lineSize;
			line++;
		}
		lines += wayNum;
		data += lineSize * wayNum;
	}

	if (memType == VFS_BUFF_MEM_HOST && SceDipswForDriver_A98FC2FD(0xd2)) {
		bc->hostMemBlock = SceSysmemForDriver_C94850C9("SceIoBuffCacheForHost", 0x10f0d006,
							   ioCacheSize, NULL);
		if (bc->hostMemBlock < 0)
			goto free_data;
		SceSysmemForDriver_A841EDDA(bc->hostMemBlock, &bc->ioBuffMemory);
		bc->hostBuff = vfs_buf_allocate_memory(pid, 0x8200, VFS_BUFF_MEM_HOST);
		if (bc->hostBuff == NULL) {
			vfs_buf_free_data_buff(bc, NULL, NULL);
			SceSysmemForDriver_009E1C61(bc->hostMemBlock);
			goto free_bc;
		}
		ioBuff = (char *)(((SceUIntPtr)bc->ioBuffMemory + 0x1ff) & ~0x1ff);
		memset(ioBuff, 0, ioCacheSize);
	} else {
		bc->ioBuffMemory = vfs_buf_allocate_memory(pid, ioCacheSize + 0x200, memType);
		if (bc->ioBuffMemory == NULL)
			goto free_data;
		ioBuff = (char *)(((SceUIntPtr)bc->ioBuffMemory + 0x1ff) & ~0x1ff);
		memset(ioBuff, 0, ioCacheSize);
	}
	bc->ioBuffCachedSize = 0;
	bc->ioBuffState = VFS_BUFF_LINE_FREE;
	bc->ioBuff = ioBuff;
	bc->ioBuffSize = ioCacheSize;
	bc->ioBuffOffset = -1;
	*bcp = bc;
	return 0;

free_data:
	vfs_buf_free_data_buff(bc, NULL, NULL);
free_bc:
	vfs_buf_free_memory(pid, bc, memType);
	return SCE_ERROR_ERRNO_ENOMEM;
}

/* name unknown: drop every line */
int vfs_buf_data_invalidate_all(struct BuffCache *bc)
{
	return vfs_buf_all_op_subr(bc, NULL, NULL, VFS_BUFF_OP_INVALIDATE);
}

/* name unknown: copy out of the I/O buffer's cached part */
int vfs_buf_io_read(struct BuffCache *bc, SceOff pos, SceSize size, void *buf)
{
	if (bc->ioBuffState >= VFS_BUFF_LINE_INVALID)
		return SCE_ERROR_ERRNO_EACCES;
	if (pos < bc->ioBuffOffset || bc->ioBuffOffset + bc->ioBuffCachedSize < pos + size)
		return SCE_ERROR_ERRNO_EINVAL;
	memcpy(buf, (char *)bc->ioBuff + (SceSize)(pos - bc->ioBuffOffset), size);
	return 0;
}

/* write the I/O buffer back, after the data cache lines it covers */
int vfsBuffIoFlush(struct BuffCache *bc, SceVfsVnode *vp, SceVfsFile *file)
{
	const struct VfsBuffIoOps *ioOps = bc->ioOps;
	SceVopTable *ops;
	SceUInt32 flags;
	SceOff position;
	int ret;

	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 1342);
	if (bc->ioBuffCachedSize == 0)
		return 0;

	ret = vfsBuffDataFlushArea(bc, bc->ioBuffOffset,
				   bc->ioBuffOffset + bc->ioBuffCachedSize - 1, 0, vp, file);
	if (ret != 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1356, "\n");
		return ret;
	}
	if (!(bc->ioBuffState & VFS_BUFF_LINE_DIRTY))
		return 0;

	if (ioOps != NULL && ioOps->write != NULL) {
		VfsBuffIoWriteArgs args;

		args.offset = bc->ioBuffOffset;
		args.data = bc->ioBuff;
		args.size = bc->ioBuffCachedSize;
		args.vp = vp;
		ret = ioOps->write(&args);
		bc->ioBuffState = VFS_BUFF_LINE_FLUSHED;
		return ret;
	}

	ops = vp->core.ops;
	if (ops->vop_write == NULL)
		return SCE_ERROR_ERRNO_EUNSUP;

	{
		SceVopWriteArgs args;

		args.vp = vp;
		args.file = file;
		args.buf = bc->ioBuff;
		args.nbyte = bc->ioBuffCachedSize;
		flags = file->flags;
		position = file->position;
		file->position = bc->ioBuffOffset;
		file->flags = flags | 3;
		ret = ops->vop_write(&args);
		file->flags = (file->flags & ~3) | (flags & 3);
		if (ret != args.nbyte) {
			if (ret < 0) {
				file->position = position;
				bc->ioBuffState = VFS_BUFF_LINE_INVALID;
				return ret;
			}
			if (vp->core.size != file->position + ret) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1414,
						     "wsz(0x%x)!=nbyte(0x%x), retval=0x%x\n", ret,
						     args.nbyte, ret);
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1416,
						     "A:::file->fpos=0x%llx vp->size=0x%llx, wsz=0x%x\n",
						     file->position, vp->core.size, ret);
				bc->ioBuffState = VFS_BUFF_LINE_INVALID;
				file->position = position;
				return ret;
			}
		}
		file->position = position;
		bc->ioBuffState = VFS_BUFF_LINE_FLUSHED;
	}
	return 0;
}

/* free the I/O buffer */
static inline int vfs_buf_free_io_buff(struct BuffCache *bc)
{
	SceUID pid = bc->pid;
	int ret;

	if (bc->memType == VFS_BUFF_MEM_HOST && SceDipswForDriver_A98FC2FD(0xd2)) {
		ret = vfs_buf_free_memory(pid, bc->hostBuff, bc->memType);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 390,
					     "vfs_buf_free_memory failed ... %d 0x%x free address is %p\n",
					     ret, ret, bc->hostBuff);
			SceSysmemForDriver_009E1C61(bc->hostMemBlock);
			return ret;
		}
		return SceSysmemForDriver_009E1C61(bc->hostMemBlock);
	}

	ret = vfs_buf_free_memory(pid, bc->ioBuffMemory, bc->memType);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 400,
				     "vfs_buf_free_memory failed ... %d 0x%x free address is %p\n", ret,
				     ret, bc->ioBuffMemory);
	return ret;
}

/* free a buffer cache, writing it back first unless flags is 1 */
int vfsBuffFree(struct BuffCache *bc, int flags, SceVfsVnode *vp, SceVfsFile *file)
{
	int ret;

	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 413);
	if (flags != 1) {
		ret = vfsBuffDataFlushAll(bc, vp, file);
		if (ret < 0) {
			if (vp->core.mnt->state != VFS_MOUNT_STATE_UMOUNTING)
				return ret;
		} else {
			ret = vfsBuffIoFlush(bc, vp, file);
			if (ret < 0 && vp->core.mnt->state != VFS_MOUNT_STATE_UMOUNTING)
				return ret;
		}
	}

	ret = vfs_buf_free_data_buff(bc, vp, file);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 438, "\n");
		return ret;
	}
	ret = vfs_buf_free_io_buff(bc);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 444, "\n");
		return ret;
	}
	ret = vfs_buf_free_memory(bc->pid, bc, bc->memType);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 452,
				     "vfs_buf_free_memory failed ... %d 0x%x free address is %p\n", ret,
				     ret, bc);
	return ret;
}

/* read the I/O buffer's block holding pos from the file */
int vfsBuffIoFetch(struct BuffCache *bc, SceOff pos, SceVfsVnode *vp, SceVfsFile *file)
{
	SceOff offset = pos & ~(SceOff)(bc->ioBuffSize - 1);
	SceOff position = file->position;
	SceUInt32 flags = file->flags;
	SceVopReadArgs args;
	SceSSize (*read)(SceVopReadArgs *argp);
	int ret, err;

	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 1445);
	read = vp->core.ops->vop_read;
	if (read == NULL)
		return SCE_ERROR_ERRNO_EUNSUP;

	if ((SceOff)vp->core.size < offset) {
		bc->ioBuffOffset = offset;
		bc->ioBuffCachedSize = 0;
		bc->ioBuffState = VFS_BUFF_LINE_FLUSHED;
		return 0;
	}

	args.vp = vp;
	args.file = file;
	args.buf = bc->ioBuff;
	args.nbyte = bc->ioBuffSize;
	file->position = offset;
	file->flags |= 3;
	ret = read(&args);
	file->flags = (file->flags & ~3) | (flags & 3);
	if (ret != args.nbyte) {
		if (ret < 0) {
			file->position = position;
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1478,
					     "rsz(0x%x)!=nbyte(0x%x) retval=0x%x vp->size=0x%llx\n", ret,
					     args.nbyte, ret, vp->core.size);
			return ret;
		}
		if (vp->core.size != offset + ret && vp->core.size != 0 &&
		    (SceOff)vp->core.size >= offset) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1487,
					     "rsz(0x%x)!=nbyte(0x%x) retval=0x%x vp->size=0x%llx\n", ret,
					     args.nbyte, ret, vp->core.size);
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1489, "pos=0x%llx offset=0x%llx org=0x%llx vp=%p\n",
					     pos, offset, position, vp);
			bc->ioBuffState = VFS_BUFF_LINE_INVALID;
			file->position = position;
			return ret;
		}
	}

	bc->ioBuffCachedSize = ret;
	bc->ioBuffState = VFS_BUFF_LINE_FLUSHED;
	bc->ioBuffOffset = offset;
	SCE_DBG_ASSERT(bc->ioBuffCachedSize <= bc->ioBuffSize, 1500);
	if (ret > 0 && vfs_buf_area_cached(bc, offset, offset + ret - 1)) {
		err = vfsBuffDataFlushArea(bc, offset, offset + ret - 1, 0, vp, file);
		if (err < 0)
			ret = err;
	}
	file->position = position;
	return ret;
}

/* grow the I/O buffer's cached part by size */
void vfsBuffIoUpdateCachedSize(struct BuffCache *bc, SceSize size)
{
	if (bc->ioBuffState >= VFS_BUFF_LINE_INVALID || bc->ioBuffCachedSize == bc->ioBuffSize)
		return;
	bc->ioBuffCachedSize += size;
	SCE_DBG_ASSERT(bc->ioBuffCachedSize <= bc->ioBuffSize, 1542);
}

/* replace the whole I/O buffer with size bytes at pos, dropping the lines it covers */
int vfsBuffIoOverWrite(struct BuffCache *bc, SceOff pos, SceSize size, const void *buf,
		       SceVfsVnode *vp)
{
	SceOff end;
	int ret;

	if (size < bc->ioBuffSize)
		return SCE_ERROR_ERRNO_EINVAL;

	ret = vfs_buf_area_invalidate(bc, pos, pos + bc->ioBuffSize - 1, vp);
	if (ret < 0)
		return ret;
	bc->ioBuffOffset = pos;
	memcpy(bc->ioBuff, buf, size);
	bc->ioBuffCachedSize = size;
	bc->ioBuffState |= VFS_BUFF_LINE_DIRTY;
	SCE_DBG_ASSERT(bc->ioBuffCachedSize <= bc->ioBuffSize, 1573);
	end = pos + size;
	if ((SceOff)vp->core.size < end)
		vp->core.size = end;
	return ret;
}

/* copy the line holding pos from the I/O buffer into the data cache */
int vfsBuffIo2DataBuff(struct BuffCache *bc, SceOff pos, SceVfsVnode *vp, SceVfsFile *file)
{
	SceSize lineSize = bc->lineSize;
	SceOff lineOffset;
	int ret;

	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 1594);
	if (bc->ioBuffState >= VFS_BUFF_LINE_INVALID)
		return SCE_ERROR_ERRNO_ENOENT;
	lineOffset = pos & ~(SceOff)(lineSize - 1);
	if (lineOffset < bc->ioBuffOffset ||
	    bc->ioBuffOffset + bc->ioBuffCachedSize < lineOffset + lineSize)
		return SCE_ERROR_ERRNO_ENOENT;

	ret = vfsBuffDataEnter(bc, lineOffset, lineSize,
			       (char *)bc->ioBuff + (SceSize)(lineOffset - bc->ioBuffOffset),
			       VFS_BUFF_LINE_FLUSHED, vp, file);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1609, "enter failed ... fpos=0x%llx, pos=0x%llx\n", pos,
				     lineOffset);
	return ret;
}

/* name unknown: does [pos, pos + size) end inside the I/O buffer */
int vfs_buf_io_buff_reaches(struct BuffCache *bc, SceOff pos, SceSize size)
{
	if (bc->ioBuffState >= VFS_BUFF_LINE_INVALID)
		return 0;
	return bc->ioBuffOffset + bc->ioBuffSize >= pos + size;
}

/*
 * Zero part of the I/O buffer, the way vfsBuffIoWrite writes it; an unused
 * buffer starts at pos.
 */
int vfsBuffIoFillZero(struct BuffCache *bc, SceOff pos, SceSize size, SceVfsVnode *vp,
		      SceSize grow)
{
	SceOff end;
	SceSize offset;

	if (bc->ioBuffOffset == -1) {
		if (pos % bc->ioBuffSize != 0 || size > bc->ioBuffSize)
			return SCE_ERROR_ERRNO_EINVAL;
		bc->ioBuffOffset = pos;
		bc->ioBuffCachedSize = size;
		offset = 0;
		goto zero;
	}

	if (grow != 0) {
		if (pos < bc->ioBuffOffset || bc->ioBuffOffset + bc->ioBuffSize < pos + size) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1659,
					     "bc->ioBuffOffset=0x%llx, pos=0x%llx, size=0x%x, bc->ioBuffSize=0x%x\n",
					     bc->ioBuffOffset, pos, size, bc->ioBuffSize);
			return SCE_ERROR_ERRNO_EINVAL;
		}
		if (grow == (SceSize)-1) {
			bc->ioBuffCachedSize = bc->ioBuffSize;
			SCE_DBG_ASSERT(bc->ioBuffCachedSize <= bc->ioBuffSize, 1666);
		} else {
			bc->ioBuffCachedSize += grow;
			if (bc->ioBuffCachedSize > bc->ioBuffSize) {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 1674,
						     "bc->ioBuffCachedSize=0x%x iooffset=0x%llx extend_size=0x%x\n",
						     bc->ioBuffCachedSize, bc->ioBuffOffset, grow);
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 1676,
						     "vp->size=0x%llx pos=0x%llx size=0x%x extend_size=0x%x\n",
						     vp->core.size, pos, size, grow);
				SCE_DBG_ASSERT(0, 1677);
			}
		}
		end = bc->ioBuffOffset + bc->ioBuffCachedSize;
		if ((SceOff)vp->core.size < end)
			vp->core.size = end;
	} else {
		if (pos < bc->ioBuffOffset ||
		    bc->ioBuffOffset + bc->ioBuffCachedSize < pos + size) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1695,
					     "bc->ioBuffOffset=0x%llx, pos=0x%llx, size=0x%x, bc->ioBuffCachedSize=0x%x\n",
					     bc->ioBuffOffset, pos, size, bc->ioBuffCachedSize);
			return SCE_ERROR_ERRNO_EINVAL;
		}
	}
	offset = pos - bc->ioBuffOffset;
zero:
	memset((char *)bc->ioBuff + offset, 0, size);
	bc->ioBuffState |= VFS_BUFF_LINE_DIRTY;
	return 0;
}

/* move the I/O buffer to pos, a multiple of its size */
int vfsBuffIoBuffSeek(struct BuffCache *bc, SceOff pos, int extend)
{
	if (!(bc->ioBuffState & (VFS_BUFF_LINE_FLUSHED | VFS_BUFF_LINE_FREE)))
		return SCE_ERROR_ERRNO_EPERM;
	if (pos % bc->ioBuffSize != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	bc->ioBuffOffset = pos;
	if (extend)
		bc->ioBuffState |= VFS_BUFF_LINE_DIRTY;
	return 0;
}

/* fill a clean I/O buffer with size bytes at pos */
int vfsBuffIoSet(struct BuffCache *bc, SceOff pos, SceSize size, const void *buf,
		 SceVfsVnode *vp)
{
	SceSize ioBuffSize;

	if (bc->ioBuffState != VFS_BUFF_LINE_FLUSHED && bc->ioBuffState != VFS_BUFF_LINE_FREE)
		return SCE_ERROR_ERRNO_EPERM;
	ioBuffSize = bc->ioBuffSize;
	if (pos % ioBuffSize != 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1744, "pos=0x%llx, bc->ioBuffSize=0x%x\n", pos,
				     size);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (size > ioBuffSize) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1750, "size=0x%x, bc->ioBuffSize=0x%x\n", size,
				     ioBuffSize);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	bc->ioBuffOffset = pos;
	memcpy(bc->ioBuff, buf, size);
	bc->ioBuffCachedSize = size;
	bc->ioBuffState |= VFS_BUFF_LINE_DIRTY;
	return 0;
}

/*
 * Give a vnode a buffer cache of this shape: reshape the lines in place when
 * the sizes stay, else replace the cache.
 */
int vfsBuffCacheCtl(SceVfsVnode *vp, SceSize dataCacheSize, SceUInt32 wayNum, SceSize lineSize,
		    SceSize ioCacheSize, int mode, int a6)
{
	struct BuffCache *bc;
	struct BuffCache *newBc;
	struct BuffLine *line, *lines;
	int lineNum, set, way;
	SceSize dataSize;
	char *data;
	SceUInt16 memType;
	int ret, err;

	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 1783);
	if (vp->core.mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_NOBUF)
		return SCE_ERROR_ERRNO_EPERM;

	bc = vp->core.bc;
	if (bc == NULL) {
		memType = vfs_buff_mem_type(vp);
		ret = vfsBuffAllocate(dataCacheSize, wayNum, lineSize, ioCacheSize,
				      ksceKernelGetProcessIdFromTLS(),
				      memType, NULL, &newBc);
		if (ret >= 0)
			vp->core.bc = newBc;
		return ret;
	}

	lineNum = bc->setNum * bc->wayNum;
	dataSize = bc->lineSize * lineNum;
	if (dataSize != dataCacheSize || bc->ioBuffSize != ioCacheSize ||
	    lineNum != dataCacheSize / lineSize) {
		memType = vfs_buff_mem_type(vp);
		ret = vfsBuffAllocate(dataCacheSize, wayNum, lineSize, ioCacheSize, bc->pid,
				      memType, NULL, &newBc);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 1847, "\n");
			return ret;
		}
		err = vfsBuffFree(bc, 0, vp, vp->core.fd_list);
		if (err < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 1856, "vfsBuffFree() failed ... %d 0x%x\n", err,
					     err);
		vp->core.bc = newBc;
		return ret;
	}

	/* the same lines: write them back and lay them out again */
	if (dataCacheSize % (lineSize * wayNum) != 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1864, "\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (wayNum != 1 && wayNum != 2 && wayNum != 4) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1869, "\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	ret = vfsBuffDataFlushAll(bc, vp, vp->core.fd_list);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1875, "\n");
		return ret;
	}
	ret = vfsBuffIoFlush(bc, vp, vp->core.fd_list);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1881, "\n");
		return ret;
	}

	line = bc->lineList[0];
	lines = line;
	if (bc->setNum != 0) {
		for (set = 0;;) {
			for (way = 0; way < bc->wayNum; way++) {
				if (line <= lines)
					lines = line;
				line = line->next;
			}
			if (++set >= bc->setNum)
				break;
			line = bc->lineList[set];
		}
	}
	data = (char *)(((SceUIntPtr)bc->dataBuffMemory + 0x1ff) & ~0x1ff);
	memset(data, 0, dataSize);
	memset(lines, 0, bc->setNum * bc->wayNum * sizeof(*lines));
	bc->wayNum = wayNum;
	bc->lineSize = lineSize;
	bc->setNum = (SceUInt8)lineNum / wayNum;
	for (set = 0; set < bc->setNum; set++) {
		bc->lineList[set] = NULL;
		for (way = 0; way < bc->wayNum; way++) {
			line = lines++;
			if (bc->lineList[set] != NULL)
				line->next = bc->lineList[set];
			bc->lineList[set] = line;
			line->data = data;
			line->offset = -1;
			data += lineSize;
			line->state = VFS_BUFF_LINE_FREE;
		}
	}
	return ret;
}

/* free a vnode's buffer cache (flags 1: without writing it back) */
int vfsBuffCacheFree(SceVfsVnode *vp, SceVfsFile *file, int flags, int a4)
{
	int ret;

	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 1947);
	if (vp->core.bc == NULL || (SceUInt32)flags > 1 || a4 != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	ret = vfsBuffFree(vp->core.bc, flags, vp, file);
	if (ret != 0)
		return ret;
	vp->core.bc = NULL;
	return 0;
}
