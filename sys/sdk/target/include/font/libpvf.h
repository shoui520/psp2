/* <- UTF-8 BOM */
/* SCE CONFIDENTIAL
 PlayStation(R)Vita Programmer Tool Runtime Library Release 03.570.011
 *
 * Copyright (C) 2011 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */


/*beginning of libpvf.h*/

#ifndef _SCE_LIBPVF_H_
#define _SCE_LIBPVF_H_


#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif	/* defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus) */


/*standard C include file(s)*/
// nothing

/*include file(s)*/
// nothing


// macro definitions---------------------------------------------------------
// nothing


// constant definitions------------------------------------------------------
#define SCE_PVF_MAX_OPEN (18) /*J 同時にオープン可能なフォントの最大数 */
							  /*E Maximum number of fonts that can be open
								simultaneously */

/*J エラーコード一覧 */
/*E List of Error codes */

/**
 * @j メモリ確保に失敗 @ej
 * @e Failed to allocate memory @ee
 */
#define SCE_PVF_ERROR_NOMEMORY					-2142896127	/* 0x80460001 */

/**
 * @j 無効なライブラリインスタンス @ej
 * @e Invalid library instance @ee
 */
#define SCE_PVF_ERROR_LIBID					-2142896126	/* 0x80460002 */

/**
 * @j 無効な引数 @ej
 * @e Invalid argument @ee
 */
#define SCE_PVF_ERROR_ARG						-2142896125	/* 0x80460003 */

/**
 * @j ファイルがない @ej
 * @e No file @ee
 */
#define SCE_PVF_ERROR_NOFILE					-2142896124	/* 0x80460004 */

/**
 * @j ファイルオープンに失敗 @ej
 * @e Failed to open file @ee
 */
#define SCE_PVF_ERROR_FILEOPEN					-2142896123	/* 0x80460005 */

/**
 * @j ファイルクローズに失敗 @ej
 * @e Failed to close file @ee
 */
#define SCE_PVF_ERROR_FILECLOSE				-2142896122	/* 0x80460006 */

/**
 * @j ファイル読み込みに失敗 @ej
 * @e Failed to read file @ee
 */
#define SCE_PVF_ERROR_FILEREAD					-2142896121	/* 0x80460007 */

/**
 * @j ファイルシークに失敗 @ej
 * @e Failed to perform file seek @ee
 */
#define SCE_PVF_ERROR_FILESEEK					-2142896120	/* 0x80460008 */

/**
 * @j オープンしたフォントが多すぎる @ej
 * @e Too many opened fonts @ee
 */
#define SCE_PVF_ERROR_TOOMANYOPENED			-2142896119	/* 0x80460009 */

/**
 * @j サポートしないフォントのバージョン @ej
 * @e Unsupported font version @ee
 */
#define SCE_PVF_ERROR_ILLEGALVERSION			-2142896118	/* 0x8046000A */

/**
 * @j フォントデータに矛盾がある @ej
 * @e Font data is inconsistent @ee
 */
#define SCE_PVF_ERROR_DATAINCONSISTENT			-2142896117	/* 0x8046000B */

/**
 * @j 使用期限切れ @ej
 * @e Usage period expired @ee
 */
#define SCE_PVF_ERROR_EXPIRED					-2142896116	/* 0x8046000C */

/**
 * @j サポートしていない要因 @ej
 * @e Unsupported error @ee
 */
#define SCE_PVF_ERROR_NOSUPPORT				-2142896114	/* 0x8046000E */

/**
 * @j グリフがない @ej
 * @e No glyph @ee
 */
#define SCE_PVF_ERROR_NOGLYPH					-2142896115	/* 0x8046000D */

/**
 * @j 未知のエラー @ej
 * @e Unknown error @ee
 */
#define SCE_PVF_ERROR_UNKNOWN					-2142830593	/* 0x8046FFFF */

/*J フォントファミリーコード一覧 */
/*E List of Font Family Codes */
typedef enum ScePvfFamilyCode {
	SCE_PVF_DEFAULT_FAMILY_CODE=(0), /*J システム標準 */
									  /*E System standard */

	SCE_PVF_FAMILY_SANSERIF=(1),
	SCE_PVF_FAMILY_SERIF=(2),

	SCE_PVF_FAMILY_ROUNDED=(3)
} ScePvfFamilyCode;

/*J スタイルコード一覧 */
/*E List of Style Codes */
typedef enum ScePvfStyleCode {
	SCE_PVF_DEFAULT_STYLE_CODE=(0), /*J システム標準 */
									 /*E System standard */

	SCE_PVF_STYLE_REGULAR=(1), /*J 標準デザイン */
								/*E Regular */
	SCE_PVF_STYLE_OBLIQUE=(2), /*J 斜体 */
								/*E Oblique	 */
	SCE_PVF_STYLE_NARROW=(3), /*J 細体 */
							   /*E Narrow  */
	SCE_PVF_STYLE_NARROW_OBLIQUE=(4), /*J 細斜体 */
									   /*E Narrow oblique  */
	SCE_PVF_STYLE_BOLD=(5), /*J 太字体 */
							 /*E Bold */
	SCE_PVF_STYLE_BOLD_OBLIQUE=(6), /*J 太字斜体 */
									 /*E Bold oblique */
	SCE_PVF_STYLE_BLACK=(7), /*J より太い太字体 */
							  /*E Thicker bold	*/
	SCE_PVF_STYLE_BLACK_OBLIQUE=(8), /*J より太い太字斜体 */
									  /*E Thicker bold oblique */

	/*J 注)以下のスタイルコードは主に和文フォントの指定時に使用する */
	/*E Note) The following style codes are mainly used when specifying Chinese fonts */
	SCE_PVF_STYLE_L=(101),	 /*J 細い */
							 /*E Narrower */
	SCE_PVF_STYLE_M=(102),	 /*J ↑ */
							 /*E ^	*/
	SCE_PVF_STYLE_DB=(103), /*J ｜ */
							 /*E |	*/
	SCE_PVF_STYLE_B=(104),	 /*J ｜ */
							 /*E |	*/
	SCE_PVF_STYLE_EB=(105), /*J ↓ */
							 /*E V	*/
	SCE_PVF_STYLE_UB=(106) /*J 太い */
							/*E Bolder */
} ScePvfStyleCode;

/*J pspFont_t_userImageBufferRec の pixelFormat に与える値 */
/*E Values assigned to pixelFormat in pspFont_t_userImageBufferRec	*/
typedef enum ScePvfImageByfferPixelFormatType {
	SCE_PVF_USERIMAGE_DIRECT4_L=(0), /*J 8ビット中下位4ビットが画面の左側に配置さ
										れるダイレクトカラーグレイスケール4ビット */
									  /*E The lower 4 bits are the direct color
										grayscale bits, which are located on the
										left side of the screen */
	SCE_PVF_USERIMAGE_DIRECT8=(2) /*J 256 色のダイレクトカラーグレイスケール8ビット */
									/*E 256-color direct color grayscale (8 bits) */
} ScePvfImageByfferPixelFormatType;

/*J 言語コード (フォント利用言語) */
/*E Language code (languages for fonts) */
typedef enum ScePvfLanguageCode {
	SCE_PVF_DEFAULT_LANGUAGE_CODE=(0), /*J 言語無指定 */
										/*E Language not specified */
	SCE_PVF_LANGUAGE_J=(1), /*J 日本語 */
							 /*E Japanese */
	SCE_PVF_LANGUAGE_LATIN=(2), /*J 英語 */
								 /*E English */
	SCE_PVF_LANGUAGE_K=(3), /*J 韓国語 */
							 /*E Korean */
	SCE_PVF_LANGUAGE_C=(4), /*J 中国語 */
							/*E Chinese */
	SCE_PVF_LANGUAGE_CJK=(5) /*J 日本語／韓国語全対応 */
							  /*E Japanese and Korean support */
} ScePvfLanguageCode;

/*J リージョンコード(フォント利用地域) */
/*E Region code (regions for fonts) */
typedef enum ScePvfRegionCode {
	SCE_PVF_GENERIC_REGION_CODE=(0), /*J リージョン無指定 */
									  /*E Region not specified */
	SCE_PVF_REGION_001=(1), /*J リージョン 001 (日本) */
							 /*E Region 001 (Japan) */
	SCE_PVF_REGION_002=(2), /*J リージョン 002(地域未定) */
							 /*E Region 002 (indeterminate) */
	SCE_PVF_REGION_003=(3), /*J リージョン 003(地域未定) */
							 /*E Region 003 (indeterminate) */
	SCE_PVF_REGION_004=(4), /*J リージョン 004(地域未定) */
							 /*E Region 004 (indeterminate) */
	SCE_PVF_REGION_005=(5), /*J リージョン 005(地域未定) */
							 /*E Region 005 (indeterminate) */
	SCE_PVF_REGION_006=(6), /*J リージョン 006(地域未定) */
							 /*E Region 006 (indeterminate) */
	SCE_PVF_REGION_007=(7) /*J リージョン 007(地域未定) */
							/*E Region 007 (indeterminate) */
} ScePvfRegionCode;

/*J 国コード (フォントベンダー国) */
/*E Country code (font vendor's country) */
typedef enum ScePvfFontVendorCountryCode {
	SCE_PVF_GENERIC_COUNTRY_CODE=(0), /*J 国無指定 */
									   /*E Country not specified */
	SCE_PVF_COUNTRY_JAPAN=(1), /*J 日本 */
								/*E Japan */
	SCE_PVF_COUNTRY_USA=(2), /*J アメリカ */
							  /*E United States of America */
	SCE_PVF_COUNTRY_KOREA=(3) /*J 韓国 */
							   /*E Korea */
} ScePvfFontVendorCountryCode;

typedef enum ScePvfBoolValue { /*J 論理値 */
			/*E Boolean value  */
	SCE_PVF_FALSE=(0),
	SCE_PVF_TRUE=(1)
} ScePvfBoolValue;

/*J サブスタイル属性のマスク値 (主にラスタ処理による太字化や旋断の指定) */
/*E Substyle attribute mask (mainly for specifying bold and italics
  for raster processing)	*/
#define SCE_PVF_SUBSTYLE_VERTICALLAYOUT (0x0001) /*J 縦組レイアウト用書体 */
												  /*E Typeface for vertical layout	*/
#define SCE_PVF_SUBSTYLE_PSEUDO_BOLD (0x0002)/*J 擬似的な太文字化処理が施されたもの */
											  /*E Style for executing pseudo boldface
												processing */
#define SCE_PVF_SUBSTYLE_PSEUDO_SLANT (0x0004)/*J 擬似的な斜体化処理が施されたもの */
											   /*E Style for executing pseudo italic
												 processing */

/*J フォント名文字列の最大長 */
/*E Font name maximum length */
#define SCE_PVF_FONTNAME_LENGTH (64)
/*J スタイル名文字列の最大長 */
/*E Style name maximum length */
#define SCE_PVF_STYLENAME_LENGTH (64)
/*J フォントファイル名の最大長 */
/*E Font filename maximum length */
#define SCE_PVF_FONTFILENAME_LENGTH (64)

/*J アクセスモード */
/*E Access mode */
typedef enum ScePvfDataAccessMode {
	SCE_PVF_FILEBASEDSTREAM=(0),
	SCE_PVF_MEMORYBASEDSTREAM=(1)
} ScePvfDataAccessMode;

/*J */
/*E */
#define SCE_PVF_MIN_EMBOLDEN_RATE (-20.0f)
#define SCE_PVF_MAX_EMBOLDEN_RATE (40.0f)

/*J */
/*E */
#define SCE_PVF_MIN_SKEW_VALUE (-30.0f)
#define SCE_PVF_MAX_SKEW_VALUE (30.0f)


// variable type definitions-------------------------------------------------
/*J 一般的な型 */
/*E Generic types  */
#if defined(__GNUC__)
__extension__ typedef unsigned long long ScePvf_t_u64; /*J 符号なし64ビット型 */
														/*E Unsigned 64-bit	 */
__extension__ typedef signed   long long ScePvf_t_s64; /*J 符号あり64ビット型 */
														/*E Signed 64-bit  */
#else	/* defined(__GNUC__) */
typedef unsigned long long ScePvf_t_u64;		/*J 符号なし64ビット型 */
												/*E Unsigned 64-bit	 */
typedef signed	 long long ScePvf_t_s64;		/*J 符号あり64ビット型 */
												/*E Signed 64-bit  */
#endif	/* defined(__GNUC__) */
typedef unsigned int  ScePvf_t_u32;			/*J 符号なし32ビット型 */
												/*E Unsigned 32-bit	 */
typedef signed	 int  ScePvf_t_s32;			/*J 符号あり32ビット型 */
												/*E Signed 32-bit  */
typedef unsigned short ScePvf_t_u16;			/*J 符号なし16ビット型 */
												/*E Unsigned 16-bit	 */
typedef signed	 short ScePvf_t_s16;			/*J 符号あり16ビット型 */
												/*E Signed 16-bit  */
typedef unsigned char  ScePvf_t_u8;			/*J 符号なし8ビット型 */
												/*E Unsigned 8-bit	*/
typedef signed	 char  ScePvf_t_s8;			/*J 符号あり8ビット型 */
												/*E Signed 8-bit  */
typedef float		   ScePvf_t_f32;			/*J 32ビット浮動小数点型 */
												/*E 32-bit floating-point  */
typedef double		   ScePvf_t_f64;			/*J 64ビット浮動小数点型 */
												/*E 64-bit floating-point  */
typedef ScePvf_t_u32  ScePvf_t_bool;			/*J 論理値型 */
												/*E Boolean */

/*J ハンドル的な型 */
/*E Handle types */
typedef void *ScePvf_t_libId; /*J scePvf のライブラリハンドル */
							   /*E scePvf library handle  */
typedef void *ScePvf_t_fontId; /*J scePvf のフォントハンドル */
								/*E scePvf font handle */
typedef void *ScePvf_t_pointer; /*J 汎用のポインタ型 */
								 /*E General pointer type  */
typedef void *ScePvf_t_handle; /*J 汎用の ID 型 (ScePvf_t_pointerと同義) */
								/*E General ID type
								  (same meaning as ScePvf_t_pointer)	 */

/*J 特殊な型 */
/*E Special types */
typedef ScePvf_t_s32 ScePvf_t_error; /*J エラー値用 */
									   /*E For error values	 */
typedef ScePvf_t_s32 ScePvf_t_int; /*J 整数型 */
									 /*E Integer  */
typedef ScePvf_t_u16 ScePvf_t_charCode; /*J 文字コード型 */
										  /*E Character code type */
typedef ScePvf_t_charCode *ScePvf_t_string; /*J 文字列型 */
											  /*E String type  */
typedef ScePvf_t_s32 ScePvf_t_fontIndex; /*J システムが決めたフォントの番号 */
										   /*E Font number determined by system	 */


// structure definitions-----------------------------------------------------
typedef struct ScePvf_t_irect { /*J 汎用の矩型 */
								 /*E General rectangle type */
	ScePvf_t_u16 width; /*J 16 ビット値 */
						 /*E 16-bit value */
	ScePvf_t_u16 height; /*J 16 ビット値 */
						  /*E 16-bit value */
} ScePvf_t_irect, ScePvfTIRect;

typedef struct ScePvf_t_rect { /*J 汎用の矩型 */
								/*E General rectangle type */
	ScePvf_t_u32 width; /*J 32 ビット値 */
						 /*E 32-bit value */
	ScePvf_t_u32 height; /*J 32 ビット値 */
						  /*E 32-bit value */
} ScePvf_t_rect, ScePvfTRect;

/*J ---- フォントキャッシュシステムとのインターフェース ---- */
/*E ---- Interface with font cache system ----	*/
/*J キャッシュをロックする関数 */ /*E Function for locking cache */
typedef ScePvf_t_s32 (*ScePvfFontCacheLockFunc)
(
 ScePvf_t_pointer cacheInstance
 );
/*J キャッシュをアンロックする関数 */ /*E Function for unlocking cache */
typedef ScePvf_t_s32 (*ScePvfFontChcheUnlockFunc)
(
 ScePvf_t_pointer cacheInstance
 );
/*J キャッシュに存在するかどうかを調べる関数 */ /*E Function for checking whether or not data exists in cache */
typedef ScePvf_t_pointer (*ScePvfFontChcheFindFunc)
(
 ScePvf_t_pointer chcheInstance,
 ScePvf_t_u32 hashValue,
 ScePvf_t_pointer key,
 ScePvf_t_bool *result
 );
/*J キーの値を書き出す関数 */ /*E Function for writing key value */
typedef ScePvf_t_s32 (*ScePvfFontChcheWriteKeyValueToCacheFunc)
(
 ScePvf_t_pointer cacheInstance,
 ScePvf_t_pointer chcheSlot,
 ScePvf_t_pointer key
 );
/*J キャッシュへ data を書き出す関数 */ /*E Function for writing data to cache */
typedef ScePvf_t_s32 (*ScePvfFontChcheWriteToCacheFunc)
(
 ScePvf_t_pointer cacheInstance,
 ScePvf_t_pointer cacheSlot,
 ScePvf_t_pointer data0,
 ScePvf_t_int size
 );
/*J キャッシュから data を読み込む関数 */ /*E Function for reading data from cache  */
typedef ScePvf_t_s32 (*ScePvfFontChcheReadFromCacheFunc)
(
 ScePvf_t_pointer cacheInstance,
 ScePvf_t_pointer cacheSlot,
 ScePvf_t_pointer data0
 );

typedef struct ScePvf_t_cacheSystemInterface {
	ScePvf_t_pointer *cacheInstance; /*J キャッシュインターフェースに渡す値 */ /*E Value passed to cache interface */
	ScePvfFontCacheLockFunc lockFunc; /*J キャッシュをロックする関数 *//*E Function for locking cache */
	ScePvfFontChcheUnlockFunc unlockFunc; /*J キャッシュをアンロックする関数 *//*E Function for unlocking cache */
	ScePvfFontChcheFindFunc	findFunc; /*J キャッシュに存在するかどうかを調べる関数 *//*E Function for checking whether or not data exists in cache */
	ScePvfFontChcheWriteKeyValueToCacheFunc writeKeyValueToCacheFunc;/*J キーの値を書き出す関数 */ /*E Function for writing key value */
	ScePvfFontChcheWriteToCacheFunc write0ToCacheFunc; /*J キャッシュへ data0 を書き出す関数 */ /*E Function for writing data0 to cache */
	ScePvfFontChcheWriteToCacheFunc write1ToCacheFunc; /*J キャッシュへ data1 を書き出す関数 */ /*E Function for writing data1 to cache */
	ScePvfFontChcheReadFromCacheFunc read0FromCacheFunc;/*J キャッシュから data0 を読み込む関数 *//*E Function for reading data0 from cache */
	ScePvfFontChcheReadFromCacheFunc read1FromCacheFunc;/*J キャッシュから data1 を読み込む関数 */ /*E Function for reading data1 from cache  */
} ScePvf_t_cacheSystemInterface, ScePvfTCacheSystemInterface;

/*J ---- ライブラリ使用開始時に指定する情報の構造体 ---- */
/*E ---- Structure of Information to Specify When Library Usage Starts ----	 */
 /*J メモリ確保関数 */ /*E Memory allocation function  */
typedef ScePvf_t_pointer (*ScePvfAllocFunc)
(
 ScePvf_t_pointer userData, /*J ユーザーデータへのポインタ */ /*E Pointer to user data  */
 ScePvf_t_u32 size /*J 要求サイズ */ /*E Request size */
 );
/*J メモリ再確保関数 */ /*E Memory re-allocation function  */
typedef ScePvf_t_pointer (*ScePvfReallocFunc)
(
 ScePvf_t_pointer userData, /*J ユーザーデータへのポインタ *//*E Pointer to user data  */
 ScePvf_t_pointer old_ptr, /*J 再確保する領域のポインタ */ /*E Pointer to area to re-alloc */
 ScePvf_t_u32 size /*J 要求サイズ *//*E Request size */
 );
/*J メモリ解放関数 */ /*E Memory free functions */
typedef void (*ScePvfFreeFunc)
(
 ScePvf_t_pointer userData, /*J ユーザーデータへのポインタ */ /*E Pointer to user data  */
 ScePvf_t_pointer ptr /*J 解放する領域のポインタ */ /*E Pointer to area to be freed */
 );
typedef struct ScePvf_t_initRec {
	/*J ●ユーザーデータ */
	/*E - User data */
	ScePvf_t_pointer userData; /*J ユーザーデータへのポインタ */
								/*E Pointer to user data  */

	/*J ●同時オープンするフォントの最大数 */
	/*E - Maximum number of fonts that are open simultaneously */
	ScePvf_t_u32 maxNumFonts; /*J 同時オープンするフォントの最大数 */
							   /*E Maximum number of fonts that are open
								 simultaneously	 */

	/*J ●キャッシュシステムのインスタンスハンドル */
	/*E - Cache system instance handle	*/
	ScePvf_t_cacheSystemInterface *cache; /*J キャッシュシステムのインスタンスハンドル */
										   /*E Cache system instance handle	 */
	ScePvf_t_pointer reserved; /*J 予約領域 (必ず 0) */ /*E Reserved area (must be 0) */

	/*J ●関数ポインタ */
	/*E - Function pointers	 */
	ScePvfAllocFunc allocFunc; /*J メモリ確保関数 */ /*E Memory allocation function  */
	ScePvfReallocFunc reallocFunc; /*J メモリ再確保関数 */ /*E Memory re-allocation function  */
	ScePvfFreeFunc freeFunc; /*J メモリ解放関数 */ /*E Memory free functions */
} ScePvf_t_initRec, ScePvfTInitRec;

/*J ---- フォント検索に使用するデータ型 ---- */
/*E ---- Data Types Used in Font Searches ----	*/
typedef struct ScePvf_t_fontStyleInfo {
	ScePvf_t_f32 weight; /*J ウェイト値 */
						  /*E Weight  */

	ScePvf_t_u16 familyCode; /*J ファミリーコード */
							  /*E Family code  */
	ScePvf_t_u16 style; /*J スタイル */
						 /*E Style */
	ScePvf_t_u16 subStyle; /*J サブスタイル */
							/*E Substyle */

	ScePvf_t_u16 languageCode; /*J 言語コード (0 の場合は無指定) */
								/*E Language code (0 means no language is specified) */
	ScePvf_t_u16 regionCode; /*J リージョンコード (0 の場合は無指定) */
							  /*E Region code (0 means no region is specified) */
	ScePvf_t_u16 countryCode; /*J 国コード (0 の場合は無指定) */
							   /*E Country code (0 means no country is specified) */
	ScePvf_t_u8 fontName [SCE_PVF_FONTNAME_LENGTH]; /*J フォント名の文字列 */
													  /*E Font name string */
	ScePvf_t_u8 styleName [SCE_PVF_STYLENAME_LENGTH]; /*J スタイル名の文字列 */
													  /*E Style name string */
	ScePvf_t_u8 fileName [SCE_PVF_FONTFILENAME_LENGTH]; /*J フォントのファイル名文字列 */
														  /*E Font filename string	*/
	ScePvf_t_u32 extraAttributes; /*J 追加属性情報 */
								   /*E Additional attribute information	 */
	ScePvf_t_u32 expireDate; /*J 有効期限 (0 の場合は無期限) */
							  /*E Expiration date (0 means none specified) */
} ScePvf_t_fontStyleInfo, ScePvfTFontStyleInfo;

/*J ---- ユーザのメモリ領域にグリフイメージをコピーする場合に使用するデータ型 ---- */
/*E ---- Data type used when glyph image is copied to user memory area ----	 */
typedef struct ScePvf_t_userImageBufferRec {
	ScePvf_t_u32 pixelFormat; /*J ピクセルフォーマット */
							   /*E Pixel format	 */
	ScePvf_t_s32 xPos64; /*J 書き込む基準点の X 位置 */
						  /*E X-position of reference point for writing	 */
	ScePvf_t_s32 yPos64; /*J 書き込む基準点の Y 位置 */
						  /*E Y-position of reference point for writing	 */
	ScePvf_t_irect rect; /*J バッファの横と縦のサイズ */
						  /*E Buffer horizontal and vertical size  */
	ScePvf_t_u16 bytesPerLine; /*J バッファの横1ラインのバイト数 */
								/*E Number of bytes per horizontal line of buffer  */
	ScePvf_t_u16 reserved; /*J パディング */
							/*E Padding */
	ScePvf_t_u8 *buffer; /*J バッファ領域へのポインタ */
						  /*E Pointer to buffer area  */
} ScePvf_t_userImageBufferRec, ScePvfTUserImageBufferRec;

/*J ---- 文字のグリフメトリクス情報 ---- */
/*E ---- Character glyph metrics ----  */
typedef struct ScePvf_t_iGlyphMetricsInfo { /*J 26.6 固定小数点表現の型 */
											 /*E 26.6 fixed-point representation */
	ScePvf_t_u32 width64; /*J 幅 */
						   /*E Width */
	ScePvf_t_u32 height64; /*J 高さ */
							/*E Height */
	ScePvf_t_s32 ascender64; /*J アセンダ
								注)個々の文字に関しては
								horizontalBearingY64 と同値 */
							  /*E Ascender
								Note) With respect to individual characters,
								this has the same value as horizontalBearingY64. */
	ScePvf_t_s32 descender64; /*J ディセンダ
								 注)個々の文字に関しては
								 horizontalBearingY64 - height64 */
							   /*E Descender
								 Note) With respect to individual characters,
								 this has the same value as
								 horizontalBearingY64 - height64.	 */
	ScePvf_t_s32 horizontalBearingX64; /*J 横文字レイアウト用 X の Bearing 値 */
										/*E Bearing value in X direction for
										  horizontal character layout */
	ScePvf_t_s32 horizontalBearingY64; /*J 横文字レイアウト用 Y の Bearing 値 */
										/*E Bearing value in Y direction for
										  horizontal character layout  */
	ScePvf_t_s32 verticalBearingX64; /*J 縦文字レイアウト用 X の Bearing 値 */
									  /*E Bearing value in X direction for
										vertical character layout  */
	ScePvf_t_s32 verticalBearingY64; /*J 縦文字レイアウト用 Y の Bearing 値 */
									  /*E Bearing value in Y direction for
										vertical character layout */
	ScePvf_t_s32 horizontalAdvance64; /*J 横文字レイアウト用 X の文字送り量 */
									   /*E Advance in X direction for horizontal
										 character layout */
	ScePvf_t_s32 verticalAdvance64; /*J 縦文字レイアウト用 X の文字送り量 */
									 /*E Advance in X direction for vertical
									   character layout  */
} ScePvf_t_iGlyphMetricsInfo, ScePvfTIGlyphMetricsInfo;

typedef struct ScePvf_t_charInfo { /*J 文字のヘッダから得られる情報 */
									/*E Information obtained from character vendor */
	ScePvf_t_u32 bitmapWidth; /*J ビットマップの幅 */
							   /*E Bitmap width */
	ScePvf_t_u32 bitmapHeight; /*J ビットマップの高さ */
								/*E Bitmap height */
	ScePvf_t_u32 bitmapPitch; /*J ビットマップのピッチ */
							   /*E Bitmap pitch */
	ScePvf_t_s32 bitmapLeft; /*J ビットマップ上でのベースライン原点の横の位置 */
							  /*E Horizontal position of baseline origin in bitmap */
	ScePvf_t_s32 bitmapTop; /*J ビットマップ上でのベースライン原点の縦の位置 */
							 /*E Vertical position of baseline origin in bitmap	 */
	ScePvf_t_iGlyphMetricsInfo glyphMetrics; /*J 文字のメトリクス情報
												(固定小数点表現) */
											  /*E Character metrics
												(fixed-point representation) */
	ScePvf_t_u8 reserved0 [2]; /*J 予約領域 */
								/*E Reserved area */
	ScePvf_t_u16 reserved1; /*J 予約領域 */
							 /*E Reserved area */
} ScePvf_t_charInfo, ScePvfTCharInfo;

typedef struct ScePvf_t_fGlyphMetricsInfo { /*J 浮動小数点表現の型 */
											 /*E Floating-point representation */
	ScePvf_t_f32 width; /*J 幅 */
						 /*E Width */
	ScePvf_t_f32 height; /*J 高さ */
						  /*E Height */
	ScePvf_t_f32 ascender; /*J アセンダ */
							/*E Ascender */
	ScePvf_t_f32 descender; /*J ディセンダ */
							 /*E Descender */
	ScePvf_t_f32 horizontalBearingX; /*J 横文字レイアウト用 X の Bearing 値 */
									  /*E Bearing value in X direction for
										horizontal character layout */
	ScePvf_t_f32 horizontalBearingY; /*J 横文字レイアウト用 Y の Bearing 値 */
									  /*E Bearing value in Y direction for
										horizontal character layout	*/
	ScePvf_t_f32 verticalBearingX; /*J 縦文字レイアウト用 X の Bearing 値 */
									/*E Bearing value in X direction for
									  vertical character layout	*/
	ScePvf_t_f32 verticalBearingY; /*J 縦文字レイアウト用 Y の Bearing 値 */
									/*E Bearing value in Y direction for
									  vertical character layout */
	ScePvf_t_f32 horizontalAdvance; /*J 横文字レイアウト用 X の文字送り量 */
									 /*E Advance in X direction for
									   horizontal character layout */
	ScePvf_t_f32 verticalAdvance; /*J 縦文字レイアウト用 Y の文字送り量 */
								   /*E Advance in Y direction for
									 vertical character layout */
} ScePvf_t_fGlyphMetricsInfo, ScePvfTFGlyphMetricsInfo;

/*J ---- フォントデータ全般に関する情報 ---- */
/*E ---- Information related to font data in general ----  */
typedef struct ScePvf_t_fontInfo {
	ScePvf_t_iGlyphMetricsInfo maxIGlyphMetrics; /*J 最大のメトリクス値
													(固定小数点表現)*/
												  /*E Maximum metrics value
													(fixed-point representation) */
	ScePvf_t_fGlyphMetricsInfo maxFGlyphMetrics; /*J 最大のメトリクス値
													(浮動小数点表現)*/
												  /*E Maximum metrics value
													(floating-point representation) */
	ScePvf_t_u32 numChars; /*J 収録されている文字種の数 */
							/*E Number of recorded character types	*/
	ScePvf_t_fontStyleInfo fontStyleInfo; /*J 実際にアクセスしているフォント
											 のスタイル情報*/
										   /*E Actual style information of font
											 that is accessed  */
	ScePvf_t_u8 reserved [4]; /*J パディング */
							   /*E Padding */
} ScePvf_t_fontInfo, ScePvfTFontInfo;

/*J ---- カーニング情報構造体 ---- */
/*E ---- Kerning information struncture ---- */
typedef struct ScePvf_t_iKerningInfo {
	ScePvf_t_s32 xOffset64; /*J */ /*E */
	ScePvf_t_s32 yOffset64; /*J */ /*E */
} ScePvf_t_iKerningInfo, ScePvfTIKerningInfo;

typedef struct ScePvf_t_fKerningInfo {
	ScePvf_t_f32 xOffset; /*J */ /*E */
	ScePvf_t_f32 yOffset; /*J */ /*E */
} ScePvf_t_fKerningInfo, ScePvfTFKerningInfo;

typedef struct ScePvf_t_kerningInfo {
	ScePvf_t_iKerningInfo iKerningInfo;
	ScePvf_t_fKerningInfo fKerningInfo;
} ScePvf_t_kerningInfo, ScePvfTKerningInfo;

/*J ---- フォントキャッシュシステムにキーとして使用させる構造体 ---- */
/*E ---- Structure to be used as key in font cache system ----	*/
typedef struct ScePvfCacheKey { /*J 単なる4つのキーの集合体 */
								 /*E Simple aggregation of four keys */
	int keyValue0; /*J 比較キー 0 */
				   /*E Comparison key 0 */
	int keyValue1; /*J 比較キー 1 */
				   /*E Comparison key 1 */
	float keyValue2; /*J 比較キー 2 */
				   /*E Comparison key 2 */
	float keyValue3; /*J 比較キー 3 */
				   /*E Comparison key 3 */
	float keyValue4; /*J 比較キー 4*/
				   /*E Comparison key 4 */
	float keyValue5; /*J 比較キー 5*/
				   /*E Comparison key 5 */
	float keyValue6; /*J 比較キー 6 */
				     /*E Comparison key 6 */
	float keyValue7; /*J 比較キー 7 */
				     /*E Comparison key 7 */
	float keyValue8; /*J 比較キー 8 */
				     /*E Comparison key 8*/
} ScePvfCacheKey;


// prototype definitions(public functions)-----------------------------------
/*J ライブラリの初期化 */
/*E Initialize library */
extern ScePvf_t_libId scePvfNewLib
(
 ScePvf_t_initRec *initParam, /*J 使用するコールバック関数のポインタ等 */
                              /*E Pointer to callback function to be used, etc. */
 ScePvf_t_error *errorCode /*J エラーコード */
                           /*E Error code */
 );
/*J ライブラリの終了 */
/*E Terminate library */
extern ScePvf_t_error scePvfDoneLib
(
 ScePvf_t_libId libID /*J ライブラリ ID */
				 /*E Library ID	 */
 );
/*J EM スクエア値をセットする */
/*E Set EM square value */
extern ScePvf_t_error scePvfSetEM
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_f32 emValue /*J 浮動小数点のポイント値 */
				/*E Floating-point point value */
 );
/*J システム解像度の値のセット */
/*E Set system resolution values */
extern ScePvf_t_error scePvfSetResolution
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_f32 hResolution, /*J 横解像度(浮動小数点値) */
				/*E Horizontal resolution value (floating-point value) */
 ScePvf_t_f32 vResolution /*J 縦解像度(浮動小数点値) */
			   /*E Vertical resolution value (floating-point value) */
 );
/*J ---- 以下はフォント検索に関する処理を行う関数 ---- */
/*E ---- The following functions are used for finding fonts ----  */
/*J 使用可能フォントリストの数の取得 */
/*E Get number of available font lists */
extern ScePvf_t_int scePvfGetNumFontList
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_error *errorCode /*J エラーコード */
				   /*E Error code */
 );
/*J 使用可能フォントリストの取得 */
/*E Get available font list */
extern ScePvf_t_error scePvfGetFontList
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_fontStyleInfo *fontStyleInfo, /*J 検索対象のフォントスタイル */
							/*E Find target font style */
 ScePvf_t_int arraySize/*J 数の上限値 */
			   /*E Upper bound of number to find */
 );
/*J 最適なフォントの検索 */
/*E Find optimum font */
extern ScePvf_t_fontIndex scePvfFindOptimumFont
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_fontStyleInfo *fontStyleInfo, /*J リクエストするフォントのスタイル */
							/*E Style of requested font */
 ScePvf_t_error *errorCode /*J エラーコード */
				   /*E Error code */
 );
extern ScePvf_t_fontIndex scePvfFindFont
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_fontStyleInfo *fontStyleInfo, /*J リクエストするフォントのスタイル */
							/*E Style of requested font */
 ScePvf_t_error *errorCode /*J エラーコード */
				   /*E Error code */
 );
/*J */
/*E */
extern ScePvf_t_error scePvfGetFontInfoByIndexNumber
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_fontStyleInfo *fontStyleInfo, /*J 検索対象のフォントスタイル */
							/*E Find target font style */
 ScePvf_t_fontIndex fontIndex /*J 調べる対象のフォントインデックス値 */
					 /*E Target font index value to find */
 );

/*J ---- 以下は特定のフォントに関する処理を行う関数 ---- */
/*E ---- The following are font-specific functions ----	 */
/*J ユーザー指定フォントファイルのオープン(メモリベースドストリーム、ファイルベースドストリーム) */
/*E Open user-specified font file (memory-based stream, file-based stream)	*/
/*J フォントのオープン(メモリベースドストリーム、ファイルベースドストリーム) */
/*E Open font (memory-based stream, file-based stream) */
extern ScePvf_t_fontId scePvfOpen
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_fontIndex fontIndex, /*J フォント指定インデックス値 (0 はシステムデフォルトフォント) */
					  /*E Font-specified index (0 means system default font) */
 ScePvf_t_u32 mode, /*J モード値 (ScePvfDataAccessMode) */
				/*E Mode (ScePvfDataAccessMode)  */
 ScePvf_t_error *errorCode /*J エラー値 */
				   /*E Error value */
 );
/*J ユーザー指定フォントファイルのオープン(メモリベースドストリーム、ファイルベースドストリーム) */
/*E Open user-specified font file (memory-based stream, file-based stream)	*/
extern ScePvf_t_fontId scePvfOpenUserFile
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_pointer filename, /*J ファイル名(フルパス) */
					/*E Filename (full path) */
 ScePvf_t_u32 mode, /*J モード値 (ScePvfDataAccessMode) */
				/*E Mode (ScePvfDataAccessMode)  */
 ScePvf_t_error *errorCode /*J エラー値 */
				   /*E Error value */
 );
/*J サブフォント番号指定可能なユーザー指定フォントファイルのオープン(メモリベースドストリーム、ファイルベースドストリーム) */
/*E Open user-specified font file (memory-based stream, file-based stream) with sub-font index. */
extern ScePvf_t_fontId scePvfOpenUserFileWithSubfontIndex
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_pointer filename, /*J ファイル名(フルパス) */
					/*E Filename (full path) */
 ScePvf_t_u32 mode, /*J モード値 (ScePvfDataAccessMode) */
				/*E Mode (ScePvfDataAccessMode)  */
 ScePvf_t_u32 subFontIndex, /*J サブフォントインデックス番号*/ /*E Subfont Index */
 ScePvf_t_error *errorCode /*J エラー値 */
				   /*E Error value */
 );
/*J ユーザー指定メモリアドレスにあるフォントのオープン(メモリベースドストリーム) */
/*E Open font at user-specified memory address (memory-based stream) */
extern ScePvf_t_fontId scePvfOpenUserMemory
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_pointer addr, /*J フォントデータのアドレス */
					/*E Font data address */
 ScePvf_t_u32 size, /*J フォントデータのサイズ */
				/*E Font data size */
 ScePvf_t_error *errorCode /*J エラー値 */
				   /*E Error value */
 );
/*J ユーザー指定メモリアドレスにあるフォントのオープン(メモリベースドストリーム) */
/*E Open font at user-specified memory address (memory-based stream) */
extern ScePvf_t_fontId scePvfOpenUserMemoryWithSubfontIndex
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_pointer addr, /*J フォントデータのアドレス */
					/*E Font data address */
 ScePvf_t_u32 size, /*J フォントデータのサイズ */
				/*E Font data size */
 ScePvf_t_u32 subFontIndex, /*J サブフォントインデックス番号*/ /*E Subfont Index */
 ScePvf_t_error *errorCode /*J エラー値 */
				   /*E Error value */
 );
/*J */
/*E */
extern ScePvf_t_fontId scePvfOpenDefaultJapaneseFontOnSharedMemory
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_error *errorCode /*J エラー値 */
				   /*E Error value */
 );
/*J */
/*E */
extern ScePvf_t_fontId scePvfOpenDefaultLatinFontOnSharedMemory
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_error *errorCode /*J エラー値 */
				   /*E Error value */
 );
/*J フォントのクローズ */
/*E Close font */
extern ScePvf_t_error scePvfClose
(
 ScePvf_t_fontId fontID /*J フォントハンドル */
				  /*E Font handle */
 );
/*J 内部のローカルキャッシュ(fontcache.c 等の外付けのキャッシュではない)にある情報をクリアする */
/*E Clear libpvf internal local cache (not fontcache.c chache) */
extern ScePvf_t_error scePvfFlush
(
 ScePvf_t_fontId fontID /*J ライブラリ ID */
				 /*E Library ID	 */
 );
/*J 文字サイズ値のセット */
/*E Set char point size values */
extern ScePvf_t_error scePvfSetCharSize
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				  /*E Font handle */
 ScePvf_t_f32 hSize,
 ScePvf_t_f32 vSize
 );
/*J 太字化パラメータのセット */
/*E Set embolden strength value */
extern ScePvf_t_error scePvfSetEmboldenRate
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				  /*E Font handle */
 ScePvf_t_f32 emboldenRate
 );
/*J 斜体化パラメータのセット */
/*E Set oblique advance value */
extern ScePvf_t_error scePvfSetSkewValue
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				  /*E Font handle */
 ScePvf_t_f32 angleX,
 ScePvf_t_f32 angleY
 );
/*J グリフの有無を調べる */
/*E Check whether or not glyph exists*/
extern ScePvf_t_bool scePvfIsElement
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				  /*E Font handle */
 ScePvf_t_charCode charCode /*J 文字コード */ /*E Character code */
 );
/*J グリフの有無を調べる(縦組みレイアウト用) */
/*E Check whether or not glyph exists(for vertical layout) */
extern ScePvf_t_bool scePvfIsVertElement
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				  /*E Font handle */
 ScePvf_t_charCode charCode /*J 文字コード */ /*E Character code */
 );
/*J フォント情報の取得 */
/*E Get font information */
extern ScePvf_t_error scePvfGetFontInfo
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				   /*E Font handle */
 ScePvf_t_fontInfo *fontInfo /*J フォント情報 */
					  /*E Font information */
 );
/*J グリフ情報の取得 */
/*E Get glyph information */
extern ScePvf_t_error scePvfGetCharInfo
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				   /*E Font handle */
 ScePvf_t_charCode charCode, /*J 文字コード */ /*E Character code */
 ScePvf_t_charInfo *charInfo /*J 文字情報 */
					  /*E Character information */
 );
/*J グリフ情報の取得(縦組みレイアウト用) */
/*E Get glyph information(for vertical layout) */
extern ScePvf_t_error scePvfGetVertCharInfo
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				   /*E Font handle */
 ScePvf_t_charCode charCode, /*J 文字コード */ /*E Character code */
 ScePvf_t_charInfo *charInfo /*J 文字情報 */
					  /*E Character information */
 );
/*J カーニング情報の取得 */
/*E Get kerning information */
extern ScePvf_t_error scePvfGetKerningInfo
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */ /*E Font handle */
 ScePvf_t_charCode leftCharCode, /*J 左文字コード */ /*E Left character code */
 ScePvf_t_charCode rightCharCode, /*J 右文字コード */ /*E Right character code */
 ScePvf_t_kerningInfo *pKerningInfo /*J フォント情報 */ /*E Kerning information */
 );
/*J 文字矩形の取得 */
/*E Get character rectangle */
extern ScePvf_t_error scePvfGetCharImageRect
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				   /*E Font handle */
 ScePvf_t_charCode charCode, /*J 文字コード */ /*E Character code */
 ScePvf_t_irect *rect /*J 文字矩形 */
				   /*E Character rectangle */
 );
/*J 文字矩形の取得(縦組みレイアウト用) */
/*E Get character rectangle(for vertical layout) */
extern ScePvf_t_error scePvfGetVertCharImageRect
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				   /*E Font handle */
 ScePvf_t_charCode charCode, /*J 文字コード */ /*E Character code */
 ScePvf_t_irect *rect /*J 文字矩形 */
				   /*E Character rectangle */
 );
/*J グリフイメージ(文字)の取得 */
/*E Get glyph image (character) */
extern ScePvf_t_error scePvfGetCharGlyphImage
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				   /*E Font handle */
 ScePvf_t_charCode charCode, /*J 文字コード */ /*E Character code */
 ScePvf_t_userImageBufferRec *imageBuffer /*J 出力先 */
								/*E Output destination */
 );
/*J グリフイメージ(文字)の取得(縦組みレイアウト用) */
/*E Get glyph image (character) (for vertical layout) */
extern ScePvf_t_error scePvfGetVertCharGlyphImage
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				   /*E Font handle */
 ScePvf_t_charCode charCode, /*J 文字コード */ /*E Character code */
 ScePvf_t_userImageBufferRec *imageBuffer /*J 出力先 */
								/*E Output destination */
 );
/*J グリフイメージ(文字)の取得(矩形クリップ処理付き) */
/*E Get glyph image (character) (with rectangle clipping) */
extern ScePvf_t_error scePvfGetCharGlyphImage_Clip
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				   /*E Font handle */
 ScePvf_t_charCode charCode, /*J 文字コード */ /*E Character code */
 ScePvf_t_userImageBufferRec *imageBuffer, /*J 出力先 */
								 /*E Output destination */
 ScePvf_t_s32 clipX, /*J クリップ矩形の X 位置 */
				/*E X position of clipping rectangle */
 ScePvf_t_s32 clipY, /*J クリップ矩形の Y 位置 */
				/*E Y position of clipping rectangle  */
 ScePvf_t_u32 clipWidth, /*J クリップ矩形の幅 */
				/*E Clipping rectangle width */
 ScePvf_t_u32 clipHeight /*J クリップ矩形の高さ */
			   /*E Clipping rectangle height */
 );
#define scePvfGetCharGlyphImageClip scePvfGetCharGlyphImage_Clip
/*J グリフイメージ(文字)の取得(矩形クリップ処理付き) (縦組みレイアウト用) */
/*E Get glyph image (character) (with rectangle clipping) (for vertical layout) */
extern ScePvf_t_error scePvfGetVertCharGlyphImage_Clip
(
 ScePvf_t_fontId fontID, /*J フォントハンドル */
				   /*E Font handle */
 ScePvf_t_charCode charCode, /*J 文字コード */ /*E Character code */
 ScePvf_t_userImageBufferRec *imageBuffer, /*J 出力先 */
								 /*E Output destination */
 ScePvf_t_s32 clipX, /*J クリップ矩形の X 位置 */
				/*E X position of clipping rectangle */
 ScePvf_t_s32 clipY, /*J クリップ矩形の Y 位置 */
				/*E Y position of clipping rectangle  */
 ScePvf_t_u32 clipWidth, /*J クリップ矩形の幅 */
				/*E Clipping rectangle width */
 ScePvf_t_u32 clipHeight /*J クリップ矩形の高さ */
			   /*E Clipping rectangle height */
 );
#define scePvfGetVertCharGlyphImageClip scePvfGetVertCharGlyphImage_Clip
/*J ピクセルからポイントに単位を変換する(横方向) */
/*E Convert units from pixels to points (horizontal direction) */
extern ScePvf_t_f32 scePvfPixelToPointH
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_f32 pixel, /*J 浮動小数点のピクセル値 */
				/*E Floating-point pixel value */
 ScePvf_t_error *errorCode /*J エラーコード */
				   /*E Error code */
 );
/*J ピクセルからポイントに単位を変換する(縦方向) */
/*E Convert units from pixels to points (vertical direction) */
extern ScePvf_t_f32 scePvfPixelToPointV
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_f32 pixel, /*J 浮動小数点のピクセル値 */
				/*E Floating-point pixel value */
 ScePvf_t_error *errorCode /*J エラーコード */
				   /*E Error code */
 );
/*J ポイントからピクセルに単位を変換する(横方向) */
/*E Convert units from points to pixels (horizontal direction) */
extern ScePvf_t_f32 scePvfPointToPixelH
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_f32 point, /*J 浮動小数点のポイント値 */
				/*E Floating-point point value */
 ScePvf_t_error *errorCode /*J エラーコード */
				   /*E Error code */
 );
/*J ポイントからピクセルに単位を変換する(縦方向) */
/*E Convert units from points to pixels (vertical direction)  */
extern ScePvf_t_f32 scePvfPointToPixelV
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_f32 point, /*J 浮動小数点のポイント値 */
				/*E Floating-point point value */
 ScePvf_t_error *errorCode /*J エラーコード */
				   /*E Error code */
 );
/*J 未定義文字コードを代替する文字コードのセット */
/*E Set undefined character code to alternate character code */
extern ScePvf_t_error scePvfSetAltCharacterCode
(
 ScePvf_t_libId libID, /*J ライブラリ ID */
				  /*E Library ID  */
 ScePvf_t_charCode charCode /*J 代替文字コード */
					/*E Alternate character code */
 );


// external variables--------------------------------------------------------
// nothing


#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif	/* defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus) */


#endif //_SCE_LIBPVF_H_
/* DON'T ADD ANYTHING AFTER THIS #endif */

/*end of libpvf.h*/


/*
 * Local Variables:
 * tab-width:4
 * coding:utf-8
 * End:
 */
