/*
 * inttypes.h: the SDK's integer formats and conversions, with Sony's psp2 definitions (SDK
 * Release 03.570.011, target/include/inttypes.h): the 32-bit types print without a length
 * modifier, the 64-bit ones with ll.
 */
#ifndef _INTTYPES
#define _INTTYPES

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	long long quot;
	long long rem;
} imaxdiv_t;

intmax_t imaxabs(intmax_t i);
imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);
intmax_t strtoimax(const char *__restrict s, char **__restrict endptr, int base);
uintmax_t strtoumax(const char *__restrict s, char **__restrict endptr, int base);

#ifdef __cplusplus
}
#endif

#if !defined(__cplusplus) || defined(__STDC_FORMAT_MACROS)
#define PRId8			"hhd"
#define PRIdLEAST8		"hhd"
#define PRIdFAST8		"hhd"
#define PRId16			"hd"
#define PRIdLEAST16		"hd"
#define PRIdFAST16		"hd"
#define PRId32			"d"
#define PRIdLEAST32		"d"
#define PRIdFAST32		"d"
#define PRId64			"lld"
#define PRIdLEAST64		"lld"
#define PRIdFAST64		"lld"
#define PRIdMAX			"lld"
#define PRIdPTR			"d"

#define PRIi8			"hhi"
#define PRIiLEAST8		"hhi"
#define PRIiFAST8		"hhi"
#define PRIi16			"hi"
#define PRIiLEAST16		"hi"
#define PRIiFAST16		"hi"
#define PRIi32			"i"
#define PRIiLEAST32		"i"
#define PRIiFAST32		"i"
#define PRIi64			"lli"
#define PRIiLEAST64		"lli"
#define PRIiFAST64		"lli"
#define PRIiMAX			"lli"
#define PRIiPTR			"i"

#define PRIo8			"hho"
#define PRIoLEAST8		"hho"
#define PRIoFAST8		"hho"
#define PRIo16			"ho"
#define PRIoLEAST16		"ho"
#define PRIoFAST16		"ho"
#define PRIo32			"o"
#define PRIoLEAST32		"o"
#define PRIoFAST32		"o"
#define PRIo64			"llo"
#define PRIoLEAST64		"llo"
#define PRIoFAST64		"llo"
#define PRIoMAX			"llo"
#define PRIoPTR			"o"

#define PRIu8			"hhu"
#define PRIuLEAST8		"hhu"
#define PRIuFAST8		"hhu"
#define PRIu16			"hu"
#define PRIuLEAST16		"hu"
#define PRIuFAST16		"hu"
#define PRIu32			"u"
#define PRIuLEAST32		"u"
#define PRIuFAST32		"u"
#define PRIu64			"llu"
#define PRIuLEAST64		"llu"
#define PRIuFAST64		"llu"
#define PRIuMAX			"llu"
#define PRIuPTR			"u"

#define PRIx8			"hhx"
#define PRIxLEAST8		"hhx"
#define PRIxFAST8		"hhx"
#define PRIx16			"hx"
#define PRIxLEAST16		"hx"
#define PRIxFAST16		"hx"
#define PRIx32			"x"
#define PRIxLEAST32		"x"
#define PRIxFAST32		"x"
#define PRIx64			"llx"
#define PRIxLEAST64		"llx"
#define PRIxFAST64		"llx"
#define PRIxMAX			"llx"
#define PRIxPTR			"x"

#define PRIX8			"hhX"
#define PRIXLEAST8		"hhX"
#define PRIXFAST8		"hhX"
#define PRIX16			"hX"
#define PRIXLEAST16		"hX"
#define PRIXFAST16		"hX"
#define PRIX32			"X"
#define PRIXLEAST32		"X"
#define PRIXFAST32		"X"
#define PRIX64			"llX"
#define PRIXLEAST64		"llX"
#define PRIXFAST64		"llX"
#define PRIXMAX			"llX"
#define PRIXPTR			"X"

#define SCNd8			"hhd"
#define SCNdLEAST8		"hhd"
#define SCNdFAST8		"hhd"
#define SCNd16			"hd"
#define SCNdLEAST16		"hd"
#define SCNdFAST16		"hd"
#define SCNd32			"d"
#define SCNdLEAST32		"d"
#define SCNdFAST32		"d"
#define SCNd64			"lld"
#define SCNdLEAST64		"lld"
#define SCNdFAST64		"lld"
#define SCNdMAX			"lld"
#define SCNdPTR			"d"

#define SCNi8			"hhi"
#define SCNiLEAST8		"hhi"
#define SCNiFAST8		"hhi"
#define SCNi16			"hi"
#define SCNiLEAST16		"hi"
#define SCNiFAST16		"hi"
#define SCNi32			"i"
#define SCNiLEAST32		"i"
#define SCNiFAST32		"i"
#define SCNi64			"lli"
#define SCNiLEAST64		"lli"
#define SCNiFAST64		"lli"
#define SCNiMAX			"lli"
#define SCNiPTR			"i"

#define SCNo8			"hho"
#define SCNoLEAST8		"hho"
#define SCNoFAST8		"hho"
#define SCNo16			"ho"
#define SCNoLEAST16		"ho"
#define SCNoFAST16		"ho"
#define SCNo32			"o"
#define SCNoLEAST32		"o"
#define SCNoFAST32		"o"
#define SCNo64			"llo"
#define SCNoLEAST64		"llo"
#define SCNoFAST64		"llo"
#define SCNoMAX			"llo"
#define SCNoPTR			"o"

#define SCNu8			"hhu"
#define SCNuLEAST8		"hhu"
#define SCNuFAST8		"hhu"
#define SCNu16			"hu"
#define SCNuLEAST16		"hu"
#define SCNuFAST16		"hu"
#define SCNu32			"u"
#define SCNuLEAST32		"u"
#define SCNuFAST32		"u"
#define SCNu64			"llu"
#define SCNuLEAST64		"llu"
#define SCNuFAST64		"llu"
#define SCNuMAX			"llu"
#define SCNuPTR			"u"

#define SCNx8			"hhx"
#define SCNxLEAST8		"hhx"
#define SCNxFAST8		"hhx"
#define SCNx16			"hx"
#define SCNxLEAST16		"hx"
#define SCNxFAST16		"hx"
#define SCNx32			"x"
#define SCNxLEAST32		"x"
#define SCNxFAST32		"x"
#define SCNx64			"llx"
#define SCNxLEAST64		"llx"
#define SCNxFAST64		"llx"
#define SCNxMAX			"llx"
#define SCNxPTR			"x"
#endif

#endif /* _INTTYPES */
