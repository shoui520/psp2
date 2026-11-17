/*
 * The application parameter keys of SceLsdb and the shell: FNV-1a hashes of the parameters'
 * names, computed at start in every file that includes this header (Sony's header defines them
 * static: each file's static initializer computes all seven).
 */
#ifndef LSDB_KEYS_H
#define LSDB_KEYS_H

template<int N>
struct LsdbKeyHasher {
	static inline unsigned int Hash(const char *name)
	{
		return (LsdbKeyHasher<N - 1>::Hash(name) ^ (unsigned char)name[N - 1]) * 16777619u;
	}
};

template<>
struct LsdbKeyHasher<0> {
	static inline unsigned int Hash(const char *name)
	{
		return 2166136261u;
	}
};

template<int N>
__attribute__((noinline)) inline unsigned int LsdbKeyHash(const char (&name)[N])
{
	return LsdbKeyHasher<N - 1>::Hash(name);
}

static struct {
	unsigned int liveareaPath;
	unsigned int ctime;
	unsigned int lltime;
	unsigned int promoteAttr;
	unsigned int upStatus;
	unsigned int libLocation;
	unsigned int appconf;
} s_lsdbKeys = {
	LsdbKeyHash("#_livearea_path"),
	LsdbKeyHash("#_ctime"),
	LsdbKeyHash("#_lltime"),
	LsdbKeyHash("#_promote_attr"),
	LsdbKeyHash("#_up_status"),
	LsdbKeyHash("#LIBLOCATION"),
	LsdbKeyHash("#appconf"),
};

#endif
