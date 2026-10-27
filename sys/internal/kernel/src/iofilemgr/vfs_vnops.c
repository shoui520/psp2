/*
 * SceIofilemgr: the vnode operations with their buffer cache paths (Sony's vfs_vnops.c).
 */

#include "iofilemgr_internal.h"

/* zero-fills the file from start up to its position through the I/O buffer */
int buff_cache_fill_zero(struct BuffCache *bc, SceOff start, SceVfsVnode *vp, SceVfsFile *file)
{
	SceOff rest = file->fpos - start;
	SceSize size;
	int ret, filled;

	SCE_DBG_ASSERT(rest > 0, 78);
	ret = vfsBuffIoFlush(bc, vp, file);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 83, "vfsBuffIoFlush() failed ... %d 0x%x\n", ret, ret);
		return -1;
	}
	if (start % bc->ioBuffSize != 0) {
		ret = vfsBuffIoFetch(bc, start, vp, file);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 92, "vfsBuffIoFetch() failed ... %d 0x%x\n", ret,
					     ret);
			return -1;
		}
		SCE_DBG_ASSERT(ret != 0, 99);
		size = bc->ioBuffSize - ret;
		if (rest < size)
			size = rest;
		ret = vfsBuffIoFillZero(bc, start, size, vp, size);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 109, "vfsBuffIoFillZero() failed ... %d 0x%x\n",
					     ret, ret);
			return -1;
		}
		ret = vfsBuffIoFlush(bc, vp, file);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 116, "vfsBuffIoFlush() failed ... %d 0x%x\n", ret,
					     ret);
			vp->core.size -= size;
			return -1;
		}
		start += size;
		rest -= size;
	}
	filled = 0;
	while (rest > 0) {
		size = bc->ioBuffSize;
		if (size >= rest) {
			ret = vfsBuffIoFetch(bc, start, vp, file);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 172,
						     "vfsBuffIoFetch() failed ... %d 0x%x\n", ret, ret);
				return -1;
			}
			size = rest;
			ret = vfsBuffIoFillZero(bc, start, size, vp, size);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 179,
						     "vfsBuffIoFillZero() failed ... %d 0x%x\n", ret,
						     ret);
				return -1;
			}
		} else if (filled) {
			/* the buffer is already zero: only move it */
			ret = vfsBuffIoBuffSeek(bc, start, 1);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 138,
						     "vfsBuffIoBuffSeek() failed ... %d 0x%x\n", ret,
						     ret);
				return -1;
			}
			vp->core.size += size;
			filled = 1;
		} else {
			ret = vfsBuffIoFetch(bc, start, vp, file);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 150,
						     "vfsBuffIoFetch() failed ... %d 0x%x\n", ret, ret);
				return -1;
			}
			ret = vfsBuffIoFillZero(bc, start, size, vp, size);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 158,
						     "vfsBuffIoFillZero() failed ... %d 0x%x\n", ret,
						     ret);
				return -1;
			}
			filled = 1;
		}
		ret = vfsBuffIoFlush(bc, vp, file);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 187, "vfsBuffIoFlush() failed ... %d 0x%x\n", ret,
					     ret);
			vp->core.size -= size;
			return -1;
		}
		rest -= size;
		start += size;
	}
	return ret;
}

/* data_write_subr's checks around a write that moves the file position */
static inline __attribute__((always_inline)) void data_write_check(SceOff start,
								   SceVfsVnode *vp,
								   SceVfsFile *file)
{
	SCE_DBG_ASSERT_FUNC(vp->core.size >= start, 413, "data_write_subr");
	SCE_DBG_ASSERT_FUNC(vp->core.size >= file->fpos, 414, "data_write_subr");
}

/* writes through the I/O buffer only, or straight to the file system */
int uncached_data_write(struct BuffCache *bc, SceOff start, SceSize nbyte, const void *data,
			SceVfsVnode *vp, SceVfsFile *file, int pwrite, SceSize *wsz)
{
	SceVopWriteArgs args;
	const char *buf = data;
	SceOff pos = start, end, saved, off;
	SceSize rest = nbyte, done, ioSize, n, fetched;
	int ret, partial, size;

	if (pwrite != 1)
		data_write_check(start, vp, file);
	ret = vfsBuffIoFlush(bc, vp, file);
	if (ret < 0) {
		done = 0;
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 222, "vfsBuffIoFlush() failed ... %d 0x%x\n", ret, ret);
		goto fail;
	}
	done = 0;
	while (rest != 0) {
		if ((pos & 0x1ff) == 0 && ((SceUIntPtr)buf & 0x3f) == 0 &&
		    bc->ioBuffSize < rest) {
			/* aligned and larger than the buffer: write it directly */
			args.vp = vp;
			args.file = file;
			args.buf = buf;
			args.nbyte = rest;
			saved = file->fpos;
			file->fpos = pos;
			n = vp->core.ops->vop_write(&args);
			file->fpos = saved;
			if (n == rest) {
				done += rest;
			} else {
				if ((int)n < 0) {
					ret = n;
					goto fail;
				}
				done += n;
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 255, "wsz(0x%x)!=nbyte(0x%x), retval=0x%x\n",
						     n, rest, n);
			}
			break;
		}
		ioSize = bc->ioBuffSize;
		off = pos % ioSize;
		size = ioSize - off;
		if (size >= rest) {
			size = rest;
			partial = 0;
		} else {
			partial = 1;
		}
		if (off == 0 && pos != vp->core.size && size == ioSize) {
			ret = vfsBuffIoOverWrite(bc, pos, size, buf, vp);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 288,
						     "vfsBuffIoOverWrite() failed ... %d 0x%x\n", ret,
						     ret);
				goto fail;
			}
			if (partial)
				goto flush;
			goto next;
		}
		ret = vfsBuffIoFlush(bc, vp, file);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 300, "vfsBuffIoFlush() failed ... %d 0x%x\n", ret,
					     ret);
			goto fail;
		}
		if ((file->openMode & SCE_O_APPEND) != 0 && size == bc->ioBuffSize) {
			bc->ioBuffOffset = pos & ~(SceOff)(size - 1);
			bc->ioBuffCachedSize = 0;
			bc->ioBuffState = 1;
		} else {
			fetched = vfsBuffIoFetch(bc, pos, vp, file);
			if ((int)fetched < 0) {
				ret = fetched;
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 315, "vfsBuffIoFetch() failed ... %d 0x%x\n",
						     ret, ret);
				goto fail;
			}
			if (fetched != 0) {
				end = bc->ioBuffOffset + bc->ioBuffCachedSize;
				if (end >= pos + size)
					ret = vfsBuffIoWrite(bc, pos, size, buf, vp, 0);
				else
					ret = vfsBuffIoWrite(bc, pos, size, buf, vp,
							     pos != end ? (SceSize)(pos - end) + size :
									  size);
				if (ret < 0) {
					SCE_DBG_PRINTF_LEVEL(2, 0xf, 369,
							     "vfsBuffIoWrite() failed ... %d 0x%x write_sz=0x%x\n",
							     ret, ret, size);
					SCE_DBG_PRINTF_LEVEL(2, 0xf, 372,
							     "start=0x%llx nbyte=0x%x fetch_sz=0x%x\n",
							     start, nbyte, fetched);
					goto fail;
				}
				if (partial)
					goto flush;
				goto next;
			}
		}
		SCE_DBG_ASSERT(pos % size == 0, 329);
		ret = vfsBuffIoSet(bc, pos, size, buf, vp);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 334, "vfsBuffIoSet() failed ... %d 0x%x\n", ret,
					     ret);
			goto fail;
		}
flush:
		ret = vfsBuffIoFlush(bc, vp, file);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 386, "vfsBuffIoFlush() failed ... %d 0x%x\n", ret,
					     ret);
			goto fail;
		}
next:
		pos += (SceSize)size;
		rest -= size;
		done += size;
		buf += size;
	}
	goto out;
fail:
	SCE_DBG_PRINTF_FUNC(0xf, 421, "data_write_subr",
			    "uncached_data_write failed ... %d 0x%x wsz=0x%x\n", ret, ret, done);
out:
	if (pwrite != 1) {
		file->fpos += (int)done;
		if (vp->core.size < file->fpos) {
			SCE_DBG_PRINTF_FUNC(0xf, 429, "data_write_subr",
					    "file->fpos[0x%llx] > vp->size[0x%llx]\n",
					    file->fpos, vp->core.size);
			SCE_DBG_PRINTF_FUNC(0xf, 432, "data_write_subr",
					    "start[0x%llx] > nbyte[0x%x] wsz=0x%x FPOS = 0x%llx\n",
					    start, nbyte, done, file->fpos - (int)done);
			SCE_DBG_ASSERT_FUNC(0, 433, "data_write_subr");
		}
	}
	*wsz = done;
	return ret;
}

/*
 * A write through the buffer cache: into a cached line, a line of its own,
 * or the I/O buffer; what does not fit goes to uncached_data_write.
 */
int buff_cache_write(SceOff pos, const void *data, SceSize nbyte, SceVfsVnode *vp,
		     SceVfsFile *file, int pwrite, SceSize *wsz)
{
	SceVopTable *ops = vp->core.ops;
	struct BuffCache *bc = vp->core.bc;
	const char *buf = data;
	SceOff end, last, ioStart, ioEnd, newSize;
	SceSize n, rest;
	SceVopPwriteArgs args;
	int ret, fetched;

	if (vp->core.size < pos)
		return -1;
	end = pos + nbyte;
	newSize = (SceOff)vp->core.size >= end ? 0 : end;

	if (bc->ioBuffState == VFS_BUFF_LINE_FREE) {
		ret = vfsBuffIoFetch(bc, pos, vp, file);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 470, "Failed first fetch ... %d %d\n", ret, ret);
	}

	if (vfs_buf_in_one_line(bc, pos, nbyte)) {
		if (vfs_buf_data_cached(bc, pos)) {
			/* the line is cached: write into it */
			ret = vfsBuffDataWrite(bc, vp, pos, nbyte, buf);
			if (ret != 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 512, "vfsBuffDataWrite failed ... %d 0x%x\n",
						     ret, ret);
				return -1;
			}
			if (pwrite == 1) {
				*wsz = nbyte;
				SCE_DBG_ASSERT(newSize == 0, 503);
			} else {
				file->fpos += nbyte;
				*wsz = nbyte;
				SCE_DBG_ASSERT(newSize == 0, 503);
				SCE_DBG_ASSERT(vp->core.size >= file->fpos, 506);
			}
			return 0;
		}

		if (nbyte == bc->lineSize) {
			/* a whole line: give it a line of its own */
			SCE_DBG_ASSERT((pos & (nbyte - 1)) == 0, 620);
			if (pos % bc->ioBuffSize == 0 && newSize != 0 &&
			    (bc->ioBuffCachedSize == 0 || !vfs_buf_io_buff_contains(bc, pos, nbyte)))
				goto uncached;
			ret = vfsBuffDataEnter(bc, pos, nbyte, buf, VFS_BUFF_LINE_DIRTY, vp, file);
			if (ret != 0) {
				if (ret != SCE_ERROR_ERRNO_EBUSY)
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 671,
							     "vfsBuffDataEnter failed ... %d 0x%x\n", ret,
							     ret);
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 674, "vfsBuffDataEnter failed ... %d 0x%x\n",
						     ret, ret);
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 996,
						     "unexpected sequence vp=%p file=%p buf=%p nbyte=0x%x\n",
						     vp, file, buf, nbyte);
				return -1;
			}
			if (pwrite != 1)
				file->fpos += nbyte;
			*wsz = nbyte;
			if (newSize != 0) {
				vfsBuffIoUpdateCachedSize(bc, newSize - vp->core.size);
				vp->core.size = newSize;
			}
			if (pwrite != 1 && vp->core.size < file->fpos) {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 662, "fpos=0x%llx, size=0x%llx\n",
						     file->fpos, vp->core.size);
				SCE_DBG_ASSERT(vp->core.size >= file->fpos, 663);
			}
			return 0;
		}

		/* part of a line: through the I/O buffer */
		fetched = 0;
		if (!vfs_buf_io_cached_contains(bc, pos, nbyte)) {
			if (newSize != 0 && vfs_buf_io_buff_contains(bc, pos, nbyte) &&
			    vfs_buf_io_buff_reaches(bc, pos, nbyte))
				goto io_write_grow;
			ret = vfsBuffIoFlush(bc, vp, file);
			if (ret < 0)
				return ret;
			ret = vfsBuffIoFetch(bc, pos, vp, file);
			if (ret < 0) {
				if (ret == SCE_ERROR_ERRNO_EUNSUP)
					ret = -1;
				return ret;
			}
			fetched = ret == 0;
			if (newSize == 0)
				goto io_write;
			if (!fetched)
				goto io_write_grow;
			ret = vfsBuffIoSet(bc, pos, nbyte, buf, vp);
			if (ret < 0)
				goto io_failed;
			goto io_flush;
		}
		if (newSize != 0)
			goto io_write_grow;
io_write:
		ret = vfsBuffIoWrite(bc, pos, nbyte, buf, vp, 0);
		if (ret < 0)
			goto io_failed;
		if (fetched) {
io_flush:
			ret = vfsBuffIoFlush(bc, vp, file);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 596, "vfsBuffIoFlush() failed ... %d 0x%x\n",
						     ret, ret);
				return -1;
			}
		}
		goto io_done;
io_write_grow:
		ret = vfsBuffIoWrite(bc, pos, nbyte, buf, vp, newSize - vp->core.size);
		if (ret < 0) {
io_failed:
			if (ret == SCE_ERROR_ERRNO_ENOSPC)
				goto uncached;
			return -1;
		}
io_done:
		vfsBuffIo2DataBuff(bc, pos, vp, file);
		if (pwrite != 1) {
			file->fpos += nbyte;
			if (vp->core.size < file->fpos) {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 608, "fpos=0x%llx size=0x%llx\n",
						     file->fpos, vp->core.size);
				SCE_DBG_ASSERT(vp->core.size >= file->fpos, 609);
			}
		}
		*wsz = nbyte;
		return ret;
	}

	/* more than one line: drop the lines it covers, then the I/O buffer */
	last = end - 1;
	if (vfs_buf_area_cached(bc, pos, last)) {
		ret = vfsBuffDataFlushArea(bc, pos, last, 0, vp, file);
		if (ret != 0)
			return ret;
		ret = vfs_buf_area_invalidate(bc, pos, last, NULL);
		if (ret != 0)
			return ret;
	}

	if (vfs_buf_io_buff_contains(bc, pos, nbyte)) {
		if (newSize != 0)
			ret = vfsBuffIoWrite(bc, pos, nbyte, buf, vp, newSize - vp->core.size);
		else
			ret = vfsBuffIoWrite(bc, pos, nbyte, buf, vp, 0);
		if (ret >= 0) {
			if (pwrite != 1) {
				file->fpos += nbyte;
				if (vp->core.size < file->fpos) {
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 742,
							     "vp=%p file->fpos=0x%llx vp->size=0x%llx nbyte=0x%x upd_fsize=0x%llx\n",
							     vp, file->fpos, vp->core.size, nbyte,
							     newSize);
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 743,
							     "offset=0x%llx fstart=0x%llx fend=0x%llx\n",
							     pos, pos, last);
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 745,
							     "bc->ioBuffCachedSize=0x%x iooffset=0x%llx\n",
							     bc->ioBuffCachedSize, bc->ioBuffOffset);
					SCE_DBG_ASSERT(vp->core.size >= file->fpos, 746);
				}
			}
			*wsz = nbyte;
			return ret;
		}
		if (ret != SCE_ERROR_ERRNO_ENOSPC) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 733, "failed ... %d 0x%x\n", ret, ret);
			return -1;
		}
		goto uncached;
	}

	if (bc->ioBuffState >= VFS_BUFF_LINE_INVALID)
		goto uncached;
	ioStart = bc->ioBuffOffset;
	ioEnd = ioStart + bc->ioBuffSize - 1;
	if (pos >= ioStart) {
		if (pos > ioEnd)
			goto uncached;
		/* it starts in the I/O buffer and runs past it */
		if (pos == ioStart && last > ioEnd &&
		    (vp->core.mntOpt & VFS_VNODE_MNT_OPT_DIRECT_WRITE))
			goto drop_io_buff;
		if (last <= ioEnd) {
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 943,
					     "fstart = 0x%llx, iostart=0x%llx, fend=0x%llx <= ioend=0x%llx nbyte=0x%x\n",
					     pos, ioStart, last, ioEnd, nbyte);
			SCE_DBG_ASSERT(0, 944);
		}
		n = ioEnd + 1 - pos;
		ret = vfsBuffIoWrite(bc, pos, n, buf, vp, (SceSize)-1);
		if (ret >= 0) {
			if (pwrite != 1) {
				file->fpos += n;
				SCE_DBG_ASSERT(vp->core.size >= file->fpos, 979);
			}
			ret = uncached_data_write(bc, pos + n, nbyte - n, buf + n, vp, file, pwrite,
						  wsz);
			*wsz += n;
			return ret;
		}
		if (ret != SCE_ERROR_ERRNO_ENOSPC) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 966, "vfsBuffIoWrite failed ... %d 0x%x\n", ret,
					     ret);
			return -1;
		}
		goto uncached;
	}
	if (last >= ioEnd) {
		/* it covers the whole I/O buffer */
drop_io_buff:
		bc->ioBuffState = VFS_BUFF_LINE_INVALID;
		goto uncached;
	}
	if (last < ioStart)
		goto uncached;

	/* it ends in the I/O buffer: write the part before it directly */
	if ((pos & 0x1ff) != 0 || ((SceUIntPtr)buf & 0x3f) != 0)
		goto uncached;
	n = ioStart - pos;
	args.vp = vp;
	args.file = file;
	args.buf = buf;
	args.nbyte = n;
	if (pwrite == 1) {
		args.offset = pos;
		*wsz = ops->vop_pwrite(&args);
	} else {
		*wsz = ops->vop_write((SceVopWriteArgs *)&args);
	}
	if (*wsz != n) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 842, "wbyte(0x%x) != args.nbyte(0x%x)\n", *wsz, n);
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 844, "fpos=0x%llx\n", file->fpos);
		return *wsz;
	}
	rest = nbyte - n;
	buf += n;
	if (newSize != 0)
		ret = vfsBuffIoWrite(bc, ioStart, rest, buf, vp, newSize - vp->core.size);
	else
		ret = vfsBuffIoWrite(bc, ioStart, rest, buf, vp, 0);
	if (ret >= 0) {
		if (pwrite != 1) {
			file->fpos += rest;
			SCE_DBG_ASSERT(vp->core.size >= file->fpos, 904);
		}
		*wsz = nbyte;
		return ret;
	}
	if (ret != SCE_ERROR_ERRNO_ENOSPC) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 871, "vfsBuffIoWrite failed ... %d 0x%x\n", ret, ret);
		args.vp = vp;
		args.file = file;
		args.buf = buf;
		args.nbyte = rest;
		if (pwrite == 1) {
			args.offset = pos + n;
			*wsz = ops->vop_pwrite(&args);
		} else {
			*wsz = ops->vop_write((SceVopWriteArgs *)&args);
		}
		return ret;
	}
	return uncached_data_write(bc, ioStart, rest, buf, vp, file, pwrite, wsz);

uncached:
	return uncached_data_write(bc, pos, nbyte, buf, vp, file, pwrite, wsz);
}

/* the end of a read: report a failure, move the file position, give the size read */
static inline __attribute__((always_inline)) void data_read_subr(int ret, SceSize done,
								 SceVfsVnode *vp,
								 SceVfsFile *file, int pread,
								 SceSize *rsz)
{
	if (ret < 0 && ret != VFS_ERROR_READ_QUIET)
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1120, "uncached_data_read failed ... %d 0x%x rsz=0x%x\n",
				     ret, ret, done);
	if (pread != 1) {
		file->fpos += (int)done;
		SCE_DBG_ASSERT(vp->core.size >= file->fpos, 1125);
	}
	*rsz = done;
}

/* reads through the I/O buffer only, or straight from the file system */
int uncached_data_read(struct BuffCache *bc, SceOff pos, SceSize nbyte, void *data,
		       SceVfsVnode *vp, SceVfsFile *file, int pread, SceSize *rsz)
{
	SceSSize (*read)(SceVopReadArgs *argp);
	SceVopReadArgs args;
	char *buf = data;
	SceSize done = 0, size;
	SceOff position;
	int ret = 0, n;

	if (vp->core.size < pos + nbyte)
		nbyte = vp->core.size - pos;
	while (nbyte != 0) {
		if ((pos & 0x1ff) == 0 && ((SceUIntPtr)buf & 0x3f) == 0 && bc->ioBuffSize < nbyte &&
		    (read = vp->core.ops->vop_read) != NULL) {
			/* aligned and larger than the buffer: read it directly */
			position = file->fpos;
			args.vp = vp;
			args.file = file;
			file->fpos = pos;
			args.buf = buf;
			args.nbyte = nbyte;
			n = read(&args);
			file->fpos = position;
			if (n == nbyte) {
				done += nbyte;
			} else if (n >= 0) {
				done += n;
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1047,
						     "rsz(0x%x)!=all_read(0x%x), retval=0x%x\n", n, nbyte,
						     n);
			} else {
				ret = n;
			}
			break;
		}
		ret = vfsBuffIoFlush(bc, vp, file);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1064, "vfsBuffIoFlush() failed ... %d 0x%x\n", ret,
					     ret);
			break;
		}
		ret = vfsBuffIoFetch(bc, pos, vp, file);
		if (ret <= 0) {
			if (ret != 0 || vp->core.size != pos)
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1073,
						     "vfsBuffIoFetch(pos=0x%llx) failed ... %d 0x%x vp->size=0x%llx\n",
						     pos, ret, ret, vp->core.size);
			break;
		}
		size = bc->ioBuffCachedSize - (SceSize)(pos % bc->ioBuffSize);
		if (nbyte <= size)
			size = nbyte;
		ret = vfs_buf_io_read(bc, pos, size, buf);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1092,
					     "vfsBuffIoRead(0x%llx) size=0x%x failed ... %d 0x%x\n", pos,
					     size, ret, ret);
			break;
		}
		pos += size;
		done += size;
		nbyte -= size;
		buf += size;
	}
	data_read_subr(ret, done, vp, file, pread, rsz);
	return ret;
}

/*
 * A read through the buffer cache: from a cached line, the I/O buffer or
 * both; what is not cached comes from uncached_data_read.
 */
int buff_cache_read(SceOff pos, void *data, SceSize nbyte, SceVfsVnode *vp, SceVfsFile *file,
		    int pread, SceSize *rsz)
{
	struct BuffCache *bc = vp->core.bc;
	SceVopTable *ops = vp->core.ops;
	char *buf = data;
	SceOff last, ioStart, ioEnd;
	SceVopPreadArgs args;
	SceSize n, rest;
	int ret;

	if (bc->ioBuffState == VFS_BUFF_LINE_FREE) {
		ret = vfsBuffIoFetch(bc, pos, vp, file);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1153, "Failed first fetch ... %d %d\n", ret, ret);
	}

	if (vfs_buf_in_one_line(bc, pos, nbyte)) {
		if (vfs_buf_data_cached(bc, pos)) {
			ret = vfsBuffDataRead(bc, vp, pos, nbyte, buf);
			if (ret != 0)
				return -1;
			if (pread != 1) {
				file->fpos += nbyte;
				SCE_DBG_ASSERT(vp->core.size >= file->fpos, 1168);
			}
			*rsz = nbyte;
			return 0;
		}
		if (!vfs_buf_io_cached_contains(bc, pos, nbyte)) {
			ret = vfsBuffIoFlush(bc, vp, file);
			if (ret < 0)
				return ret;
			ret = vfsBuffIoFetch(bc, pos, vp, file);
			if (ret <= 0) {
				if (ret == SCE_ERROR_ERRNO_EUNSUP)
					return -1;
				*rsz = 0;
				return ret;
			}
		}
		ret = vfs_buf_io_read(bc, pos, nbyte, buf);
		if (ret < 0)
			return -1;
		vfsBuffIo2DataBuff(bc, pos, vp, file);
		if (pread != 1) {
			file->fpos += nbyte;
			SCE_DBG_ASSERT(vp->core.size >= file->fpos, 1213);
		}
		*rsz = nbyte;
		return ret;
	}

	/* more than one line: write back and drop the lines it covers */
	last = pos + nbyte - 1;
	if (vfs_buf_area_cached(bc, pos, last)) {
		ret = vfsBuffDataFlushArea(bc, pos, last, 0, vp, file);
		if (ret != 0)
			return ret;
		ret = vfs_buf_area_invalidate(bc, pos, last, NULL);
		if (ret != 0)
			return ret;
	}

	if (vfs_buf_io_cached_contains(bc, pos, nbyte)) {
		ret = vfs_buf_io_read(bc, pos, nbyte, buf);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1263, "vfsBuffIoRead failed ... %d 0x%x\n", ret,
					     ret);
			return -1;
		}
		if (pread != 1) {
			file->fpos += nbyte;
			SCE_DBG_ASSERT(vp->core.size >= file->fpos, 1269);
		}
		*rsz = nbyte;
		return ret;
	}

	if (bc->ioBuffState >= VFS_BUFF_LINE_INVALID)
		goto uncached;
	ioStart = bc->ioBuffOffset;
	ioEnd = ioStart + bc->ioBuffCachedSize - 1;
	if (pos >= ioStart) {
		if (pos > ioEnd)
			goto uncached;
		/* it starts in the I/O buffer and runs past it */
		SCE_DBG_ASSERT(ioEnd < last, 1435);
		n = ioStart + bc->ioBuffCachedSize - pos;
		ret = vfs_buf_io_read(bc, pos, n, buf);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1445, "vfsBuffIoRead failed ... %d 0x%x\n", ret,
					     ret);
			return -1;
		}
		if (pread != 1) {
			file->fpos += n;
			SCE_DBG_ASSERT(vp->core.size >= file->fpos, 1451);
		}
		ret = uncached_data_read(bc, pos + n, nbyte - n, buf + n, vp, file, pread, rsz);
		*rsz += n;
		return ret;
	}
	if (last < ioEnd) {
		if (last < ioStart)
			goto uncached;
		if ((pos & 0x1ff) == 0 && ((SceUIntPtr)buf & 0x3f) == 0) {
			/* it ends in the I/O buffer: read the part before it directly */
			n = ioStart - pos;
			args.vp = vp;
			args.file = file;
			args.buf = buf;
			args.nbyte = n;
			if (pread == 1) {
				args.offset = pos;
				*rsz = ops->vop_pread(&args);
			} else {
				*rsz = ops->vop_read((SceVopReadArgs *)&args);
			}
			if (*rsz != n) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1363, "rbyte(0x%x) != args.nbyte(0x%x)\n",
						     *rsz, n);
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1365,
						     "fstart(0x%llx) fend(0x%llx) iostart(0x%llx) ioend(0x%llx) vp=%p size=0x%x\n",
						     pos, last, ioStart, ioEnd, vp, nbyte);
				return *rsz;
			}
			buf += n;
			rest = nbyte - n;
			ret = vfs_buf_io_read(bc, ioStart, rest, buf);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1376, "vfsBuffIoRead failed ... %d 0x%x\n",
						     ret, ret);
				args.vp = vp;
				args.file = file;
				args.buf = buf;
				args.nbyte = rest;
				if (pread == 1) {
					args.offset = pos + n;
					ret = ops->vop_pread(&args);
				} else {
					ret = ops->vop_read((SceVopReadArgs *)&args);
				}
				if (ret >= 0) {
					*rsz += ret;
					ret = 0;
				}
				return ret;
			}
			if (pread != 1) {
				file->fpos += rest;
				SCE_DBG_ASSERT(vp->core.size >= file->fpos, 1414);
			}
			*rsz = nbyte;
			return ret;
		}
	}
	ret = vfsBuffIoFlush(bc, vp, file);
	if (ret < 0)
		return -1;

uncached:
	return uncached_data_read(bc, pos, nbyte, buf, vp, file, pread, rsz);
}

int sceVopLookup(SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path, SceUInt32 flags)
{
	SceVopLookupArgs args;
	int (*op)(SceVopLookupArgs *argp);
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(dvp) == 1, 1505, "sceVopLookup");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_LOOKUP, 3, dvp, flags, path->path);
	if (dvp->core.state >= SCE_VNODE_STATE_INACTIVE) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_LOOKUP + 1, 2, dvp, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	ret = SCE_ERROR_ERRNO_EUNSUP;
	op = dvp->core.ops->vop_lookup;
	if (op != NULL) {
		args.dvp = dvp;
		args.vpp = vpp;
		args.path = path;
		args.flags = flags;
		ret = op(&args);
		if (ret == 0)
			SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(*vpp) == 1, 1527, "sceVopLookup");
		vfs_set_mnt_sync_request(dvp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_LOOKUP + 1, 2, dvp, ret);
	return ret;
}

int sceVopOpen(SceVfsVnode *vp, SceVfsPath *path, int flags, SceVfsFile *file)
{
	int ret, err;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 1566, "sceVopOpen");
	if (__builtin_expect(g_vfsVopHook != NULL, 0)) {
		if (path->path != NULL)
			g_vfsVopHook(VFS_VOP_HOOK_OPEN, 3, vp, flags, path->path);
		else
			g_vfsVopHook(VFS_VOP_HOOK_OPEN, 3, vp, flags, path->name);
	}
	if (vp->core.state >= SCE_VNODE_STATE_INACTIVE) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_OPEN + 1, 1, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	if ((flags & SCE_O_WRONLY) != 0 &&
	    (vp->core.mnt->mntFlags & SCE_VFS_MOUNT_FLAG_RDONLY) != 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_OPEN + 1, 2, vp, SCE_ERROR_ERRNO_EROFS);
		return SCE_ERROR_ERRNO_EROFS;
	}
	if (vp->core.ops->vop_open == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		{
			SceVopOpenArgs args;

			args.vp = vp;
			args.path = path;
			args.flags = flags;
			args.file = file;
			ret = vp->core.ops->vop_open(&args);
		}
		vfs_set_mnt_sync_request(vp->core.mnt);
		if (ret == 0) {
			if (vp->core.bc == NULL &&
			    (vp->core.mnt->mntFlags & SCE_VFS_MOUNT_FLAG_NOBUF) == 0 &&
			    (flags & (VFS_O_NO_BUFFCACHE | SCE_O_NOBUF)) == 0) {
				struct BuffCache *bc;
				int memType;

				memType = vfs_is_cdialog_thread() != 0 ?
					  VFS_BUFF_MEM_CDIALOG : VFS_BUFF_MEM_DEFAULT;
				if (vp->core.mnt->fsType == SCE_VFS_FS_TYPE_HOSTFS)
					memType = VFS_BUFF_MEM_HOST;
				err = vfsBuffAllocate(VFS_BUFF_DEFAULT_DATA_CACHE_SIZE,
						      VFS_BUFF_DEFAULT_WAY_NUM,
						      VFS_BUFF_DEFAULT_LINE_SIZE,
						      vp->core.mnt->defaultIoCacheSize, file->pid,
						      memType, NULL, &bc);
				if (err == 0)
					vp->core.bc = bc;
				else if (err != SCE_ERROR_ERRNO_ENOMEM)
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1636, "sceVopOpen",
								  "vfsBuffAllocate() failed ... %d 0x%x\n",
								  ret, ret);
			}
			if (sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART) != 0)
				vfs_debug_path_create(file, path->path);
		}
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_OPEN + 1, 2, vp, ret);
	return ret;
}

int sceVopCreate(SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path, int flags, int mode)
{
	SceVopCreateArgs args;
	SceVopTable *ops;
	int (*op)(SceVopCreateArgs *argp);
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(dvp) == 1, 1683, "sceVopCreate");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CREATE, 4, dvp, flags, mode, path->path);
	if (dvp->core.state >= SCE_VNODE_STATE_INACTIVE) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CREATE + 1, 2, dvp, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	ops = dvp->core.ops;
	if ((dvp->core.mnt->mntFlags & SCE_VFS_MOUNT_FLAG_RDONLY) != 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CREATE + 1, 2, dvp, SCE_ERROR_ERRNO_EROFS);
		return SCE_ERROR_ERRNO_EROFS;
	}
	op = ops->vop_create;
	if (op == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.dvp = dvp;
		args.vpp = vpp;
		args.path = path;
		args.flags = flags;
		args.mode = mode;
		ret = op(&args);
		if (ret == 0)
			SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(*vpp) == 1, 1716, "sceVopCreate");
		vfs_set_mnt_sync_request(dvp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CREATE + 1, 2, dvp, ret);
	return ret;
}

int sceVopClose(SceVfsVnode *vp, SceVfsFile *file)
{
	SceVopTable *ops;
	struct BuffCache *bc;
	SceVfsFile *f;
	SceUID pid;
	int ret, err;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 1750, "sceVopClose");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CLOSE, 1, vp);
	ret = vfs_get_file_closed_error(file);
	if (ret < 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CLOSE + 1, 2, vp, ret);
		return ret;
	}
	if (vp->core.state > 0xfff) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CLOSE + 1, 2, vp, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	bc = vp->core.bc;
	ops = vp->core.ops;
	if (bc != NULL) {
		if (vp->core.fdNum == 1) {
			err = vfsBuffFree(bc, vp->core.mnt->state != VFS_MOUNT_STATE_MOUNTED, vp, file);
			if (err < 0) {
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1782, "sceVopClose",
							  "vfsBuffFree() failed ... 0x%x %d %p\n",
							  err, err, vp);
				goto fail;
			}
			vp->core.bc = NULL;
		} else {
			pid = file->pid;
			if (pid != SCE_KERNEL_PROCESS_ID && pid == bc->pid) {
				struct BuffCache *newBc;

				/* the buffer cache belongs to this process: hand it to another */
				for (f = vp->core.fdList; f != NULL; f = f->next) {
					if (pid == f->pid && f != file)
						goto close;
				}
				err = vfsBuffFree(bc, vp->core.mnt->state != VFS_MOUNT_STATE_MOUNTED, vp,
						  file);
				if (err < 0) {
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1818, "sceVopClose",
								  "vfsBuffFree() failed ... 0x%x %d %p\n", err,
								  err, vp);
					goto fail;
				}
				vp->core.bc = NULL;
				newBc = NULL;
				pid = file->pid;
				for (f = vp->core.fdList; f != NULL; f = f->next) {
					int memType;

					if (f->pid == pid)
						continue;
					memType = vfs_is_cdialog_thread() != 0 ?
						  VFS_BUFF_MEM_CDIALOG : VFS_BUFF_MEM_DEFAULT;
					if (vp->core.mnt->fsType == SCE_VFS_FS_TYPE_HOSTFS)
						memType = VFS_BUFF_MEM_HOST;
					ret = vfsBuffAllocate(VFS_BUFF_DEFAULT_DATA_CACHE_SIZE,
							      VFS_BUFF_DEFAULT_WAY_NUM,
							      VFS_BUFF_DEFAULT_LINE_SIZE,
							      vp->core.mnt->defaultIoCacheSize, f->pid,
							      memType, NULL, &newBc);
					if (ret == 0) {
						vp->core.bc = newBc;
						break;
					}
					if (ret != SCE_ERROR_ERRNO_ENOMEM)
						SCE_DBG_PRINTF_FUNC(0xf, 1856, "sceVopClose",
								    "vfsBuffAllocate returns %d 0x%x\n",
								    ret, ret);
				}
			}
		}
	}
close:
	if (ops->vop_close == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		SceVopCloseArgs args;

		args.file = file;
		args.vp = vp;
		ret = ops->vop_close(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CLOSE + 1, 2, vp, ret);
	return ret;
fail:
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CLOSE + 1, 2, vp, err);
	return err;
}

int sceVopRead(SceVfsVnode *vp, SceVfsFile *file, void *data, SceSize nbyte, SceSize *result)
{
	struct BuffCache *bc = vp->core.bc;
	SceVopReadArgs args;
	SceVopTable *ops;
	SceSSize n;
	int ret, err;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 1911, "sceVopRead");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_READ, 8, vp, file, data, nbyte,
			 (SceUInt32)((SceUInt64)vp->core.size >> 32), (SceUInt32)vp->core.size,
			 (SceUInt32)((SceUInt64)file->fpos >> 32), (SceUInt32)file->fpos);
	ret = vfs_get_file_state_error(file);
	if (ret < 0)
		goto out;
	if (vp->core.state > 0xfff) {
		ret = SCE_ERROR_ERRNO_EPERM;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_READ + 1, 3, vp, ret, *result);
		return ret;
	}
	ops = vp->core.ops;
	if (ops->vop_read == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_READ + 1, 3, vp, ret, *result);
		return ret;
	}
	if (nbyte == 0) {
		*result = 0;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_READ + 1, 3, vp, 0, 0);
		return 0;
	}
	if ((vp->core.type & (SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_CHR)) ==
	    (SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_CHR)) {
		vfsUnlockFd(file);
		vfs_decrement_io_count();
		args.vp = vp;
		args.file = file;
		args.buf = data;
		args.nbyte = nbyte;
		n = ops->vop_read(&args);
		*result = n;
		ret = n < 0 ? n : 0;
		vfs_increment_io_count();
		err = vfsTrylockFd(file);
		if (err != 0) {
			if (err == SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN) {
				vfsUnlockVnode(vp);
				vfsLockFd(file);
				vfsLockVnode(vp);
			} else {
				ret = err;
			}
		}
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_READ + 1, 3, vp, ret, *result);
		return ret;
	}
	if (file->fpos >= (SceOff)vp->core.size) {
		*result = 0;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_READ + 1, 3, vp, ret, 0);
		return 0;
	}
	if (data == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_READ + 1, 3, vp, ret, *result);
		return ret;
	}
	vfs_set_mnt_sync_request(vp->core.mnt);
	if (bc != NULL) {
		ret = buff_cache_read(file->fpos, data, nbyte, vp, file, 0, result);
		if (ret != -1)
			goto out;
	}
	args.vp = vp;
	args.file = file;
	args.buf = data;
	args.nbyte = nbyte;
	n = ops->vop_read(&args);
	ret = n < 0 ? n : 0;
	*result = n;
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_READ + 1, 3, vp, ret, n);
	return ret;
out:
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_READ + 1, 3, vp, ret, *result);
	return ret;
}

int sceVopWrite(SceVfsVnode *vp, SceVfsFile *file, const void *data, SceSize nbyte,
		 SceSize *result)
{
	SceVfsMount *mnt = vp->core.mnt;
	struct BuffCache *bc = vp->core.bc;
	SceVopWriteArgs args;
	SceVopTable *ops;
	SceSSize n;
	int ret, err;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 2043, "sceVopWrite");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE, 8, vp, file, data, nbyte,
			 (SceUInt32)((SceUInt64)vp->core.size >> 32), (SceUInt32)vp->core.size,
			 (SceUInt32)((SceUInt64)file->fpos >> 32), (SceUInt32)file->fpos);
	err = vfs_get_file_state_error(file);
	if (err < 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE + 1, 3, vp, err, *result);
		return err;
	}
	if (vp->core.state > 0xfff) {
		ret = SCE_ERROR_ERRNO_EPERM;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE + 1, 3, vp, ret, *result);
		return ret;
	}
	ops = vp->core.ops;
	if (ops->vop_write == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE + 1, 3, vp, ret, *result);
		return ret;
	}
	if (nbyte == 0) {
		*result = 0;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE + 1, 3, vp, 0, 0);
		return 0;
	}
	if ((vp->core.type & (SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_CHR)) ==
	    (SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_CHR)) {
		/* the device may block: release the fd lock around the write */
		vfsUnlockFd(file);
		vfs_decrement_io_count();
		args.vp = vp;
		args.nbyte = nbyte;
		args.file = file;
		args.buf = data;
		n = ops->vop_write(&args);
		*result = n;
		ret = n < 0 ? n : 0;
		vfs_increment_io_count();
		err = vfsTrylockFd(file);
		if (err != 0) {
			if (err == SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN) {
				vfsUnlockVnode(vp);
				vfsLockFd(file);
				vfsLockVnode(vp);
			} else {
				ret = err;
			}
		}
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE + 1, 3, vp, ret, *result);
		return ret;
	}
	if (data == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE + 1, 3, vp, ret, *result);
		return ret;
	}
	vfs_set_mnt_sync_request(vp->core.mnt);
	if (bc != NULL &&
	    ((SceOff)vp->core.size >= file->fpos ||
	     buff_cache_fill_zero(bc, vp->core.size, vp, file) != -1)) {
		ret = buff_cache_write(file->fpos, data, nbyte, vp, file, 0, result);
		if (ret != -1) {
			if ((mnt->mntFlags & SCE_VFS_MOUNT_FLAG_WRITE_CACHE) == 0) {
				err = vfsBuffDataFlushAll(vp->core.bc, vp, file);
				if (err < 0) {
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 2148, "sceVopWrite",
								  "vfsBuffDataFlushAll() failed ... 0x%x %d %p\n",
								  err, err, vp);
					VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE + 1, 3, vp, err, *result);
					return err;
				}
				err = vfsBuffIoFlush(vp->core.bc, vp, file);
				if (err < 0) {
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 2157, "sceVopWrite",
								  "vfsBuffIoFlush() failed ... 0x%x %d %p\n",
								  err, err, vp);
					VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE + 1, 3, vp, err, *result);
					return err;
				}
			}
			goto out;
		}
	}
	args.vp = vp;
	args.nbyte = nbyte;
	args.file = file;
	args.buf = data;
	n = ops->vop_write(&args);
	ret = n < 0 ? n : 0;
	*result = n;
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE + 1, 3, vp, ret, n);
	return ret;
out:
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WRITE + 1, 3, vp, ret, *result);
	return ret;
}

int sceVopPread(SceVfsVnode *vp, SceVfsFile *file, void *data, SceSize nbyte, SceOff offset,
		 SceSize *result)
{
	SceVopPreadArgs args;
	SceVopTable *ops;
	SceSSize n;
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 2234, "sceVopPread");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PREAD, 10, vp, file, data, nbyte,
			 (SceUInt32)((SceUInt64)vp->core.size >> 32), (SceUInt32)vp->core.size,
			 (SceUInt32)((SceUInt64)file->fpos >> 32), (SceUInt32)file->fpos,
			 (SceUInt32)((SceUInt64)offset >> 32), (SceUInt32)offset);
	if (offset < 0)
		goto inval;
	ret = vfs_get_file_state_error(file);
	if (ret < 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PREAD + 1, 3, vp, ret, *result);
		return ret;
	}
	if (vp->core.state > 0xfff) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PREAD + 1, 3, vp, SCE_ERROR_ERRNO_EPERM, *result);
		return SCE_ERROR_ERRNO_EPERM;
	}
	ops = vp->core.ops;
	if (ops->vop_pread == NULL) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PREAD + 1, 3, vp, SCE_ERROR_ERRNO_EUNSUP, *result);
		return SCE_ERROR_ERRNO_EUNSUP;
	}
	if (nbyte == 0 || offset >= (SceOff)vp->core.size) {
		*result = 0;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PREAD + 1, 3, vp, 0, 0);
		return 0;
	}
	if (data == NULL)
		goto inval;
	vfs_set_mnt_sync_request(vp->core.mnt);
	if (vp->core.bc != NULL) {
		ret = buff_cache_read(offset, data, nbyte, vp, file, 1, result);
		if (ret != -1) {
			VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PREAD + 1, 3, vp, ret, *result);
			return ret;
		}
	}
	args.vp = vp;
	args.file = file;
	args.buf = data;
	args.nbyte = nbyte;
	args.offset = offset;
	n = ops->vop_pread(&args);
	ret = n < 0 ? n : 0;
	*result = n;
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PREAD + 1, 3, vp, ret, n);
	return ret;
inval:
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PREAD + 1, 3, vp, SCE_ERROR_ERRNO_EINVAL, *result);
	return SCE_ERROR_ERRNO_EINVAL;
}

int sceVopPwrite(SceVfsVnode *vp, SceVfsFile *file, const void *data, SceSize nbyte,
		  SceOff offset, SceSize *result)
{
	struct BuffCache *bc = vp->core.bc;
	SceVfsMount *mnt = vp->core.mnt;
	SceVopPwriteArgs args;
	SceVopTable *ops;
	SceOff position;
	SceSSize n;
	int ret, err;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 2347, "sceVopPwrite");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PWRITE, 10, vp, file, data, nbyte,
			 (SceUInt32)((SceUInt64)vp->core.size >> 32), (SceUInt32)vp->core.size,
			 (SceUInt32)((SceUInt64)file->fpos >> 32), (SceUInt32)file->fpos,
			 (SceUInt32)((SceUInt64)offset >> 32), (SceUInt32)offset);
	if (offset < 0)
		goto inval;
	ret = vfs_get_file_state_error(file);
	if (ret < 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PWRITE + 1, 3, vp, ret, *result);
		return ret;
	}
	if (vp->core.state > 0xfff) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PWRITE + 1, 3, vp, SCE_ERROR_ERRNO_EPERM, *result);
		return SCE_ERROR_ERRNO_EPERM;
	}
	ops = vp->core.ops;
	if (ops->vop_pwrite == NULL) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PWRITE + 1, 3, vp, SCE_ERROR_ERRNO_EUNSUP, *result);
		return SCE_ERROR_ERRNO_EUNSUP;
	}
	if (nbyte == 0) {
		*result = 0;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PWRITE + 1, 3, vp, 0, 0);
		return 0;
	}
	if (data == NULL)
		goto inval;
	vfs_set_mnt_sync_request(vp->core.mnt);
	if (bc == NULL)
		goto direct;
	if ((SceOff)vp->core.size < offset) {
		/* the gap up to the new offset reads back as zeroes */
		position = file->fpos;
		file->fpos = offset;
		ret = buff_cache_fill_zero(bc, vp->core.size, vp, file);
		file->fpos = position;
		if (ret < 0) {
			if (ret == -1)
				goto direct;
			goto out;
		}
	}
	ret = buff_cache_write(offset, data, nbyte, vp, file, 1, result);
	if (ret == -1)
		goto direct;
	if ((mnt->mntFlags & SCE_VFS_MOUNT_FLAG_WRITE_CACHE) == 0) {
		err = vfsBuffDataFlushAll(bc, vp, file);
		if (err < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 2469, "sceVopPwrite",
						  "vfsBuffDataFlushAll() failed ... 0x%x %d %p\n",
						  err, err, vp);
			VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PWRITE + 1, 3, vp, err, *result);
			return err;
		}
		err = vfsBuffIoFlush(bc, vp, file);
		if (err < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 2478, "sceVopPwrite",
						  "vfsBuffIoFlush() failed ... 0x%x %d %p\n",
						  err, err, vp);
			VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PWRITE + 1, 3, vp, err, *result);
			return err;
		}
	}
	goto out;
direct:
	args.vp = vp;
	args.file = file;
	args.buf = data;
	args.nbyte = nbyte;
	args.offset = offset;
	n = ops->vop_pwrite(&args);
	ret = n < 0 ? n : 0;
	*result = n;
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PWRITE + 1, 3, vp, ret, n);
	return ret;
out:
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PWRITE + 1, 3, vp, ret, *result);
	return ret;
inval:
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_PWRITE + 1, 3, vp, SCE_ERROR_ERRNO_EINVAL, *result);
	return SCE_ERROR_ERRNO_EINVAL;
}

SceOff sceVopLseek(SceVfsVnode *vp, SceVfsFile *file, SceOff offset, int whence)
{
	SceVopLseekArgs args;
	SceVopTable *ops;
	SceOff (*op)(SceVopLseekArgs *argp);
	SceOff base, pos;
	int err;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 2557, "sceVopLseek");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_LSEEK, 5, vp, file,
			 (SceUInt32)((SceUInt64)offset >> 32), (SceUInt32)offset, whence);
	err = vfs_get_file_state_error(file);
	if (err < 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_LSEEK + 1, 3, vp, 0, err);
		return err;
	}
	if (vp->core.state > 0xfff) {
		err = SCE_ERROR_ERRNO_EPERM;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_LSEEK + 1, 3, vp, 0, err);
		return err;
	}
	ops = vp->core.ops;
	switch (whence) {
	case SCE_SEEK_CUR:
		base = file->fpos;
		break;
	case SCE_SEEK_END:
		base = vp->core.size;
		break;
	case SCE_SEEK_SET:
		base = 0;
		break;
	default:
		err = SCE_ERROR_ERRNO_EINVAL;
		return err;
	}
	pos = base + offset;
	if (pos == 0x7fffffffffffffffLL || pos < 0) {
		err = SCE_ERROR_ERRNO_EINVAL;
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_LSEEK + 1, 3, vp, err);
		return err;
	}
	op = ops->vop_lseek;
	if (op == NULL) {
		pos = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.vp = vp;
		args.file = file;
		args.offset = offset;
		args.whence = whence;
		pos = op(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_LSEEK + 1, 3, vp,
			 (SceUInt32)((SceUInt64)pos >> 32), (SceUInt32)pos);
	return pos;}

int sceVopMkdir(SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path, int mode)
{
	SceVfsMount *mnt = dvp->core.mnt;
	SceVopMkdirArgs args;
	SceVopTable *ops;
	int (*op)(SceVopMkdirArgs *argp);
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(dvp) == 1, 2656, "sceVopMkdir");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_MKDIR, 3, dvp, mode, path->path);
	if (dvp->core.state >= SCE_VNODE_STATE_INACTIVE) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_MKDIR + 1, 2, dvp, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	ops = dvp->core.ops;
	if ((mnt->mntFlags & SCE_VFS_MOUNT_FLAG_RDONLY) != 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_MKDIR + 1, 2, dvp, SCE_ERROR_ERRNO_EROFS);
		return SCE_ERROR_ERRNO_EROFS;
	}
	op = ops->vop_mkdir;
	if (op == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.dvp = dvp;
		args.vpp = vpp;
		args.path = path;
		args.mode = mode;
		ret = op(&args);
		if (ret == 0)
			SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(*vpp) == 1, 2687, "sceVopMkdir");
		vfs_set_mnt_sync_request(dvp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_MKDIR + 1, 2, dvp, ret);
	return ret;
}

int sceVopRmdir(SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path, SceUInt32 flags)
{
	SceVfsMount *mnt = dvp->core.mnt;
	SceVopTable *ops;
	SceVopRmdirArgs args;
	int (*op)(SceVopRmdirArgs *argp);
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(dvp) == 1, 2728, "sceVopRmdir");
	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 2729, "sceVopRmdir");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_RMDIR, 3, dvp, flags, path->path);
	if (dvp->core.state >= SCE_VNODE_STATE_INACTIVE)
		goto eperm;
	ops = dvp->core.ops;
	if (vp->core.state >= SCE_VNODE_STATE_INACTIVE) {
eperm:
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_RMDIR + 1, 2, dvp, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	if ((mnt->mntFlags & SCE_VFS_MOUNT_FLAG_RDONLY) != 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_RMDIR + 1, 2, dvp, SCE_ERROR_ERRNO_EROFS);
		return SCE_ERROR_ERRNO_EROFS;
	}
	op = ops->vop_rmdir;
	if (op == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.dvp = dvp;
		args.vp = vp;
		args.path = path;
		ret = op(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_RMDIR + 1, 2, dvp, ret);
	return ret;
}

int sceVopInactive(SceVfsVnode *vp)
{
	SceVopTable *ops = vp->core.ops;
	SceVopInactiveArgs args;
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 2795, "sceVopInactive");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_INACTIVE, 1, vp);
	if (ops->vop_inactive == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.vp = vp;
		ret = ops->vop_inactive(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_INACTIVE + 1, 2, vp, ret);
	return ret;
}

int sceVopGetstat(SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat)
{
	SceVopTable *ops = vp->core.ops;
	SceVopGetstatArgs args;
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 2843, "sceVopGetstat");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_GETSTAT, 2, vp,
			  path->path != NULL ? path->path : path->name);
	if (stat == NULL) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_GETSTAT + 1, 6, vp, SCE_ERROR_ERRNO_EINVAL, 0, 0,
				     0, 0);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (ops->vop_getstat == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.vp = vp;
		args.path = path;
		args.stat = stat;
		ret = ops->vop_getstat(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
		/* a file being written through the cache is bigger than the media says */
		if (ret == 0 && stat->st_size != (SceOff)vp->core.size && vp->core.bc != NULL &&
		    (vp->core.mnt->mntFlags & SCE_VFS_MOUNT_FLAG_WRITE_CACHE) != 0)
			stat->st_size = vp->core.size;
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_GETSTAT + 1, 6, vp, ret, stat->st_mode, stat->st_attr,
			 (SceUInt32)((SceUInt64)stat->st_size >> 32), (SceUInt32)stat->st_size);
	return ret;
}

int sceVopRemove(SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path, SceUInt32 flags)
{
	SceVopRemoveArgs args;
	SceVfsMount *mnt;
	SceVopTable *ops;
	int ret;

	if (dvp != NULL)
		SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(dvp) == 1, 2916, "sceVopRemove");
	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 2918, "sceVopRemove");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_REMOVE, 3, dvp, flags, path->path);
	if (dvp == NULL) {
		mnt = vp->core.mnt;
		ops = vp->core.ops;
	} else {
		mnt = dvp->core.mnt;
		if (dvp->core.state >= SCE_VNODE_STATE_INACTIVE)
			goto perm;
		ops = dvp->core.ops;
	}
	if ((mnt->mntFlags & SCE_VFS_MOUNT_FLAG_RDONLY) != 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_REMOVE + 1, 2, dvp, SCE_ERROR_ERRNO_EROFS);
		return SCE_ERROR_ERRNO_EROFS;
	}
	if (vp->core.state >= SCE_VNODE_STATE_INACTIVE && (vp->core.state & SCE_VNODE_STATE_DELETED) == 0)
		goto perm;
	if (ops->vop_remove == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.dvp = dvp;
		args.vp = vp;
		args.path = path;
		args.flags = flags;
		ret = ops->vop_remove(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_REMOVE + 1, 2, dvp, ret);
	return ret;
perm:
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_REMOVE + 1, 2, dvp, SCE_ERROR_ERRNO_EPERM);
	return SCE_ERROR_ERRNO_EPERM;
}

int sceVopRename(SceVfsVnode *odvp, SceVfsVnode *ovp, SceVfsPath *old_path, SceVfsVnode *ndvp,
		  SceVfsVnode **nvpp, SceVfsPath *new_path)
{
	SceVfsMount *mnt = odvp->core.mnt;
	SceVopRenameArgs args;
	SceVopTable *ops;
	int (*op)(SceVopRenameArgs *argp);
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(odvp) == 1, 3019, "sceVopRename");
	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(ovp) == 1, 3020, "sceVopRename");
	if (ndvp != NULL)
		SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(ndvp) == 1, 3022, "sceVopRename");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_RENAME, 3, odvp, old_path->path, new_path->path);
	if (odvp->core.state >= SCE_VNODE_STATE_INACTIVE)
		goto eperm;
	ops = odvp->core.ops;
	if (ovp->core.state >= SCE_VNODE_STATE_INACTIVE ||
	    (ndvp != NULL && ndvp->core.state >= SCE_VNODE_STATE_INACTIVE)) {
eperm:
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_RENAME + 1, 2, odvp, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	if ((mnt->mntFlags & SCE_VFS_MOUNT_FLAG_RDONLY) != 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_RENAME + 1, 2, odvp, SCE_ERROR_ERRNO_EROFS);
		return SCE_ERROR_ERRNO_EROFS;
	}
	op = ops->vop_rename;
	if (op == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.odvp = odvp;
		args.ovp = ovp;
		args.oldPath = old_path;
		args.ndvp = ndvp;
		args.nvpp = nvpp;
		args.newPath = new_path;
		ret = op(&args);
		if (ret == 0)
			SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(*nvpp) == 1, 3071, "sceVopRename");
		vfs_set_mnt_sync_request(ovp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_RENAME + 1, 2, odvp, ret);
	return ret;
}

int sceVopDopen(SceVfsVnode *vp, SceVfsPath *path, SceVfsFile *file)
{
	SceVopDopenArgs args;
	SceVopTable *ops;
	int (*op)(SceVopDopenArgs *argp);
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 3108, "sceVopDopen");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DOPEN, 2, vp, path->path);
	if (vp->core.state >= SCE_VNODE_STATE_INACTIVE) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DOPEN + 1, 2, vp, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	ops = vp->core.ops;
	ret = SCE_ERROR_ERRNO_EUNSUP;
	op = ops->vop_dopen;
	if (op != NULL) {
		args.vp = vp;
		args.path = path;
		args.file = file;
		ret = op(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
		if (sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART) != 0)
			vfs_debug_path_create(file, path->path);
	}
	VFS_VOP_HOOK_CALL_PLAIN(VFS_VOP_HOOK_DOPEN + 1, 2, vp, ret);
	return ret;
}

int sceVopDread(SceVfsVnode *vp, SceVfsFile *file, SceIoDirent *dir)
{
	SceVopDreadArgs args;
	int (*op)(SceVopDreadArgs *argp);
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 3163, "sceVopDread");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DREAD, 2, vp, dir);
	if (dir == NULL) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DREAD + 1, 2, vp, SCE_ERROR_ERRNO_EINVAL);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	ret = vfs_get_file_state_error(file);
	if (ret < 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DREAD + 1, 2, vp, ret);
		return ret;
	}
	if (vp->core.state > 0xfff) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DREAD + 1, 2, vp, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	op = vp->core.ops->vop_dread;
	if (op == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.vp = vp;
		args.file = file;
		args.dir = dir;
		ret = op(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DREAD + 1, 2, vp, ret);
	return ret;
}

int sceVopDclose(SceVfsVnode *vp, SceVfsFile *file)
{
	SceVopDcloseArgs args;
	int (*op)(SceVopDcloseArgs *argp);
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 3224, "sceVopDclose");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DCLOSE, 1, vp);
	ret = vfs_get_file_closed_error(file);
	if (ret < 0) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DCLOSE + 1, 2, vp, ret);
		return ret;
	}
	if (vp->core.state > 0xfff) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DCLOSE + 1, 2, vp, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	op = vp->core.ops->vop_dclose;
	if (op == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.vp = vp;
		args.file = file;
		ret = op(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_DCLOSE + 1, 2, vp, ret);
	return ret;
}

int sceVopChstat(SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat, int bit)
{
	SceVopTable *ops = vp->core.ops;
	struct BuffCache *bc = vp->core.bc;
	SceVopChstatArgs args;
	SceOff last = 0, newSize = 0, size = 0;
	int invalidate = 0, refetch = 0;
	int ret, err;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 3298, "sceVopChstat");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CHSTAT, 4, vp, stat, bit, path->path);
	if (stat == NULL) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CHSTAT + 1, 2, vp, SCE_ERROR_ERRNO_EINVAL);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (ops->vop_chstat == NULL) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CHSTAT + 1, 2, vp, SCE_ERROR_ERRNO_EUNSUP);
		return SCE_ERROR_ERRNO_EUNSUP;
	}
	if (vp->core.mnt->mntFlags & SCE_VFS_MOUNT_FLAG_RDONLY) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CHSTAT + 1, 2, vp, SCE_ERROR_ERRNO_EROFS);
		return SCE_ERROR_ERRNO_EROFS;
	}
	vfs_set_mnt_sync_request(vp->core.mnt);

	if ((bit & SCE_CST_SIZE) && bc != NULL) {
		/* a new size: the cached data past it goes after the call */
		size = vp->core.size;
		newSize = stat->st_size;
		if (newSize >= size) {
			refetch = bc->ioBuffState != VFS_BUFF_LINE_FREE;
			newSize = 0;
		} else {
			last = newSize - 1;
			if (size & 0x1ff) {
				if (vfs_buf_data_cached(bc, last)) {
					ret = vfsBuffDataFlush(bc, last, 0, vp, vp->core.fdList);
					if (ret < 0)
						goto out;
				}
				if (vfs_buf_io_cached_contains(bc, last, 1)) {
					ret = vfsBuffIoFlush(bc, vp, vp->core.fdList);
					if (ret < 0)
						goto out;
				}
			}
			invalidate = 1;
		}
	}
	if ((bit & SCE_CST_MODE) && bc != NULL && (stat->st_mode & (IO_STM_WU | IO_STM_WS)) == 0) {
		/* it becomes read-only: write the cache back first */
		ret = vfsBuffDataFlushAll(bc, vp, vp->core.fdList);
		if (ret < 0)
			goto out;
		ret = vfsBuffIoFlush(bc, vp, vp->core.fdList);
		if (ret < 0)
			goto out;
	}

	args.vp = vp;
	args.path = path;
	args.stat = stat;
	args.bit = bit;
	ret = ops->vop_chstat(&args);
	if (ret == 0) {
		if (invalidate) {
			if (newSize == 0) {
				err = vfs_buf_data_invalidate_all(bc, vp);
				if (err < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0xf, 3417, "sceVopChstat",
								  "Data buff invalidate failed ... %d 0x%x file end=0x%llx\n",
								  err, err, newSize);
				bc->ioBuffState = VFS_BUFF_LINE_INVALID;
			} else {
				err = vfs_buf_area_invalidate(bc, last, size - 1, vp);
				if (err < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0xf, 3428, "sceVopChstat",
								  "Data buff invalidate failed ... %d 0x%x file end=0x%llx\n",
								  err, err, last);
				if (last < bc->ioBuffOffset ||
				    vfs_buf_io_cached_overlaps(bc, last, size - last))
					bc->ioBuffState = VFS_BUFF_LINE_INVALID;
			}
		} else if (refetch && bc->ioBuffCachedSize != bc->ioBuffSize) {
			vfsBuffIoFetch(bc, bc->ioBuffOffset, vp, vp->core.fdList);
		}
	}
out:
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CHSTAT + 1, 2, vp, ret);
	return ret;
}

int sceVopIoctl(SceVfsVnode *vp, SceVfsFile *file, int cmd, const void *in_data, SceSize in_len,
		 void *out_data, SceSize out_len)
{
	SceVopTable *ops = vp->core.ops;
	SceVopIoctlArgs args;
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 3484, "sceVopIoctl");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_IOCTL, 7, vp, file, cmd, in_data, in_len, out_data, out_len);
	if (ops->vop_ioctl == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.vp = vp;
		args.file = file;
		args.cmd = cmd;
		args.inData = in_data;
		args.inLen = in_len;
		args.outData = out_data;
		args.outLen = out_len;
		ret = ops->vop_ioctl(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_IOCTL + 1, 2, vp, ret);
	return ret;
}

int SceIofilemgrForDriver_942AA61F(SceVfsVnode *fvp, SceVfsVnode *tvp)
{
	SceVopLinkArgs args;
	int (*op)(SceVopLinkArgs *argp) = fvp->core.ops->vop_link;

	if (op == NULL)
		return SCE_ERROR_ERRNO_EUNSUP;
	args.fvp = fvp;
	args.tvp = tvp;
	return op(&args);
}

int SceIofilemgrForDriver_0D8A806E(SceVfsVnode *fvp, SceVfsVnode *tvp)
{
	SceVopUnlinkArgs args;
	int (*op)(SceVopUnlinkArgs *argp);

	op = fvp->core.ops->vop_unlink;
	if (op == NULL)
		return SCE_ERROR_ERRNO_EUNSUP;
	args.fvp = fvp;
	args.tvp = tvp;
	return op(&args);
}

int sceVopSync(SceVfsVnode *vp, SceVfsFile *file, int flags)
{
	SceVopTable *ops = vp->core.ops;
	SceVopSyncArgs args;
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 3592, "sceVopSync");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_SYNC, 3, vp, file, flags);
	if (ops->vop_sync == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		if (file != NULL && vp->core.bc != NULL) {
			ret = vfsBuffDataFlushAll(vp->core.bc, vp, file);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 3605, "sceVopSync",
							  "vfsBuffDataFlushAll() failed ... 0x%x %d %p\n",
							  ret, ret, vp);
				VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_SYNC + 1, 2, vp, ret);
				return ret;
			}
			ret = vfsBuffIoFlush(vp->core.bc, vp, file);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 3614, "sceVopSync",
							  "vfsBuffIoFlush() failed ... 0x%x %d %p\n",
							  ret, ret, vp);
				VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_SYNC + 1, 2, vp, ret);
				return ret;
			}
		}
		args.vp = vp;
		args.file = file;
		args.flags = flags;
		ret = ops->vop_sync(&args);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_SYNC + 1, 2, vp, ret);
	return ret;
}

int sceVopFgetstat(SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat)
{
	SceVopTable *ops = vp->core.ops;
	SceVopFgetstatArgs args;
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 3655, "sceVopFgetstat");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_FGETSTAT, 3, vp, file, stat);
	if (stat == NULL) {
		/* Sony reports the result under the call's own event */
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_FGETSTAT, 2, vp, SCE_ERROR_ERRNO_EINVAL);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (ops->vop_fgetstat == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.vp = vp;
		args.file = file;
		args.stat = stat;
		ret = ops->vop_fgetstat(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
		/* a file being written through the cache is bigger than the media says */
		if (ret == 0 && stat->st_size != (SceOff)vp->core.size && vp->core.bc != NULL &&
		    (vp->core.mnt->mntFlags & SCE_VFS_MOUNT_FLAG_WRITE_CACHE) != 0)
			stat->st_size = vp->core.size;
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_FGETSTAT, 2, vp, ret);
	return ret;
}

int sceVopFchstat(SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat, int bit)
{
	SceVopTable *ops = vp->core.ops;
	struct BuffCache *bc = vp->core.bc;
	SceVopFchstatArgs args;
	SceOff last = 0, newSize = 0, size = 0;
	int invalidate = 0, refetch = 0;
	int ret, err;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 3722, "sceVopFchstat");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_FCHSTAT, 4, vp, file, stat, bit);
	if (stat == NULL) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_FCHSTAT + 1, 2, vp, SCE_ERROR_ERRNO_EINVAL);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (ops->vop_fchstat == NULL) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_FCHSTAT + 1, 2, vp, SCE_ERROR_ERRNO_EUNSUP);
		return SCE_ERROR_ERRNO_EUNSUP;
	}
	if (vp->core.mnt->mntFlags & SCE_VFS_MOUNT_FLAG_RDONLY) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_FCHSTAT + 1, 2, vp, SCE_ERROR_ERRNO_EROFS);
		return SCE_ERROR_ERRNO_EROFS;
	}
	vfs_set_mnt_sync_request(vp->core.mnt);

	if ((bit & SCE_CST_SIZE) && bc != NULL) {
		/* a new size: the cached data past it goes after the call */
		size = vp->core.size;
		newSize = stat->st_size;
		if (newSize >= size) {
			refetch = bc->ioBuffState != VFS_BUFF_LINE_FREE;
			newSize = 0;
		} else {
			last = newSize - 1;
			if (size & 0x1ff) {
				if (vfs_buf_data_cached(bc, last)) {
					err = vfsBuffDataFlush(bc, last, 0, vp, file);
					if (err < 0)
						goto fail;
				}
				if (vfs_buf_io_cached_contains(bc, last, 1)) {
					err = vfsBuffIoFlush(bc, vp, file);
					if (err < 0)
						goto fail;
				}
			}
			invalidate = 1;
		}
	}
	if ((bit & SCE_CST_MODE) && bc != NULL && (stat->st_mode & (IO_STM_WU | IO_STM_WS)) == 0) {
		/* it becomes read-only: write the cache back first */
		err = vfsBuffDataFlushAll(bc, vp, file);
		if (err < 0)
			goto fail;
		err = vfsBuffIoFlush(bc, vp, file);
		if (err < 0)
			goto fail;
	}

	args.vp = vp;
	args.file = file;
	args.stat = stat;
	args.bit = bit;
	ret = ops->vop_fchstat(&args);
	if (ret == 0) {
		if (invalidate) {
			if (newSize == 0) {
				err = vfs_buf_data_invalidate_all(bc, vp);
				if (err < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 3841,
							     "Data buff invalidate failed ... %d 0x%x file end=0x%llx\n",
							     err, err, newSize);
				bc->ioBuffState = VFS_BUFF_LINE_INVALID;
			} else {
				err = vfs_buf_area_invalidate(bc, last, size - 1, vp);
				if (err < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 3853,
							     "Data buff invalidate failed ... %d 0x%x file end=0x%llx\n",
							     err, err, last);
				if (last < bc->ioBuffOffset ||
				    vfs_buf_io_cached_overlaps(bc, last, size - last))
					bc->ioBuffState = VFS_BUFF_LINE_INVALID;
			}
		} else if (refetch && bc->ioBuffCachedSize != bc->ioBuffSize) {
			vfsBuffIoFetch(bc, bc->ioBuffOffset, vp, vp->core.fdList);
		}
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_FCHSTAT + 1, 2, vp, ret);
	return ret;
fail:
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_FCHSTAT + 1, 2, vp, err);
	return err;
}

int sceVopWhiteout(SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *old_path,
		    SceVfsPath *new_path, SceUInt32 a4, SceUInt32 a5)
{
	SceVopTable *ops = vp->core.ops;
	SceVopWhiteoutArgs args;
	int ret;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(dvp) == 1, 3910, "sceVopWhiteout");
	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 3911, "sceVopWhiteout");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WHITEOUT, 3, dvp, old_path->path, new_path->path);
	if (ops->vop_whiteout == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		/* a4 and a5 are not passed on: the file system always sees 0 */
		args.dvp = dvp;
		args.vp = vp;
		args.oldPath = old_path;
		args.newPath = new_path;
		args.a4 = 0;
		args.a5 = 0;
		/* vitasdk types the table entry with its shorter args */
		ret = ((int (*)(SceVopWhiteoutArgs *))ops->vop_whiteout)(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
	}
	/* Sony's event for the result is 0x5a, one past the pair */
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_WHITEOUT + 2, 2, dvp, ret);
	return ret;
}

int sceVopCleanup(SceVfsVnode *vp, SceVfsFile *file, SceUInt32 a2, SceUInt32 a3)
{
	SceVopTable *ops;
	int fdNum;
	SceVfsFile *other;
	SceUID pid, filePid;
	int ret, err;

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 3960, "sceVopCleanup");
	SCE_DBG_ASSERT_FUNC(a2 == 0, 3962, "sceVopCleanup");
	SCE_DBG_ASSERT_FUNC(a3 == 0, 3963, "sceVopCleanup");
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CLEANUP, 2, vp, file);
	if (vp->core.state > 0xfff) {
		VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CLEANUP + 1, 2, vp, SCE_ERROR_ERRNO_EPERM);
		return SCE_ERROR_ERRNO_EPERM;
	}
	ops = vp->core.ops;
	if (vp->core.bc != NULL) {
		err = vfsBuffFree(vp->core.bc, 1, vp, file);
		if (err < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0xf, 3987, "sceVopCleanup",
						  "vfsBuffFree() failed ... 0x%x %d %p\n",
						  err, err, vp);
		fdNum = vp->core.fdNum;
		vp->core.bc = NULL;
		/* the cache goes on for the next process that still has the file open */
		if (fdNum > 1) {
			filePid = file->pid;
			for (other = vp->core.fdList; other != NULL; other = other->next) {
				pid = other->pid;
				if (pid == filePid || other == file)
					continue;
				if (pid != 0) {
					struct BuffCache *bc = NULL;

					if (vfs_is_cdialog_thread())
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 0xf, 4016, "sceVopCleanup",
									  "called by CDialog, unexpected sequence !!\n");
					err = vfsBuffAllocate(VFS_BUFF_DEFAULT_DATA_CACHE_SIZE,
							      VFS_BUFF_DEFAULT_WAY_NUM,
							      VFS_BUFF_DEFAULT_LINE_SIZE,
							      vp->core.mnt->defaultIoCacheSize, pid,
							      VFS_BUFF_MEM_DEFAULT, NULL, &bc);
					if (err == 0)
						vp->core.bc = bc;
					else if (err != SCE_ERROR_ERRNO_ENOMEM)
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 0xf, 4030, "sceVopCleanup",
									  "vfsBuffAllocate() returns %d 0x%x\n",
									  err, err);
				}
				break;
			}
		}
	}
	if (ops->vop_cleanup == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		SceVopCleanupArgs args;

		args.vp = vp;
		args.file = file;
		args.a2 = 0;
		args.a3 = 0;
		ret = ((int (*)(SceVopCleanupArgs *))ops->vop_cleanup)(&args);
		vfs_set_mnt_sync_request(vp->core.mnt);
	}
	VFS_VOP_HOOK_CALL(VFS_VOP_HOOK_CLEANUP + 1, 2, vp, ret);
	return ret;
}

int sceVopZerofill(SceVfsVnode *vp, SceUInt64 forwardedValue1, SceUInt64 forwardedValue2,
		   SceUInt64 forwardedValue3)
{
	SceVopZerofillArgs args;
	int (*op)(SceVopZerofillArgs *argp);

	SCE_DBG_ASSERT_CHECK_FUNC(vfsIsOwnerVnode(vp) == 1, 4066, "sceVopZerofill");
	op = vp->core.ops->vop_zerofill;
	if (op == NULL)
		return SCE_ERROR_ERRNO_EUNSUP;
	args.vp = vp;
	args.forwardedValue1 = forwardedValue1;
	args.forwardedValue2 = forwardedValue2;
	args.forwardedValue3 = forwardedValue3;
	return op(&args);
}

/* the VFS operations of a mount point, as its file system registered them */
static inline __attribute__((always_inline)) const SceVfsOpTable *vfs_mnt_ops(SceVfsMount *mnt)
{
	return mnt->mntVfsInf->vfsOps;
}

int sceVfsOpMount(SceVfsMount *mnt, SceVfsPath *devPath)
{
	const SceVfsOpTable *ops = vfs_mnt_ops(mnt);
	SceVfsOpMountArgs args;
	int ret;

	VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_MOUNT, 2, mnt, devPath->path);
	if (ops->vfs_mount == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.mnt = mnt;
		args.devFilePath = devPath;
		ret = ops->vfs_mount(&args);
	}
	VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_MOUNT + 1, 2, mnt, ret);
	return ret;
}

int sceVfsOpUmount(SceVfsMount *mnt, int flags)
{
	const SceVfsOpTable *ops = vfs_mnt_ops(mnt);
	SceVfsOpUmountArgs args;
	int ret;

	VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_UMOUNT, 2, mnt, flags);
	if (ops->vfs_umount == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.mnt = mnt;
		args.flags = flags;
		ret = ops->vfs_umount(&args);
	}
	VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_UMOUNT + 1, 2, mnt, ret);
	return ret;
}

int sceVfsOpInit(SceVfsInfo *vfsInf)
{
	SceVfsOpInitArgs args;
	int (*op)(SceVfsOpInitArgs *argp) = vfsInf->vfsOps->vfs_init;

	if (op == NULL)
		return SCE_ERROR_ERRNO_EUNSUP;
	args.vfsInf = vfsInf;
	return op(&args);
}

int sceVfsOpFini(SceVfsInfo *vfsInf)
{
	SceVfsOpFiniArgs args;
	int (*op)(SceVfsOpFiniArgs *argp) = vfsInf->vfsOps->vfs_fini;

	if (op == NULL)
		return SCE_ERROR_ERRNO_EUNSUP;
	args.vfsInf = vfsInf;
	return op(&args);
}

int sceVfsOpDevctl(SceVfsMount *mnt, const char *dev, unsigned int cmd, const void *indata,
		    SceSize inlen, void *outdata, SceSize outlen)
{
	SceVfsOpDevctlArg args;
	int (*op)(SceVfsOpDevctlArg *argp);
	int ret;

	VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_DEVCTL, 7, mnt, cmd, indata, inlen, outdata, outlen, dev);
	if (mnt->state == VFS_MOUNT_STATE_UMOUNTING) {
		VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_DEVCTL + 1, 2, mnt, SCE_ERROR_ERRNO_ENODEV);
		return SCE_ERROR_ERRNO_ENODEV;
	}
	op = vfs_mnt_ops(mnt)->vfs_devctl;
	if (op == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.mnt = mnt;
		args.dev = dev;
		args.cmd = cmd;
		args.arg = indata;
		args.argLen = inlen;
		args.buf = outdata;
		args.bufLen = outlen;
		ret = op(&args);
	}
	VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_DEVCTL + 1, 2, mnt, ret);
	return ret;
}

int sceVfsOpSetRoot(SceVfsMount *mnt, int unused, SceVfsVnode *vp)
{
	SceVfsOpSetRootArgs args;
	int (*op)(SceVfsOpSetRootArgs *argp) = vfs_mnt_ops(mnt)->vfs_set_root;

	if (op == NULL)
		return SCE_ERROR_ERRNO_EUNSUP;
	args.mnt = mnt;
	args.unused = unused;
	args.vp = vp;
	return op(&args);
}

int sceVfsOpSync(SceVfsMount *mnt, int flags)
{
	SceVfsOpSyncArgs args;
	int (*op)(SceVfsOpSyncArgs *argp);
	int ret;

	VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_SYNC, 2, mnt, flags);
	if (mnt->state == VFS_MOUNT_STATE_UMOUNTING) {
		VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_SYNC + 1, 2, mnt, SCE_ERROR_ERRNO_ENODEV);
		return SCE_ERROR_ERRNO_ENODEV;
	}
	op = vfs_mnt_ops(mnt)->vfs_sync;
	if (op == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.mnt = mnt;
		args.flags = flags;
		ret = op(&args);
	}
	VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_SYNC + 1, 2, mnt, ret);
	return ret;
}

int sceVfsOpDecodePathElem(SceVfsMount *mnt, const char *path, const char **path2,
			    const char **path3, char *buf, SceSize buf_len, SceSize *decode_len)
{
	const SceVfsOpTable *ops = vfs_mnt_ops(mnt);
	SceVfsOpDecodePathElemArgs args;
	int ret;

	VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_DECODE_PATH_ELEM, 4, mnt, buf, buf_len, path);
	if (ops->vfs_decode_path_elem == NULL) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
	} else {
		args.mnt = mnt;
		args.path = path;
		args.path2 = path2;
		args.path3 = path3;
		args.buf = buf;
		args.bufLen = buf_len;
		args.decodeLen = decode_len;
		ret = ops->vfs_decode_path_elem(&args);
	}
	VFS_VOP_HOOK_CALL(VFS_VFSOP_HOOK_DECODE_PATH_ELEM + 1, 2, mnt, ret);
	return ret;
}

void vfs_dump_fast_mutex(SceKernelFastMutex *mtx)
{
#if FW_DEBUG
	ThreadmgrFastMutexInfo info;
	int ret;

	info.size = sizeof(info);
	ret = sceKernelGetFastMutexInfo(mtx, &info);
	if (ret < 0) {
		SceDebugForDriver_391B74B7("can not get fmtx info ... %d 0x%X\n", ret, ret);
		return;
	}
	SceDebugForDriver_391B74B7("============== fast mtx =================\n");
	SceDebugForDriver_391B74B7("size = %d 0x%x\n", info.size, info.size);
	SceDebugForDriver_391B74B7("fastMutexId = %d 0x%x\n", info.uid, info.uid);
	SceDebugForDriver_391B74B7("%s\n", info.name);
	SceDebugForDriver_391B74B7("attr = %d 0x%x\n", info.attr, info.attr);
	SceDebugForDriver_391B74B7("lockCount = %d 0x%x\n", info.lockCount, info.lockCount);
	SceDebugForDriver_391B74B7("currentOwnerId = %d 0x%x\n", info.currentOwnerId, info.currentOwnerId);
	SceDebugForDriver_391B74B7("ceilingPriority = %d 0x%x\n", info.ceilingPriority, info.ceilingPriority);
	SceDebugForDriver_391B74B7("numWaitThreads = %d 0x%x\n", info.numWaitThreads, info.numWaitThreads);
	SceDebugForDriver_391B74B7("numSpinThreads = %d 0x%x\n", info.numSpinThreads, info.numSpinThreads);
	SceDebugForDriver_391B74B7("=========================================\n");
#endif
}

#if FW_DEBUG
/* name unknown */
static __attribute__((noinline)) void vfs_dump_vdlock(SceVfsVnode *vp)
{
	SceDebugForDriver_391B74B7("ownerId=0x%x\n", vp->vdlock.ownerId);
	SceDebugForDriver_391B74B7("waiter=0x%x\n", vp->vdlock.waiter);
	SceDebugForDriver_391B74B7("recursiveCount=0x%x\n", vp->vdlock.recursiveCount);
	SceDebugForDriver_391B74B7("waitId=0x%x\n", vp->vdlock.waitId);
	SceDebugForDriver_391B74B7("waitPattern=0x%x\n", vp->vdlock.waitPattern);
}

#endif

void vfs_dump_vnode(SceVfsVnode *vp, int unused)
{
#if FW_DEBUG
	SceDebugForDriver_391B74B7("============== vnode %p =================\n", vp);
	vfs_dump_vdlock(vp);
	SceDebugForDriver_391B74B7("ops = %p\n", vp->core.ops);
	SceDebugForDriver_391B74B7("nodeInf = %d 0x%x\n", vp->core.nodeInf, vp->core.nodeInf);
	SceDebugForDriver_391B74B7("nodeData = %p\n", vp->core.nodeData);
	SceDebugForDriver_391B74B7("mnt = %p\n", vp->core.mnt);
	SceDebugForDriver_391B74B7("dd = %p\n", vp->core.dd);
	SceDebugForDriver_391B74B7("refCount = %d\n", vp->core.refCount);
	SceDebugForDriver_391B74B7("fid = %lld 0x%llx\n", *(SceUInt64 *)vp->core.fid,
			 *(SceUInt64 *)vp->core.fid);
	SceDebugForDriver_391B74B7("allocator = %d 0x%x\n", vp->core.allocator, vp->core.allocator);
	SceDebugForDriver_391B74B7("ncache = %p\n", vp->core.ncache);
	SceDebugForDriver_391B74B7("state = %d 0x%x\n", vp->core.state, vp->core.state);
	SceDebugForDriver_391B74B7("size = %lld 0x%llx\n", vp->core.size, vp->core.size);
	SceDebugForDriver_391B74B7("type = %d 0x%x\n", vp->core.type, vp->core.type);
	SceDebugForDriver_391B74B7("aclData = %lld 0x%llx\n", *(SceUInt64 *)vp->core.aclData,
			 *(SceUInt64 *)vp->core.aclData);
	SceDebugForDriver_391B74B7("fdList = %p\n", vp->core.fdList);
	SceDebugForDriver_391B74B7("fdNum = %d\n", vp->core.fdNum);
#endif
}
