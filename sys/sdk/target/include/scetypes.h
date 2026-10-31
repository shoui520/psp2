/*
 * scetypes.h: the SDK's basic types, under Sony's names and with Sony's definitions (SDK
 * Release 03.570.011, target/include/scetypes.h; the kernel's DWARF names the header at
 * sys/sdk/target/include/scetypes.h).
 */
#ifndef _SCE_SCETYPES_SCETYPES_H
#define _SCE_SCETYPES_SCETYPES_H

#if !defined(__LANGUAGE_ASSEMBLY)

/* integers */
typedef signed char		SceChar8;
typedef unsigned char		SceUChar8;
typedef signed char		SceInt8;
typedef unsigned char		SceUInt8;
typedef signed short		SceShort16;
typedef unsigned short		SceUShort16;
typedef short			SceInt16;
typedef unsigned short		SceUInt16;
typedef signed int		SceInt32;
typedef unsigned int		SceUInt32;
typedef int			SceInt;
typedef unsigned int		SceUInt;
typedef long long		SceInt64;
typedef unsigned long long	SceUInt64;
typedef long			SceLong;
typedef unsigned long		SceULong;
typedef long long		SceLong64;
typedef unsigned long long	SceULong64;

/* floating point */
typedef float			SceFloat;
typedef float			SceFloat32;
typedef double			SceDouble;
typedef double			SceDouble64;

/* bytes and characters */
typedef signed char		SceSByte;
typedef signed char		SceSByte8;
typedef unsigned char		SceByte;
typedef unsigned char		SceByte8;
typedef unsigned short		SceWChar16;
typedef unsigned int		SceWChar32;

#if defined(__cplusplus) && __cplusplus < 201103L && !defined(_Static_assert)
/* C++03 (SNC) has no static_assert: the C11 checks of the shared headers */
#define _Static_assert(expr, msg)	extern char _StaticAssert[(expr) ? 1 : -1]
#endif

#if defined(__cplusplus)
#define SCE_NULL		(0)
#else
#define SCE_NULL		((void *)0)
#endif

typedef int			SceBool;
#define SCE_FALSE		(0)
#define SCE_TRUE		(1)

typedef int			SceIntPtr;
typedef unsigned int		SceUIntPtr;
typedef void			SceVoid;
typedef void			*ScePVoid;

/*
 * The vector unions carry NEON members when arm_neon.h came first (they then align to 8, as
 * Sony's do).
 */
#if defined(__ARM_NEON__) && (defined(__SNC__) || (defined(__GNUC__) && defined(_GCC_ARM_NEON_H)))
#define SCE_TYPES_NEON	1
#endif

/* vectors */
typedef struct SceIVector2 {
	int x, y;
} SceIVector2;

typedef struct SceFVector2 {
	float x, y;
} SceFVector2;

typedef union SceUVector2 {
	SceIVector2	iv;
	SceFVector2	fv;
#ifdef SCE_TYPES_NEON
	int32x2_t	i32x2;
	float32x2_t	f32x2;
	uint32x2_t	ui32x2;
#endif
} SceUVector2;

typedef struct SceIVector3 {
	int x, y, z;
} SceIVector3;

typedef struct SceFVector3 {
	float x, y, z;
} SceFVector3;

typedef union SceUVector3 {
	SceIVector3	iv;
	SceFVector3	fv;
} SceUVector3;

typedef struct SceIVector4 {
	int x, y, z, w;
} SceIVector4;

typedef struct SceFVector4 {
	float x, y, z, w;
} SceFVector4;

typedef union SceUVector4 {
	SceIVector4	iv;
	SceFVector4	fv;
#ifdef SCE_TYPES_NEON
	int32x4_t	i32x4;
	float32x4_t	f32x4;
	uint32x4_t	ui32x4;
#endif
} SceUVector4;

/* matrices: one vector per column */
typedef struct SceIMatrix2 {
	SceIVector2 x, y;
} SceIMatrix2;

typedef struct SceFMatrix2 {
	SceFVector2 x, y;
} SceFMatrix2;

typedef union SceUMatrix2 {
	SceFMatrix2	fm;
	SceIMatrix2	im;
	SceFVector2	fv[2];
	SceIVector2	iv[2];
	SceUVector2	uv[2];
	float		f[2][2];
	int		i[2][2];
#ifdef SCE_TYPES_NEON
	int32x2x2_t	i32x2x2;
	float32x2x2_t	f32x2x2;
	uint32x2x2_t	ui32x2x2;
#endif
#ifdef SCE_TYPES_USE_UNNAMED_UNION
	struct {
		float f00, f10;
		float f01, f11;
	};
	struct {
		int i00, i10;
		int i01, i11;
	};
#endif
} SceUMatrix2;

typedef struct SceIMatrix3 {
	SceIVector3 x, y, z;
} SceIMatrix3;

typedef struct SceFMatrix3 {
	SceFVector3 x, y, z;
} SceFMatrix3;

typedef union SceUMatrix3 {
	SceFMatrix3	fm;
	SceIMatrix3	im;
	SceFVector3	fv[3];
	SceIVector3	iv[3];
	SceUVector3	uv[3];
	float		f[3][3];
	int		i[3][3];
#ifdef SCE_TYPES_USE_UNNAMED_UNION
	struct {
		float f00, f10, f20;
		float f01, f11, f21;
		float f02, f12, f22;
	};
	struct {
		int i00, i10, i20;
		int i01, i11, i21;
		int i02, i12, i22;
	};
#endif
} SceUMatrix3;

typedef struct SceIMatrix4 {
	SceIVector4 x, y, z, w;
} SceIMatrix4;

typedef struct SceFMatrix4 {
	SceFVector4 x, y, z, w;
} SceFMatrix4;

typedef union SceUMatrix4 {
	SceFMatrix4	fm;
	SceIMatrix4	im;
	SceFVector4	fv[4];
	SceIVector4	iv[4];
	SceUVector4	uv[4];
	float		f[4][4];
	int		i[4][4];
#ifdef SCE_TYPES_NEON
	int32x4x4_t	i32x4x4;
	float32x4x4_t	f32x4x4;
	uint32x4x4_t	ui32x4x4;
#endif
#ifdef SCE_TYPES_USE_UNNAMED_UNION
	struct {
		float f00, f10, f20, f30;
		float f01, f11, f21, f31;
		float f02, f12, f22, f32;
		float f03, f13, f23, f33;
	};
	struct {
		int i00, i10, i20, i30;
		int i01, i11, i21, i31;
		int i02, i12, i22, i32;
		int i03, i13, i23, i33;
	};
#endif
} SceUMatrix4;

typedef struct SceFQuaternion {
	float x, y, z, w;
} SceFQuaternion;

typedef struct SceFColor {
	float r, g, b, a;
} SceFColor;

typedef struct SceFPlane {
	float a, b, c, d;
} SceFPlane;

/* one value seen as each of the types of its size */
typedef union SceUnion32 {
	unsigned int	ui;
	int		i;
	unsigned short	us[2];
	short		s[2];
	unsigned char	uc[4];
	char		c[4];
	float		f;
	void		*p;
} SceUnion32;

typedef union SceUnion64 {
	SceULong64	ull;
	SceLong64	ll;
	unsigned int	ui[2];
	int		i[2];
	unsigned short	us[4];
	short		s[4];
	unsigned char	uc[8];
	char		c[8];
	float		f[2];
	SceFVector2	fv;
	SceIVector2	iv;
#ifdef SCE_TYPES_NEON
	int8x8_t	i8x8;
	int16x4_t	i16x4;
	int32x2_t	i32x2;
	int64x1_t	i64x1;
	float32x2_t	f32x2;
	poly8x8_t	p8x8;
	poly16x4_t	p16x4;
	uint8x8_t	ui8x8;
	uint16x4_t	ui16x4;
	uint32x2_t	ui32x2;
	uint64x1_t	ui64x1;
#endif
} SceUnion64;

typedef union SceUnion128 {
	SceULong64	ull[2];
	SceLong64	ll[2];
	unsigned int	ui[4];
	int		i[4];
	unsigned short	us[8];
	short		s[8];
	unsigned char	uc[16];
	char		c[16];
	float		f[4];
	SceFVector4	fv;
	SceFQuaternion	fq;
	SceFPlane	fp;
	SceFColor	fc;
	SceIVector4	iv;
#ifdef SCE_TYPES_NEON
	int8x16_t	i8x16;
	int16x8_t	i16x8;
	int32x4_t	i32x4;
	int64x2_t	i64x2;
	float32x4_t	f32x4;
	poly8x16_t	p8x16;
	poly16x8_t	p16x8;
	uint8x16_t	ui8x16;
	uint16x8_t	ui16x8;
	uint32x4_t	ui32x4;
	uint64x2_t	ui64x2;
#endif
} SceUnion128;

typedef union SceUnion512 {
	SceUnion32	un32[16];
	SceUnion64	un64[8];
	SceULong64	ull[8];
	SceLong64	ll[8];
	unsigned int	ui[16];
	int		i[16];
	unsigned short	us[32];
	short		s[32];
	unsigned char	uc[64];
	char		c[64];
	float		f[16];
	SceFMatrix4	fm;
	SceIMatrix4	im;
	SceUMatrix4	um;
#ifdef SCE_TYPES_NEON
	int32x4x4_t	i32x4x4;
	float32x4x4_t	f32x4x4;
#endif
} SceUnion512;

#undef SCE_TYPES_NEON

typedef struct SceDateTime {
	unsigned short	year;
	unsigned short	month;
	unsigned short	day;
	unsigned short	hour;
	unsigned short	minute;
	unsigned short	second;
	unsigned int	microsecond;
} SceDateTime;

/* sizes */
typedef unsigned int		SceSize;
typedef SceSize			ScePSize;
typedef SceSize			SceVSize;
typedef int			SceSSize;
typedef SceSSize		ScePSSize;
typedef SceSSize		SceVSSize;

/* an unsigned integer the size of a pointer */
typedef unsigned int		SceUIntVAddr;

/* kernel object identifiers */
typedef int			SceUID;
#if defined(__cplusplus)
#define SCE_UID_INVALID_UID	(static_cast<SceUID>(0xFFFFFFFF))
#else
#define SCE_UID_INVALID_UID	((SceUID)0xFFFFFFFF)
#endif

typedef char			*SceName;
typedef SceInt64		SceOff;
typedef SceUID			ScePID;

#endif /* !__LANGUAGE_ASSEMBLY */

#endif /* _SCE_SCETYPES_SCETYPES_H */
